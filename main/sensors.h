#ifndef _SENSORS_H_
#define _SENSORS_H_
#include "cbor.h"
#include "notifications.h"
int initDHT22(int nrofDHT22);
Notifications_ID_t addDHT22(CborValue *entry);
#endif