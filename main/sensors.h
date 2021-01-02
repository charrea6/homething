#ifndef _SENSORS_H_
#define _SENSORS_H_
#include "cbor.h"
#include "notifications.h"
int initDHT22(int nrofSensors);
int initBME280(int nrofSensors);
int initSI7021(int nrofSensors);
Notifications_ID_t addDHT22(CborValue *entry);
Notifications_ID_t addBME280(CborValue *entry);
Notifications_ID_t addSI7021(CborValue *entry);
#endif