#ifndef _DHT_H_
#define _DHT_H_

#include <stdint.h>
#include "notifications.h"

Notifications_ID_t dht22Add(uint8_t pin);
void dht22Start(void);
#endif
