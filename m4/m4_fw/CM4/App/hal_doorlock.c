#include "hal_doorlock.h"
#include "hal_servo.h"

void DoorLock_Init(void)
{
    Servo_Init();
    DoorLock_Close();
}

void DoorLock_Open(void)
{
    Servo_SetAngle(DOORLOCK_ANGLE_UNLOCKED);
}

void DoorLock_Close(void)
{
    Servo_SetAngle(DOORLOCK_ANGLE_LOCKED);
}
