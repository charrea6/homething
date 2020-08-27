#ifndef _IOT_H_
#define _IOT_H_
#include <stdbool.h>

#define IOT_DEFAULT_CONTROL NULL

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
    enum iotValueType_e type;
    iotElementSubUpdateCallback_t callback;
    void *userData;

    /* Private fields */
    char *path;
    struct iotElement *element;
    struct iotElementSub *next;
}iotElementSub_t;

typedef struct iotElementPub {
    const char *name;
    struct {
        enum iotValueType_e type:2;
        bool retain:1;
    };
    iotValue_t value;

    /* Private fields */
    struct iotElement *element;
    struct iotElementPub *next;
}iotElementPub_t;

typedef struct iotElement {
    const char *name;
    
    /* Private fields */
    struct iotElement *next;
    iotElementSub_t *subs;
    iotElementPub_t *pubs;
}iotElement_t;

/** Initialse the IOT subsystem.
 * This needs to be done before you can add Elements and Pub/Sub items.
 */
int iotInit(void);

/** Start the IOT Subsystem
 * Connects to the configured Wifi network and then to the configured MQTT server.
 * Once connected any updates to Pub items will be reflected to the MQTT server every
 * period (a period is currently 0.1 seconds).
 * Any message sent to subscribe topics (iotElementSub_t) will be reflected to the specified 
 * callback as soon as they are recieved.
 */
void iotStart();

void iotElementAdd(iotElement_t *element);
void iotElementSubAdd(iotElement_t *element, iotElementSub_t *sub);
void iotElementPubAdd(iotElement_t *element, iotElementPub_t *pub);
void iotElementPubUpdate(iotElementPub_t *pub, iotValue_t value);

/** Convert a string value to a boolean
 * Accepts "on"/"off", "true"/"false" (ignoring case) and returns 0;
 * Invalid values return 1
 */
int iotStrToBool(const char *str, bool *out);

#endif
