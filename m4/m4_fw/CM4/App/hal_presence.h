#ifndef HAL_PRESENCE_H
#define HAL_PRESENCE_H

#include <stdbool.h>

void PresenceSensor_Init(void);
bool PresenceSensor_GetState(void);   /* true = human present */

#endif /* HAL_PRESENCE_H */
