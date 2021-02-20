#ifndef _LED_H_
#define _LED_H_
#include "cbor.h"
#include "notifications.h"

int initLeds(int nrofRelays);
Notifications_ID_t addLed(CborValue *entry);
#define LED_COMPONENT {initLeds, addLed}
#endif