#ifndef _HOMEASSISTANT_H_
#define _HOMEASSISTANT_H_
#include "cJSON.h"
#include "iot.h"

void homeAssistantDiscoveryInit(void);
void homeAssistantDiscoveryAnnounce(const char* type, iotElement_t element, const char *pubName, cJSON *details, bool addBasePath);
#endif