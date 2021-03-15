#ifndef _IOTINTERNAL_H_

#define MQTT_PATH_PREFIX_LEN 23 // homething/<MAC 12 Hexchars> \0
#define MQTT_COMMON_CTRL_SUB_LEN (MQTT_PATH_PREFIX_LEN + 7) // "/+/ctrl"

#define PUB_INDEX_TYPE 0
#define PUB_INDEX_NAME 1

#define SUB_INDEX_TYPE 0
#define SUB_INDEX_NAME 1

#define PUB_GET_TYPE(_element, _pubId) ((_element)->desc->pubs[_pubId][PUB_INDEX_TYPE])
#define PUB_GET_NAME(_element, _pubId) (&(_element)->desc->pubs[_pubId][PUB_INDEX_NAME])

#define SUB_GET_TYPE(_element, _subId) ((_element)->desc->subs[_subId].type_name[SUB_INDEX_TYPE])
#define SUB_GET_NAME(_element, _subId) (&(_element)->desc->subs[_subId].type_name[SUB_INDEX_NAME])
#define SUB_GET_CALLBACK(_element, _subId) ((_element)->desc->subs[_subId].callback)

#define _HEX_TYPE(v) 0x0 ## v
#define HEX_TYPE(type) _HEX_TYPE(type)

#define VT_BOOL       HEX_TYPE(IOT_VALUE_TYPE_BOOL)
#define VT_INT        HEX_TYPE(IOT_VALUE_TYPE_INT)
#define VT_FLOAT      HEX_TYPE(IOT_VALUE_TYPE_FLOAT)
#define VT_STRING     HEX_TYPE(IOT_VALUE_TYPE_STRING)
#define VT_BINARY     HEX_TYPE(IOT_VALUE_TYPE_BINARY)
#define VT_HUNDREDTHS HEX_TYPE(IOT_VALUE_TYPE_HUNDREDTHS)
#define VT_CELSIUS    HEX_TYPE(IOT_VALUE_TYPE_CELSIUS)
#define VT_PERCENT_RH HEX_TYPE(IOT_VALUE_TYPE_PERCENT_RH)
#define VT_KPA        HEX_TYPE(IOT_VALUE_TYPE_KPA)
#define VT_ON_CONNECT HEX_TYPE(IOT_VALUE_TYPE_ON_CONNECT)

#define VT_BARE_TYPE(type) (type & 0x7f)
#define VT_IS_RETAINED(type) ((type & 0x80) == 0x80) ? 1:0


struct iotElement {
    const iotElementDescription_t *desc;
    uint32_t flags;
    char *name;
    void *userContext;
    struct iotElement *next;
    iotValue_t values[];
};

iotElement_t iotElementsHead;
const char *IOT_DEFAULT_CONTROL_STR;

bool mqttIsSetup;
bool mqttIsConnected;

int mqttInit(void);
void mqttNetworkConnected(bool connected);
bool mqttSubscribe(char *topic);

void iotMqttProcessMessage(char *topic, char *data, int dataLen);
void iotMqttConnected(void);

int iotDeviceInit(void);
void iotDeviceStart(void);
#endif