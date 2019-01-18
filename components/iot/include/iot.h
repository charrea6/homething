#ifndef _IOT_H_
#define _IOT_H_
#include <stdbool.h>

#define IOT_MAX_ELEMENT 8
#define IOT_MAX_PUB 5
#define IOT_MAX_SUB 5

typedef union {
    int i;
    float f;
    bool b;
    const char *s;
} iotValue_t;

enum iotValueType_e {
    iotValueType_Bool,
    iotValueType_Int,
    iotValueType_Float,
    iotValueType_String
};

struct iotElementSub;

typedef void (*iotElementSubUpdateCallback_t)(void *userData, struct iotElementSub *sub, iotValue_t value);

typedef struct iotElementSub {
    const char *name;
    char *path;
    enum iotValueType_e type;
    iotElementSubUpdateCallback_t callback;
    void *userData;
}iotElementSub_t;

typedef struct iotElementPub {
    const char *name;
    enum iotValueType_e type;
    iotValue_t value;
    bool updateRequired:1;
    bool retain:1;
}iotElementPub_t;

typedef struct {
    const char *name;
    iotElementSub_t subs[IOT_MAX_SUB];
    iotElementPub_t pubs[IOT_MAX_PUB];
    bool updateRequired:1;
}iotElement_t;

/** Initialse the IOT subsystem.
 * This needs to be done before you can add Elements and Pub/Sub items.
 */
void iotInit(void);

/** Start the IOT Subsystem
 * Connects to the configured Wifi network and then to the configured MQTT server.
 * Once connected any updates to Pub items will be reflected to the MQTT server every
 * period (a period is currently 0.1 seconds).
 * Any message sent to subscribe topics (iotElementSub_t) will be reflected to the specified 
 * callback as soon as they are recieved.
 */
void iotStart();

void iotElementAdd(const char *name, iotElement_t **ppElement);
void iotElementSubAdd(iotElement_t *element, const char *name, enum iotValueType_e type, iotElementSubUpdateCallback_t callback, void *userData, iotElementSub_t **ppSub);
void iotElementPubAdd(iotElement_t *element, const char *name, enum iotValueType_e type, bool retain, iotValue_t initial, iotElementPub_t **ppPub);
void iotElementPubUpdate(iotElementPub_t *pub, iotValue_t value);



#endif
