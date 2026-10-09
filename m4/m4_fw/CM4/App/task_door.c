#include "app_access.h"
#include "hal_doorlock.h"
#include "cmsis_os2.h"

/*
 * DoorControlTask: wait for door commands and drive the SG90 servo lock.
 * On UNLOCK: open the lock, hold for a while, then close automatically.
 */

#define DOOR_HOLD_MS   3000u

void DoorControlTask(void *argument)
{
    (void)argument;

    for (;;) {
        door_cmd_t cmd;
        if (App_RecvDoorCmd(&cmd, osWaitForever)) {
            if (cmd == DOOR_CMD_UNLOCK) {
                DoorLock_Open();
                App_SetDoorState(DOOR_STATE_UNLOCKED);
                App_NotifyRPMsg(RPMSG_MSG_DOOR_STATE, DOOR_STATE_VAL_UNLOCKED);
                osDelay(DOOR_HOLD_MS);
                DoorLock_Close();
                App_SetDoorState(DOOR_STATE_LOCKED);
                App_NotifyRPMsg(RPMSG_MSG_DOOR_STATE, DOOR_STATE_VAL_LOCKED);
                App_NotifyRPMsg(RPMSG_MSG_UNLOCK_DONE, 0);
            } else if (cmd == DOOR_CMD_LOCK) {
                DoorLock_Close();
                App_SetDoorState(DOOR_STATE_LOCKED);
                App_NotifyRPMsg(RPMSG_MSG_DOOR_STATE, DOOR_STATE_VAL_LOCKED);
            }
        }
    }
}
