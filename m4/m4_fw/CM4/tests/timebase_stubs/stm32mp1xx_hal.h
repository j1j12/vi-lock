#ifndef TIMEBASE_TEST_HAL_H
#define TIMEBASE_TEST_HAL_H
#include <stdint.h>
#define __NVIC_PRIO_BITS 4
#define RCC_PERIPHCLK_TIM6 1
#define TIM6 ((void *)0x1000)
#define TIM6_IRQn 54
#define TIM_CLOCKDIVISION_DIV1 0
#define TIM_COUNTERMODE_UP 0
#define TIM_IT_UPDATE 1
#define __HAL_RCC_TIM6_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM6_FORCE_RESET() ((void)0)
#define __HAL_RCC_TIM6_RELEASE_RESET() ((void)0)
#define __HAL_TIM_DISABLE_IT(h, i) ((void)(h))
#define __HAL_TIM_ENABLE_IT(h, i) ((void)(h))
typedef enum {HAL_OK, HAL_ERROR} HAL_StatusTypeDef;
typedef enum {HAL_TICK_FREQ_1KHZ=1, HAL_TICK_FREQ_100HZ=10, HAL_TICK_FREQ_10HZ=100} HAL_TickFreqTypeDef;
extern HAL_TickFreqTypeDef uwTickFreq;
extern uint32_t uwTickPrio;
typedef struct {
    void *Instance;
    struct {uint32_t Period, Prescaler, ClockDivision, CounterMode;} Init;
} TIM_HandleTypeDef;
uint32_t HAL_RCCEx_GetPeriphCLKFreq(uint64_t peripheral);
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *timer);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *timer);
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority);
void HAL_NVIC_EnableIRQ(int irq);
#endif
