#ifndef _SWITCHES_H_
#define _SWITCHES_H_
#include "cbor.h"
#include "notifications.h"

int initSwitches(int norfSwitches);
Notifications_ID_t addSwitch(CborValue *entry);
void switchRelayController(void *user, NotificationsMessage_t *message);
#define SWITCHES_COMPONENT {initSwitches, addSwitch}
#endif