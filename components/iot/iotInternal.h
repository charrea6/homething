#ifndef _IOTINTERNAL_H_

#define MQTT_PATH_PREFIX_LEN 23 // homething/<MAC 12 Hexchars> \0
#define MQTT_COMMON_CTRL_SUB_LEN (MQTT_PATH_PREFIX_LEN + 7) // "/+/ctrl"

struct iotElement {
    const iotElementDescription_t *desc;
    uint32_t flags;
    char *name;
    void *userContext;
    iotElementCallback_t callback;
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