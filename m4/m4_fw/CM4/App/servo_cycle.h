#ifndef SERVO_CYCLE_H
#define SERVO_CYCLE_H
#include <stdint.h>
// Return0=no transition,1=PWM off/wait,2=start close PWM,3=stop timer.
static inline uint32_t ServoCycle_Tick(volatile uint32_t *remaining,
                                     volatile uint32_t *phase) {
    if (!*remaining || --*remaining) return 0;
    if (*phase == 1) { *phase = 2; *remaining = 100; return 1; }
    if (*phase == 2) { *phase = 3; *remaining = 50; return 2; }
    *phase = 0;
    return 3;
}
#endif
