#ifndef _ACCESS_CONTROL_H
#define _ACCESS_CONTROL_H

/*
 * User-space header for /dev/access_control.
 * Message protocol MUST match the M4 firmware (App/rpmsg_protocol.h)
 * and the kernel driver (linux/driver/access_control.c).
 */

#include <stdint.h>
#include <sys/ioctl.h>

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
	uint32_t msg_id;
	uint32_t value;
};

#define AC_IOC_MAGIC 'A'
#define AC_IOCTL_GET_DOOR_STATE _IOR(AC_IOC_MAGIC, 1, uint32_t)
#define AC_IOCTL_UNLOCK         _IO(AC_IOC_MAGIC, 2)
#define AC_IOCTL_SEND_MSG       _IOW(AC_IOC_MAGIC, 3, struct ac_msg)

#endif /* _ACCESS_CONTROL_H */
