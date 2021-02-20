#ifndef _SENSORS_H_
#define _SENSORS_H_
#include "cbor.h"
#include "notifications.h"
#include "sdkconfig.h"

#ifdef CONFIG_DHT22
int initDHT22(int nrofSensors);
Notifications_ID_t addDHT22(CborValue *entry);
#define DHT22_COMPONENT {initDHT22, addDHT22}
#else
#define DHT22_COMPONENT {NULL, NULL}
#endif

#ifdef CONFIG_BME280
int initBME280(int nrofSensors);
Notifications_ID_t addBME280(CborValue *entry);
#define BME280_COMPONENT {initBME280, addBME280}
#else
#define BME280_COMPONENT {NULL, NULL}
#endif

#ifdef CONFIG_SI7021
int initSI7021(int nrofSensors);
Notifications_ID_t addSI7021(CborValue *entry);
#define SI7021_COMPONENT {initSI7021, addSI7021}
#else
#define SI7021_COMPONENT {NULL, NULL}
#endif

#ifdef CONFIG_DS18x20
int initDS18x20(int nrofSensors);
Notifications_ID_t addDS18x20(CborValue *entry);
#define DS18x20_COMPONENT {initDS18x20, addDS18x20}
#else
#define DS18x20_COMPONENT {NULL, NULL}
#endif

#endif