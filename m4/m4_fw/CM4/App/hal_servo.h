#ifndef HAL_SERVO_H
#define HAL_SERVO_H

#include <stdint.h>

#define SERVO_ANGLE_MIN   0u
#define SERVO_ANGLE_MAX   180u

void Servo_Init(void);
void Servo_SetAngle(uint16_t angle_deg);

#if defined(SERVO_MANUAL_TEST)
void ServoTest_Init(void);
void ServoTest_Stop(void);
uint32_t ServoTest_Command(uint32_t pulse_us);
uint32_t ServoTest_State(void); /* 0=PWM registers off, 1=active */
#if defined(SERVO_ONESHOT_TEST)
uint32_t ServoTest_Cycle(void);
#endif
#endif

#endif /* HAL_SERVO_H */
