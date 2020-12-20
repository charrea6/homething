#ifndef _SWITCHES_H_
#define _SWITCHES_H_
#include "cbor.h"
#include "notifications.h"

int initSwitches(int norfSwitches);
int addSwitch(CborValue *entry);
void switchRelayController(void *user, NotificationsMessage_t *message);
#endif