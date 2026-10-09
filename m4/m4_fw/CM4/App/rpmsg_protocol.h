#ifndef RPMSG_PROTOCOL_H
#define RPMSG_PROTOCOL_H

#include <stdint.h>

/*
 * A7 <-> M4 RPMsg message protocol.
 * MUST stay identical on both sides (M4 firmware and A7 application).
 */

typedef enum {
    RPMSG_MSG_PING            = 0,
    RPMSG_MSG_PONG            = 1,
    RPMSG_MSG_PERSON_DETECTED = 2,   /* M4 -> A7 : human presence detected */
    RPMSG_MSG_AUTH_SUCCESS    = 3,   /* A7 -> M4 : face recognition authorized */
    RPMSG_MSG_AUTH_FAILED     = 4,   /* A7 -> M4 : face recognition denied */
    RPMSG_MSG_UNLOCK          = 5,   /* A7 -> M4 : command to unlock door */
    RPMSG_MSG_UNLOCK_DONE     = 6,   /* M4 -> A7 : door action completed */
    RPMSG_MSG_DOOR_STATE      = 7,   /* M4 -> A7 : door state report */
} rpmsg_msg_id_t;

#define DOOR_STATE_VAL_LOCKED    0u
#define DOOR_STATE_VAL_UNLOCKED  1u

/* Generic RPMsg frame: message id + optional value. */
typedef struct {
    uint32_t msg_id;
    uint32_t value;
} rpmsg_msg_t;

#define RPMSG_ENDPOINT_NAME "rpmsg-access-ctrl"

#endif /* RPMSG_PROTOCOL_H */
