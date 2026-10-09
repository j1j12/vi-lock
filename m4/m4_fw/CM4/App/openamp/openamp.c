/**
  ******************************************************************************
  * @file   openamp.c
  * @author  MCD Application Team
  * @brief  Code for openamp applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "openamp.h"
#include "rsc_table.h"
#include "metal/sys.h"
#include "metal/device.h"
#include "openamp_log.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */


/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Define */

/* USER CODE END Define */

#define SHM_DEVICE_NAME         "STM32_SHM"

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */


/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

static struct metal_io_region *shm_io;
static struct metal_io_region *rsc_io;
static struct shared_resource_table *rsc_table;
static struct rpmsg_virtio_shm_pool shpool;
static struct rpmsg_virtio_device rvdev;


static metal_phys_addr_t shm_physmap;
static metal_phys_addr_t rsc_physmap;

struct metal_device shm_device = {
  .name = SHM_DEVICE_NAME,
  .num_regions = 2,
  .regions = {
      {.virt = NULL}, /* shared memory */
      {.virt = NULL}, /* rsc_table memory */
  },
  .node = { NULL },
  .irq_num = 0,
  .irq_info = NULL
};

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

static int OPENAMP_shmem_init(int RPMsgRole)
{
  int status = 0;
  struct metal_device *device = NULL;
  struct metal_init_params metal_params = METAL_INIT_DEFAULTS;
  void* rsc_tab_addr = NULL;
  int rsc_size = 0;


  /* USER CODE BEGIN PRE_LIB_METAL_INIT */

  /* USER CODE END  PRE_LIB_METAL_INIT */
  metal_init(&metal_params);

  status = metal_register_generic_device(&shm_device);
  if (status != 0) {
    return status;
  }

  status = metal_device_open("generic", SHM_DEVICE_NAME, &device);
  if (status != 0) {
    return status;
  }

  shm_physmap = SHM_START_ADDRESS;
  metal_io_init(&device->regions[0], (void *)SHM_START_ADDRESS, &shm_physmap,
                SHM_SIZE, (unsigned int)-1, 0, NULL);

  /* USER CODE BEGIN PRE_SHM_IO_INIT */

  /* USER CODE END PRE_SHM_IO_INIT */
  shm_io = metal_device_io_region(device, 0);
  if (shm_io == NULL) {
    return -1;
  }

  /* USER CODE BEGIN POST_SHM_IO_INIT */

  /* USER CODE END POST_SHM_IO_INIT */

  /* Initialize resources table variables */
  resource_table_init(RPMsgRole, &rsc_tab_addr, &rsc_size);
  rsc_table = (struct shared_resource_table *)rsc_tab_addr;
  if (!rsc_table)
  {
    return -1;
  }

  /* USER CODE BEGIN POST_RSC_TABLE_INIT */

  /* USER CODE END  POST_RSC_TABLE_INIT */

  rsc_physmap = (metal_phys_addr_t)rsc_tab_addr;
  metal_io_init(&device->regions[1], rsc_table,
               &rsc_physmap, rsc_size, -1U, 0, NULL);

  /* USER CODE BEGIN POST_METAL_IO_INIT */

  /* USER CODE END  POST_METAL_IO_INIT */
  rsc_io = metal_device_io_region(device, 1);
  if (rsc_io == NULL) {
    return -1;
  }

  /* USER CODE BEGIN POST_RSC_IO_INIT */

  /* USER CODE END  POST_RSC_IO_INIT */
  return 0;
}

int MX_OPENAMP_Init(int RPMsgRole, rpmsg_ns_bind_cb ns_bind_cb)
{
  struct fw_rsc_vdev_vring *vring_rsc = NULL;
  struct virtio_device *vdev = NULL;
  int status = 0;


  /* USER CODE BEGIN MAILBOX_Init */

  /* USER CODE END MAIL_BOX_Init */

  MAILBOX_Init();
  log_info("TRACE: MAILBOX_Init complete\n");

  /* Libmetal Initilalization */
  status = OPENAMP_shmem_init(RPMsgRole);
  if(status)
  {
    return status;
  }
  log_info("TRACE: OPENAMP_shmem_init complete\n");

  /* USER CODE BEGIN  PRE_VIRTIO_INIT */

  /* USER CODE END PRE_VIRTIO_INIT */
  vdev = rproc_virtio_create_vdev(RPMsgRole, VDEV_ID, &rsc_table->vdev,
                                  rsc_io, NULL, MAILBOX_Notify, NULL);
  if (vdev == NULL)
  {
    return -1;
  }

  log_info("TRACE: create_vdev complete status=%lu\n",
           (unsigned long)rsc_table->vdev.status);
  log_info("TRACE: wait_remote_ready begin\n");
  rproc_virtio_wait_remote_ready(vdev);
  log_info("TRACE: wait_remote_ready complete\n");

  /* USER CODE BEGIN  POST_VIRTIO_INIT */

  /* USER CODE END POST_VIRTIO_INIT */
  vring_rsc = &rsc_table->vring0;
  status = rproc_virtio_init_vring(vdev, 0, vring_rsc->notifyid,
                                   (void *)vring_rsc->da, shm_io,
                                   vring_rsc->num, vring_rsc->align);
  if (status != 0)
  {
    return status;
  }
  log_info("TRACE: vring0 complete da=0x%08lx\n",
           (unsigned long)vring_rsc->da);


  /* USER CODE BEGIN  POST_VRING0_INIT */

  /* USER CODE END POST_VRING0_INIT */
  vring_rsc = &rsc_table->vring1;
  status = rproc_virtio_init_vring(vdev, 1, vring_rsc->notifyid,
                                   (void *)vring_rsc->da, shm_io,
                                   vring_rsc->num, vring_rsc->align);
  if (status != 0)
  {
    return status;
  }
  log_info("TRACE: vring1 complete da=0x%08lx\n",
           (unsigned long)vring_rsc->da);

  /* USER CODE BEGIN  POST_VRING1_INIT */

  /* USER CODE END POST_VRING1_INIT */

  rpmsg_virtio_init_shm_pool(&shpool, (void *)VRING_BUFF_ADDRESS,
                             (size_t)VRING_BUFF_SIZE);
  log_info("TRACE: shmem pool addr=0x%08lx size=%lu\n",
           (unsigned long)VRING_BUFF_ADDRESS,
           (unsigned long)VRING_BUFF_SIZE);
  status = rpmsg_init_vdev(&rvdev, vdev, ns_bind_cb, shm_io, &shpool);
  if (status != 0)
    return status;
  log_info("TRACE: rpmsg_init_vdev complete features=0x%08lx\n",
           (unsigned long)vdev->features);

  /* USER CODE BEGIN POST_RPMSG_INIT */

  /* USER CODE END POST_RPMSG_INIT */

  return 0;
}

