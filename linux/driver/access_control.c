// SPDX-License-Identifier: GPL-2.0
/*
 * access_control.c - STM32MP157 Access Control RPMsg driver
 *
 * Provides /dev/access_control as an RPMsg endpoint to the Cortex-M4
 * firmware (OpenAMP). The M4 handles presence detection (LD2412 radar)
 * and door lock (SG90 servo); this driver relays messages between the
 * A7 user space and the M4.
 *
 * User space interface:
 *   read()  : read a received rpmsg message (struct ac_msg), blocking
 *   write() : send a rpmsg message (struct ac_msg) to the M4
 *   poll()  : wait for incoming messages
 *   ioctl() : AC_IOCTL_GET_DOOR_STATE / AC_IOCTL_UNLOCK / AC_IOCTL_SEND_MSG
 *
 * Message protocol MUST match the M4 side (m4_fw: App/rpmsg_protocol.h).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/kref.h>
#include <linux/mutex.h>

#define DEVICE_NAME   "access_control"
#define RPMSG_CHANNEL "rpmsg-access-ctrl"
#define AC_RX_QUEUE_SIZE 16

/* --- Message protocol (must match M4 rpmsg_protocol.h) --- */
enum {
	AC_MSG_PING            = 0,
	AC_MSG_PONG            = 1,
	AC_MSG_PERSON_DETECTED = 2,
	AC_MSG_AUTH_SUCCESS    = 3,
	AC_MSG_AUTH_FAILED     = 4,
	AC_MSG_UNLOCK          = 5,
	AC_MSG_UNLOCK_DONE     = 6,
	AC_MSG_DOOR_STATE      = 7,
};

#define AC_DOOR_STATE_LOCKED    0u
#define AC_DOOR_STATE_UNLOCKED  1u

struct ac_msg {
	u32 msg_id;
	u32 value;
};

/* --- ioctl interface --- */
#define AC_IOC_MAGIC 'A'
#define AC_IOCTL_GET_DOOR_STATE _IOR(AC_IOC_MAGIC, 1, u32)
#define AC_IOCTL_UNLOCK         _IO(AC_IOC_MAGIC, 2)
#define AC_IOCTL_SEND_MSG       _IOW(AC_IOC_MAGIC, 3, struct ac_msg)

struct ac_dev {
	struct kref refs;
	struct mutex tx_lock;
	bool offline; /* protected by rx_lock; READ_ONCE in wait condition */
	struct rpmsg_endpoint *ept;
	struct miscdevice miscdev;
	wait_queue_head_t rx_wait;
	spinlock_t rx_lock;
	struct ac_msg rx_buf[AC_RX_QUEUE_SIZE];
	unsigned int rx_head;   /* next write position */
	unsigned int rx_tail;   /* next read position */
	u32 door_state;
};

/* Serialize drvdata lookup/use against removal, including callbacks already
 * dispatched by virtio before the core destroys its default endpoint. */
static DEFINE_MUTEX(ac_callback_lock);

static void ac_free(struct kref *ref)
{
	kfree(container_of(ref, struct ac_dev, refs));
}

static int ac_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len,
		       void *priv, u32 src)
{
	struct ac_dev *ac;
	struct ac_msg msg;

	if (len < (int)sizeof(msg))
		return 0;
	memcpy(&msg, data, sizeof(msg));
	mutex_lock(&ac_callback_lock);
	ac = dev_get_drvdata(&rpdev->dev);
	if (!ac) {
		mutex_unlock(&ac_callback_lock);
		return -ENODEV;
	}

	spin_lock(&ac->rx_lock);
	if (ac->offline) {
		spin_unlock(&ac->rx_lock);
		mutex_unlock(&ac_callback_lock);
		return -ENODEV;
	}

	if (msg.msg_id == AC_MSG_DOOR_STATE)
		ac->door_state = msg.value;

	/* enqueue into ring buffer (drop if full) */
	if ((ac->rx_head + 1) % AC_RX_QUEUE_SIZE != ac->rx_tail) {
		ac->rx_buf[ac->rx_head] = msg;
		ac->rx_head = (ac->rx_head + 1) % AC_RX_QUEUE_SIZE;
	}

	spin_unlock(&ac->rx_lock);

	wake_up_interruptible(&ac->rx_wait);
	mutex_unlock(&ac_callback_lock);
	return 0;
}

