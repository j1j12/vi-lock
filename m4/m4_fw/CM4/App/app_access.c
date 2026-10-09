#include "app_access.h"
#include "cmsis_os2.h"
#include "hal_doorlock.h"

#define DOOR_QUEUE_LEN   4u
#define RPMSG_QUEUE_LEN  8u

static osMessageQueueId_t        s_doorQueue = NULL;
static osMessageQueueId_t        s_rpmsgQueue = NULL;
static volatile door_state_t     s_doorState = DOOR_STATE_LOCKED;

void App_Init(void)
{
    DoorLock_Init();
    s_doorState = DOOR_STATE_LOCKED;
}

void App_CreateTasks(void)
{
#if !defined(RPMSG_BRINGUP_ONLY)
    s_doorQueue = osMessageQueueNew(DOOR_QUEUE_LEN, sizeof(door_cmd_t), NULL);
#endif
    s_rpmsgQueue = osMessageQueueNew(RPMSG_QUEUE_LEN, sizeof(rpmsg_msg_t), NULL);

#if !defined(RPMSG_BRINGUP_ONLY) || defined(PRESENCE_OUT_ONLY)
    const osThreadAttr_t sensor_attr = {
        .name = "SensorTask",
        .stack_size = 256 * 4,
        .priority = osPriorityNormal,
    };
    osThreadNew(SensorTask, NULL, &sensor_attr);
#endif
#if !defined(RPMSG_BRINGUP_ONLY)
    const osThreadAttr_t door_attr = {
        .name = "DoorTask",
        .stack_size = 256 * 4,
        .priority = osPriorityNormal,
    };
    osThreadNew(DoorControlTask, NULL, &door_attr);
#endif

    const osThreadAttr_t rpmsg_attr = {
        .name = "RPMsgTask",
        .stack_size = 512 * 4,
        .priority = osPriorityNormal,
    };
    osThreadNew(RPMsgTask, NULL, &rpmsg_attr);
}

bool App_SendDoorCmd(door_cmd_t cmd)
{
    if (s_doorQueue == NULL) {
        return false;
    }
    return (osMessageQueuePut(s_doorQueue, &cmd, 0, 0) == osOK);
}

bool App_RecvDoorCmd(door_cmd_t *cmd, uint32_t timeout_ms)
{
    if (s_doorQueue == NULL) {
        return false;
    }
    return (osMessageQueueGet(s_doorQueue, cmd, NULL, timeout_ms) == osOK);
}

door_state_t App_GetDoorState(void)
{
    return (door_state_t)s_doorState;
}

void App_SetDoorState(door_state_t state)
{
    s_doorState = state;
}

void App_NotifyRPMsg(rpmsg_msg_id_t id, uint32_t value)
{
    if (s_rpmsgQueue == NULL) {
        return;
    }
    rpmsg_msg_t msg = { .msg_id = (uint32_t)id, .value = value };
    (void)osMessageQueuePut(s_rpmsgQueue, &msg, 0, 0);
}

bool App_GetRPMsg(rpmsg_msg_t *msg, uint32_t timeout_ms)
{
    if (s_rpmsgQueue == NULL) {
        return false;
    }
    return (osMessageQueueGet(s_rpmsgQueue, msg, NULL, timeout_ms) == osOK);
}
