#ifndef APP_ACCESS_H
#define APP_ACCESS_H

#include <stdint.h>
#include <stdbool.h>
#include "rpmsg_protocol.h"

/* Door control command (M4 internal message queue) */
typedef enum {
    DOOR_CMD_UNLOCK = 1,
    DOOR_CMD_LOCK   = 2,
} door_cmd_t;

typedef enum {
    DOOR_STATE_LOCKED   = 0,
    DOOR_STATE_UNLOCKED = 1,
} door_state_t;

/* Application init (HAL level, before scheduler starts) */
void App_Init(void);

/* Create FreeRTOS tasks (called from freertos.c after osKernelInitialize) */
void App_CreateTasks(void);

/* Door command queue accessors */
bool App_SendDoorCmd(door_cmd_t cmd);
bool App_RecvDoorCmd(door_cmd_t *cmd, uint32_t timeout_ms);

/* Door state */
door_state_t App_GetDoorState(void);
void App_SetDoorState(door_state_t state);

/* Outgoing RPMsg queue (drained by RPMsgTask) */
void App_NotifyRPMsg(rpmsg_msg_id_t id, uint32_t value);
bool App_GetRPMsg(rpmsg_msg_t *msg, uint32_t timeout_ms);

/* FreeRTOS task entry points */
void SensorTask(void *argument);
void DoorControlTask(void *argument);
void RPMsgTask(void *argument);

#endif /* APP_ACCESS_H */
