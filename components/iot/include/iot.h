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

#define IOT_VALUE_TYPE_BOOL            0
#define IOT_VALUE_TYPE_INT             1
#define IOT_VALUE_TYPE_FLOAT           2
#define IOT_VALUE_TYPE_STRING          3
#define IOT_VALUE_TYPE_BINARY          4
#define IOT_VALUE_TYPE_HUNDREDTHS      5
#define IOT_VALUE_TYPE_CELCIUS         6
#define IOT_VALUE_TYPE_PERCENT_RH      7
#define IOT_VALUE_TYPE_KPA             8

#define IOT_VALUE_NOT_RETAINED 0
#define IOT_VALUE_RETAINED     8

#define _IOT_STRINGIFY(x) #x
#define _IOT_DEFINE_TYPE_2(r, t) _IOT_STRINGIFY(\x ## r ## t)
#define _IOT_DEFINE_TYPE_1(r, t) _IOT_DEFINE_TYPE_2(r, t)

// Define both retained and not retained types at this point to allow for compile time errors,
// combining the retained setting and type when used doesn't allow use to catch misspelling etc.
#define IOT_VALUE_TYPE_BOOL_NOT_RETAINED        _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_BOOL )    
#define IOT_VALUE_TYPE_BOOL_RETAINED            _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_BOOL )    

#define IOT_VALUE_TYPE_INT_NOT_RETAINED         _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_INT )
#define IOT_VALUE_TYPE_INT_RETAINED             _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_INT )

#define IOT_VALUE_TYPE_FLOAT_NOT_RETAINED       _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_FLOAT)
#define IOT_VALUE_TYPE_FLOAT_RETAINED           _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_FLOAT)

#define IOT_VALUE_TYPE_STRING_NOT_RETAINED      _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_STRING ) 
#define IOT_VALUE_TYPE_STRING_RETAINED          _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_STRING ) 

#define IOT_VALUE_TYPE_BINARY_NOT_RETAINED      _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_BINARY ) 
#define IOT_VALUE_TYPE_BINARY_RETAINED          _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_BINARY ) 

#define IOT_VALUE_TYPE_HUNDREDTHS_NOT_RETAINED  _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_HUNDREDTHS ) 
#define IOT_VALUE_TYPE_HUNDREDTHS_RETAINED      _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_HUNDREDTHS ) 

#define IOT_VALUE_TYPE_CELCIUS_NOT_RETAINED     _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_CELCIUS ) 
#define IOT_VALUE_TYPE_CELCIUS_RETAINED         _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_CELCIUS ) 

#define IOT_VALUE_TYPE_PERCENT_RH_NOT_RETAINED  _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_PERCENT_RH ) 
#define IOT_VALUE_TYPE_PERCENT_RH_RETAINED      _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_PERCENT_RH ) 

#define IOT_VALUE_TYPE_KPA_NOT_RETAINED         _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_KPA ) 
#define IOT_VALUE_TYPE_KPA_RETAINED             _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_KPA ) 

#define _IOT_DEFINE_TYPE(r, t) IOT_VALUE_TYPE_ ## t ## _ ## r

#define IOT_SUB_DEFAULT_NAME ""
#define IOT_PUB_USE_ELEMENT ""

#define IOT_DESCRIBE_PUB(retain, type, name) _IOT_DEFINE_TYPE(retain, type) name
#define IOT_PUB_DESCRIPTIONS(pubs...) { pubs }
#define IOT_DESCRIBE_SUB(type, name, _callback) { .type_name = _IOT_DEFINE_TYPE(NOT_RETAINED, type) name, .callback=_callback }
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

#define IOT_ELEMENT_FLAGS_DONT_ANNOUNCE 1

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

iotElement_t iotNewElement(const iotElementDescription_t *desc, uint32_t flags, void *userContext, const char const *nameFormat, ...);

void iotElementPublish(iotElement_t element, int pubId, iotValue_t value);

/** Convert a string value to a boolean
 * Accepts "on"/"off", "true"/"false" (ignoring case) and returns 0;
 * Invalid values return 1
 */
int iotStrToBool(const char *str, bool *out);

#endif
