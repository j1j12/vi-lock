#ifndef SERVO_TIMING_H
#define SERVO_TIMING_H
#include <stdint.h>
/* Round time to timer counts using the actual kernel clock and divider. */
static inline uint32_t Servo_Counts(uint32_t hz, uint32_t divider, uint32_t us)
{
    if (!divider) return 0;
    uint64_t denominator = (uint64_t)divider * 1000000u;
    return (uint32_t)(((uint64_t)hz * us + denominator / 2u) / denominator);
}
static inline uint32_t Servo_Divider(uint32_t hz)
{
    if (!hz) return 0;
    uint32_t divider = (uint32_t)(((uint64_t)hz + 500000u) / 1000000u);
    if (!divider) divider = 1;
    return divider <= 65536u ? divider : 0;
}
#endif
