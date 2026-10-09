#include "app_access.h"
#include "hal_presence.h"
#include "cmsis_os2.h"
#include "openamp_log.h"

/*
 * SensorTask: poll the presence radar and detect a rising edge
 * (no person -> person present). On detection, notify the A7 via RPMsg
 * (PERSON_DETECTED). This does not authorize motion. Current bench GUI only
 * displays recognition; manual servo firmware has no DoorControlTask.
 */

#define SENSOR_POLL_MS         50u
#define SENSOR_DEBOUNCE_CNT    3u

void SensorTask(void *argument)
{
    (void)argument;

    bool previous = false;
    uint32_t high_count = 0;

    PresenceSensor_Init();

    for (;;) {
        bool present = PresenceSensor_GetState();

        if (present) {
            if (high_count < SENSOR_DEBOUNCE_CNT) {
                high_count++;
            }
        } else {
            high_count = 0;
        }

        bool debounced = (high_count >= SENSOR_DEBOUNCE_CNT);

        if (debounced && !previous) {
            log_info("PRESENCE: detected\n");
            /* Rising edge: person appeared. Notify A7 via RPMsg. */
            App_NotifyRPMsg(RPMSG_MSG_PERSON_DETECTED, 0);
        }

        if (!debounced && previous) {
            log_info("PRESENCE: absent\n");
        }

        previous = debounced;
        osDelay(SENSOR_POLL_MS);
    }
}
