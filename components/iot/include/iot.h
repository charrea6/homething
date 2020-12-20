#ifndef _IOT_H_
#define _IOT_H_
#include <stdbool.h>
#include <stdint.h>

#define IOT_DEFAULT_CONTROL NULL

typedef struct {
    uint8_t *data;
    int32_t len;
} iotBinaryValue_t;

typedef union {
    int i;
    float f;
    bool b;
    const char *s;
    const iotBinaryValue_t *bin;
} iotValue_t;

typedef struct iotElement *iotElement_t;

typedef void (*iotElementSubUpdateCallback_t)(void *userData, iotElement_t element, iotValue_t value);

typedef struct iotElementSubDescription {
    const char *type_name;
    iotElementSubUpdateCallback_t callback;
}iotElementSubDescription_t;

typedef struct iotElementDescription {
    const char * const *pubs;
    const int nrofPubs;
    const iotElementSubDescription_t const *subs;
    const int nrofSubs;
}iotElementDescription_t;

#define IOT_VALUE_TYPE_BOOL            00
#define IOT_VALUE_TYPE_INT             01
#define IOT_VALUE_TYPE_FLOAT           02
#define IOT_VALUE_TYPE_STRING          03
#define IOT_VALUE_TYPE_BINARY          04

#define IOT_VALUE_TYPE_RETAINED_BOOL   80
#define IOT_VALUE_TYPE_RETAINED_INT    81
#define IOT_VALUE_TYPE_RETAINED_FLOAT  82
#define IOT_VALUE_TYPE_RETAINED_STRING 83
#define IOT_VALUE_TYPE_RETAINED_BINARY 84

#define IOT_SUB_DEFAULT_NAME ""
#define IOT_PUB_USE_ELEMENT ""

#define _IOT_STRINGIFY(x) #x
#define _IOT_DEFINE_TYPE(t) _IOT_STRINGIFY(\x ## t)

#define IOT_DESCRIBE_PUB(type, name) _IOT_DEFINE_TYPE(type) name
#define IOT_PUB_DESCRIPTIONS(pubs...) { pubs }
#define IOT_DESCRIBE_SUB(type, name, _callback) { .type_name = _IOT_DEFINE_TYPE(type) name, .callback=_callback }
#define IOT_SUB_DESCRIPTIONS(subs...) { subs }

#define IOT_DESCRIBE_ELEMENT(name, pub_descriptions, sub_descriptions) \
    static const char * const name ## _pubs[] = pub_descriptions; \
    static const iotElementSubDescription_t name ## _subs[] = sub_descriptions; \
    static const iotElementDescription_t name = { \
        .pubs = name ## _pubs,\
        .nrofPubs = sizeof(name ## _pubs) / sizeof(char*),\
        .subs = name ## _subs,\
        .nrofSubs= sizeof(name ## _subs) / sizeof(iotElementSubDescription_t)\
        }

#define IOT_DESCRIBE_ELEMENT_NO_SUBS(name, pub_descriptions) \
    static const char *name ## _pubs[] = pub_descriptions; \
    static const iotElementDescription_t name = { \
        .pubs = name ## _pubs,\
        .nrofPubs = sizeof(name ## _pubs) / sizeof(char*),\
        .subs = NULL,\
        .nrofSubs= 0\
        }

/** Initialse the IOT subsystem.
 * This needs to be done before you can add Elements and Pub/Sub items.
 */
int iotInit(void);

/** Start the IOT Subsystem
 * Connects to the configured Wifi network and then to the configured MQTT server.
 * Once connected any updates to Pub items will be reflected to the MQTT server if
 * connected or stored for transmission when a connection is established.
 * Any message sent to subscribe topics will be reflected to the specified 
 * callback as soon as they are recieved.
 */
void iotStart();

iotElement_t iotNewElement(const iotElementDescription_t *desc, void *userContext, const char const *nameFormat, ...);

void iotElementPublish(iotElement_t element, int pubId, iotValue_t value);

/** Convert a string value to a boolean
 * Accepts "on"/"off", "true"/"false" (ignoring case) and returns 0;
 * Invalid values return 1
 */
int iotStrToBool(const char *str, bool *out);

#endif