static int ac_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = filp->private_data;
	struct ac_dev *ac = container_of(misc, struct ac_dev, miscdev);

	/* BSP misc_open holds misc_mtx until this returns; deregister waits. */
	spin_lock(&ac->rx_lock);
	if (ac->offline) {
		spin_unlock(&ac->rx_lock);
		return -ENODEV;
	}
	kref_get(&ac->refs);
	filp->private_data = ac;
	spin_unlock(&ac->rx_lock);
	return 0;
}

static int ac_release(struct inode *inode, struct file *filp)
{
	struct ac_dev *ac = filp->private_data;
	kref_put(&ac->refs, ac_free);
	return 0;
}

static int ac_send(struct ac_dev *ac, struct ac_msg *msg)
{
	int ret;
	if (mutex_lock_interruptible(&ac->tx_lock))
		return -ERESTARTSYS;
	/* Keep existing blocking send semantics; do not change protocol/retries. */
	ret = ac->ept ? rpmsg_send(ac->ept, msg, sizeof(*msg)) : -ENODEV;
	mutex_unlock(&ac->tx_lock);
	return ret;
}

static ssize_t ac_read(struct file *filp, char __user *buf, size_t len,
		       loff_t *off)
{
	struct ac_dev *ac = filp->private_data;
	struct ac_msg msg;

	if (len < sizeof(msg))
		return -EINVAL;

	spin_lock(&ac->rx_lock);
	while (!ac->offline && ac->rx_head == ac->rx_tail) {
		spin_unlock(&ac->rx_lock);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(ac->rx_wait,
					     READ_ONCE(ac->offline) ||
					     READ_ONCE(ac->rx_head) != READ_ONCE(ac->rx_tail)))
			return -ERESTARTSYS;
		spin_lock(&ac->rx_lock);
	}
	if (ac->offline) {
		spin_unlock(&ac->rx_lock);
		return -ENODEV;
	}

	msg = ac->rx_buf[ac->rx_tail];
	ac->rx_tail = (ac->rx_tail + 1) % AC_RX_QUEUE_SIZE;
	spin_unlock(&ac->rx_lock);

	if (copy_to_user(buf, &msg, sizeof(msg)))
		return -EFAULT;

	return sizeof(msg);
}

static ssize_t ac_write(struct file *filp, const char __user *buf, size_t len,
			loff_t *off)
{
	struct ac_dev *ac = filp->private_data;
	struct ac_msg msg;
	int ret;

	if (len < sizeof(msg))
		return -EINVAL;
	if (copy_from_user(&msg, buf, sizeof(msg)))
		return -EFAULT;
	ret = ac_send(ac, &msg);
	if (ret)
		return ret;

	return sizeof(msg);
}

static __poll_t ac_poll(struct file *filp, poll_table *wait)
{
	struct ac_dev *ac = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &ac->rx_wait, wait);

	spin_lock(&ac->rx_lock);
	if (ac->offline)
		mask |= EPOLLHUP | EPOLLERR;
	else if (ac->rx_head != ac->rx_tail)
		mask |= EPOLLIN | EPOLLRDNORM;
	spin_unlock(&ac->rx_lock);

	return mask;
}

static long ac_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ac_dev *ac = filp->private_data;
	struct ac_msg msg;
	u32 state;

	switch (cmd) {
	case AC_IOCTL_GET_DOOR_STATE:
		spin_lock(&ac->rx_lock);
		if (ac->offline) {
			spin_unlock(&ac->rx_lock);
			return -ENODEV;
		}
		state = ac->door_state;
		spin_unlock(&ac->rx_lock);
		if (copy_to_user((void __user *)arg, &state, sizeof(state)))
			return -EFAULT;
		return 0;

	case AC_IOCTL_UNLOCK:
		msg.msg_id = AC_MSG_UNLOCK;
		msg.value = 0;
		return ac_send(ac, &msg);

	case AC_IOCTL_SEND_MSG:
		if (copy_from_user(&msg, (void __user *)arg, sizeof(msg)))
			return -EFAULT;
		return ac_send(ac, &msg);

	default:
		return -ENOTTY;
	}
}

