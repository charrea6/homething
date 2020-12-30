#ifndef _RELAYS_H_
#define _RELAYS_H_
#include "cbor.h"
#include "notifications.h"

int initRelays(int nrofRelays);
int addRelay(CborValue *entry, Notifications_ID_t *ids, uint32_t idCount);
#endif