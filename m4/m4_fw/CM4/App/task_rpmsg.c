#include "app_access.h"
#include "rpmsg_protocol.h"
#include "openamp.h"
#include "main.h"
#include "cmsis_os2.h"
#include "openamp_log.h"
#if defined(SERVO_MANUAL_TEST)
#include "hal_servo.h"
#endif

/*
 * RPMsgTask: OpenAMP/RPMsg remote-side message handling (M4 <-> A7 Linux).
 *
 *   TX (M4 -> A7): PONG, PERSON_DETECTED, UNLOCK_DONE, DOOR_STATE
 *   RX (A7 -> M4): PING, AUTH_SUCCESS, AUTH_FAILED, UNLOCK
 */

static struct rpmsg_endpoint s_ept;

static int rpmsg_endpoint_cb(struct rpmsg_endpoint *ept, void *data,
                             size_t len, uint32_t src, void *priv)
{
    (void)src;
    (void)priv;

    if ((data == NULL) || (len < sizeof(rpmsg_msg_t))) {
        return RPMSG_SUCCESS;
    }

    rpmsg_msg_t *msg = (rpmsg_msg_t *)data;
#if defined(SERVO_MANUAL_TEST)
    /* Separate diagnostic namespace; production UNLOCK cannot trigger PWM. */
#if defined(SERVO_ONESHOT_TEST)
    if (msg->msg_id == 0x53525632UL && len == sizeof(*msg) && msg->value == 1) {
        rpmsg_msg_t reply = { .msg_id = 0x53525633UL, .value = 0x100UL | ServoTest_State() };
        rpmsg_send(ept, &reply, sizeof(reply));
        return RPMSG_SUCCESS;
    }
    if (msg->msg_id == 0x53525634UL && len == sizeof(*msg) && msg->value == 0) {
        uint32_t result = ServoTest_Cycle();
        log_info("SERVO ONESHOT: result=%lu\n", (unsigned long)result);
        rpmsg_msg_t reply = { .msg_id = 0x53525631UL, .value = result };
        rpmsg_send(ept, &reply, sizeof(reply));
        return RPMSG_SUCCESS;
    }
#endif
    if (msg->msg_id == 0x53525632UL && len == sizeof(*msg) && msg->value == 0) {
        rpmsg_msg_t reply = { .msg_id = 0x53525633UL, .value = ServoTest_State() };
        rpmsg_send(ept, &reply, sizeof(reply));
        return RPMSG_SUCCESS;
    }
    if (msg->msg_id == 0x53525630UL && len == sizeof(*msg)) {
        uint32_t result = ServoTest_Command(msg->value);
        log_info("SERVO TEST: request=%lu result=%lu\n", (unsigned long)msg->value,
                 (unsigned long)result);
        rpmsg_msg_t reply = { .msg_id = 0x53525631UL, .value = result };
        rpmsg_send(ept, &reply, sizeof(reply));
        return RPMSG_SUCCESS;
    }
#endif

    switch ((rpmsg_msg_id_t)msg->msg_id) {
    case RPMSG_MSG_PING: {
        rpmsg_msg_t reply = { .msg_id = RPMSG_MSG_PONG, .value = 0 };
        rpmsg_send(ept, &reply, sizeof(reply));
        break;
    }
    case RPMSG_MSG_UNLOCK:
        App_SendDoorCmd(DOOR_CMD_UNLOCK);
        break;
    case RPMSG_MSG_AUTH_SUCCESS:
    case RPMSG_MSG_AUTH_FAILED:
    default:
        /* informational / ignored for now */
        break;
    }

    return RPMSG_SUCCESS;
}

void RPMsgTask(void *argument)
{
    (void)argument;
    log_info("TRACE: RPMsgTask entered\n");
#if defined(RPMSG_BRINGUP_ONLY)
    log_info("TRACE: timebase-v6; real ready/features negotiation\n");
    log_info("CLOCK: pclk1=%lu tim6=%lu PSC=%lu ARR=%lu rtos_hz=%lu\n",
             (unsigned long)HAL_RCC_GetPCLK1Freq(),
             (unsigned long)HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_TIM6),
             (unsigned long)TIM6->PSC, (unsigned long)TIM6->ARR,
             (unsigned long)osKernelGetTickFreq());
#endif

    if (MX_OPENAMP_Init(RPMSG_REMOTE, NULL) != 0) {
        log_err("TRACE: MX_OPENAMP_Init failed\n");
        Error_Handler();
    }
    log_info("TRACE: MX_OPENAMP_Init complete\n");
    OPENAMP_trace_queues();
    trace_checkpoint(0x200, 0);
    log_info("TRACE: endpoint create begin (timebase-v6)\n");
    trace_checkpoint(0x201, 0);

    if (OPENAMP_create_endpoint(&s_ept, RPMSG_ENDPOINT_NAME, RPMSG_ADDR_ANY,
                                rpmsg_endpoint_cb, NULL) != 0) {
        log_err("TRACE: endpoint create failed\n");
        Error_Handler();
    }
    log_info("TRACE: endpoint created\n");
    trace_checkpoint(0x202, 0);
    __DSB();
    OPENAMP_trace_queues();
    {
        uint32_t last = HAL_GetTick();
        unsigned int samples = 0;
        while (!is_rpmsg_ept_ready(&s_ept)) {
            trace_checkpoint(0x300, osKernelGetTickCount());
            OPENAMP_check_for_message();
            trace_checkpoint(0x301, osKernelGetTickCount());
            if (samples < 3 && (uint32_t)(HAL_GetTick() - last) >= 1000) {
                log_info("TRACE: waiting for host endpoint sample=%u\n", ++samples);
                OPENAMP_trace_queues();
                last = HAL_GetTick();
            }
            osDelay(1);
        }
    }
    log_info("TRACE: endpoint ready\n");
    trace_checkpoint(0x400, 0);

    for (;;) {
        rpmsg_msg_t out;
        while (App_GetRPMsg(&out, 0)) {
            rpmsg_send(&s_ept, &out, sizeof(out));
        }

        OPENAMP_check_for_message();
        trace_checkpoint(0x401, osKernelGetTickCount());
        osDelay(1);
    }
}