static const struct file_operations ac_fops = {
	.owner          = THIS_MODULE,
	.open           = ac_open,
	.release        = ac_release,
	.read           = ac_read,
	.write          = ac_write,
	.poll           = ac_poll,
	.unlocked_ioctl = ac_ioctl,
};

static int ac_probe(struct rpmsg_device *rpdev)
{
	struct ac_dev *ac;
	int ret;

	ac = kzalloc(sizeof(*ac), GFP_KERNEL);
	if (!ac)
		return -ENOMEM;

	/* The rpmsg core created this endpoint from the M4 name-service
	 * announcement, including the discovered remote destination address. */
	ac->ept = rpdev->ept;
	if (!ac->ept) {
		dev_err(&rpdev->dev, "default rpmsg endpoint is unavailable\n");
		kfree(ac);
		return -ENODEV;
	}

	spin_lock_init(&ac->rx_lock);
	kref_init(&ac->refs); /* probe reference; released after deregistration */
	mutex_init(&ac->tx_lock);
	init_waitqueue_head(&ac->rx_wait);
	ac->rx_head = 0;
	ac->rx_tail = 0;
	ac->door_state = AC_DOOR_STATE_LOCKED;

	ac->miscdev.minor = MISC_DYNAMIC_MINOR;
	ac->miscdev.name = DEVICE_NAME;
	ac->miscdev.fops = &ac_fops;
	mutex_lock(&ac_callback_lock);
	dev_set_drvdata(&rpdev->dev, ac);
	mutex_unlock(&ac_callback_lock);

	ret = misc_register(&ac->miscdev);
	if (ret) {
		dev_err(&rpdev->dev, "misc_register failed %d\n", ret);
		mutex_lock(&ac_callback_lock);
		dev_set_drvdata(&rpdev->dev, NULL);
		mutex_unlock(&ac_callback_lock);
		kref_put(&ac->refs, ac_free);
		return ret;
	}

	dev_info(&rpdev->dev, "access_control lifetime-v2 probed (endpoint %s)\n",
		 RPMSG_CHANNEL);
	return 0;
}

static void ac_remove(struct rpmsg_device *rpdev)
{
	struct ac_dev *ac = dev_get_drvdata(&rpdev->dev);

	if (!ac)
		return;

	/* Core destroys ept only AFTER remove returns. Drain senders first. */
	mutex_lock(&ac->tx_lock);
	ac->ept = NULL;
	spin_lock(&ac->rx_lock);
	WRITE_ONCE(ac->offline, true);
	ac->rx_head = ac->rx_tail; /* do not expose stale authorization events */
	spin_unlock(&ac->rx_lock);
	mutex_unlock(&ac->tx_lock);
	wake_up_interruptible(&ac->rx_wait);
	/* Do not hold callback/tx locks across misc_deregister (misc_mtx). */
	misc_deregister(&ac->miscdev);
	mutex_lock(&ac_callback_lock);
	dev_set_drvdata(&rpdev->dev, NULL);
	mutex_unlock(&ac_callback_lock);
	kref_put(&ac->refs, ac_free);
}

static const struct rpmsg_device_id ac_id_table[] = {
	{ .name = RPMSG_CHANNEL },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, ac_id_table);

static struct rpmsg_driver ac_driver = {
	.drv = {
		.name = DEVICE_NAME,
	},
	.id_table = ac_id_table,
	.probe = ac_probe,
	.callback = ac_rpmsg_cb,
	.remove = ac_remove,
};

static int __init ac_init(void)
{
	return register_rpmsg_driver(&ac_driver);
}

static void __exit ac_exit(void)
{
	unregister_rpmsg_driver(&ac_driver);
}

module_init(ac_init);
module_exit(ac_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("2.0");
MODULE_DESCRIPTION("STM32MP157 Access Control RPMsg driver");
MODULE_AUTHOR("Access Control Project");