void OPENAMP_DeInit()
{

  /* USER CODE BEGIN PRE_OPENAMP_DEINIT */

  /* USER CODE END PRE_OPENAMP_DEINIT */

  rpmsg_deinit_vdev(&rvdev);

  metal_finish();

  /* USER CODE BEGIN POST_OPENAMP_DEINIT */

  /* USER CODE END POST_OPENAMP_DEINIT */
}

int OPENAMP_create_endpoint(struct rpmsg_endpoint *ept, const char *name,
                            uint32_t dest, rpmsg_ept_cb cb,
                            rpmsg_ns_unbind_cb unbind_cb)
{
  int ret = 0;
  /* USER CODE BEGIN PRE_EP_CREATE */

  /* USER CODE END PRE_EP_CREATE */

  ret = rpmsg_create_ept(ept, &rvdev.rdev, name, RPMSG_ADDR_ANY, dest, cb,
		          unbind_cb);

  /* USER CODE BEGIN POST_EP_CREATE */

  /* USER CODE END POST_EP_CREATE */
  return ret;
}

/* Observe live queue pointers rather than assuming a fixed layout. */
void OPENAMP_trace_queues(void)
{
#if defined(__LOG_TRACE_IO_)
  unsigned int i;
  log_info("TRACE: rsc=%08lx status=%lu gfeatures=%08lx MPU=%08lx RASR=%08lx\n",
      (unsigned long)rsc_table,
      (unsigned long)*(volatile uint8_t *)&rsc_table->vdev.status,
      (unsigned long)*(volatile uint32_t *)&rsc_table->vdev.gfeatures,
      (unsigned long)MPU->CTRL, (unsigned long)MPU->RASR);
  for (i = 0; i < rvdev.vdev->vrings_num; ++i) {
    trace_checkpoint(0x100 + i, (uint32_t)rvdev.vdev->vrings_info);
    struct virtqueue *vq = rvdev.vdev->vrings_info[i].vq;
    trace_checkpoint(0x110 + i, (uint32_t)vq);
    if (!vq) {
      trace_checkpoint(0xe100 + i, 0);
      return;
    }
    struct vring *vr = &vq->vq_ring;
    trace_checkpoint(0x140 + i, (uint32_t)vr->avail);
    unsigned int ai = *(volatile uint16_t *)&vr->avail->idx;
    trace_checkpoint(0x150 + i, (uint32_t)vr->used);
    unsigned int ui = *(volatile uint16_t *)&vr->used->idx;
    trace_checkpoint(0x120 + i, ui);
    log_info("TRACE: q%u n=%u desc=%08lx avail=%08lx used=%08lx ai=%u ui=%u\n",
        i, vr->num, (unsigned long)vr->desc,
        (unsigned long)vr->avail, (unsigned long)vr->used,
        ai, ui);
    trace_checkpoint(0x130 + i, ui);
  }
  trace_checkpoint(0x180, rvdev.vdev->vrings_num);
#endif
}

void OPENAMP_check_for_message(void)
{
  /* USER CODE BEGIN MSG_CHECK */

  /* USER CODE END MSG_CHECK */
  MAILBOX_Poll(rvdev.vdev);
}

void OPENAMP_Wait_EndPointready(struct rpmsg_endpoint *rp_ept)
{
  /* USER CODE BEGIN EP_READY */

  /* USER CODE END EP_READY */

  while(!is_rpmsg_ept_ready(rp_ept))
  {
    /* USER CODE BEGIN 0 */

    /* USER CODE END 0 */
      MAILBOX_Poll(rvdev.vdev);

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */
  }
}
