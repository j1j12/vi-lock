#include "hal_presence.h"
#include "main.h"
#include "gpio.h"
#include "openamp_log.h"

/*
 * HLK-LD2401 24GHz human presence radar.
 * OUT pin = PE4 (GPIO input, configured in gpio.c).
 *   HIGH = human present
 *   LOW  = absent
 *
 * The UART7 interface (256000 baud) is reserved for configuration and
 * distance reporting, used in a later stage.
 */
#if defined(PRESENCE_OUT_ONLY)
#define PRESENCE_GPIO_PORT   GPIOC
#define PRESENCE_GPIO_PIN    GPIO_PIN_13
#else
#define PRESENCE_GPIO_PORT   GPIOE
#define PRESENCE_GPIO_PIN    GPIO_PIN_4
#endif

void PresenceSensor_Init(void)
{
#if defined(PRESENCE_OUT_ONLY)
    GPIO_InitTypeDef config = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    config.Pin = GPIO_PIN_13;
    config.Mode = GPIO_MODE_INPUT;
    config.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOC, &config);
#if defined(SERVO_RADAR_TEST)
    log_info("PRESENCE v12: LD2412 PC13 input; manual servo only; UART disabled\n");
#else
    log_info("PRESENCE: LD2412 PC13 input; UART and servo disabled\n");
#endif
#endif
}

bool PresenceSensor_GetState(void)
{
    return (HAL_GPIO_ReadPin(PRESENCE_GPIO_PORT, PRESENCE_GPIO_PIN) == GPIO_PIN_SET);
}
