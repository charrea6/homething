#ifndef _LED_STRIPS_H_
#define _LED_STRIPS_H_
#include "cbor.h"
#include "notifications.h"

int initLEDStripSPI(int nrofStrips);
Notifications_ID_t addLEDStripSPI(CborValue *entry);
#define LED_STRIP_SPI_COMPONENT {initLEDStripSPI, addLEDStripSPI}
#endif