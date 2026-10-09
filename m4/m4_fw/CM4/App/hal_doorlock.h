#ifndef HAL_DOORLOCK_H
#define HAL_DOORLOCK_H

#define DOORLOCK_ANGLE_LOCKED    0u
#define DOORLOCK_ANGLE_UNLOCKED  90u

void DoorLock_Init(void);
void DoorLock_Open(void);
void DoorLock_Close(void);

#endif /* HAL_DOORLOCK_H */
