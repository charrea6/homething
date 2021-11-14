#ifndef _IOT_H_
#define _IOT_H_
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define IOT_DEFAULT_CONTROL NULL

typedef char iotValueType_t;

typedef union iotValue iotValue_t;

typedef struct iotElement *iotElement_t;

typedef void *iotElementIterator_t;

typedef void (*iotElementSubUpdateCallback_t)(void *userData, iotElement_t element, iotValue_t value);

typedef void (*iotValueOnConnectCallback_t)(void *userData, iotElement_t element, int pubId, bool release, iotValueType_t *valueType, iotValue_t *value);

typedef struct {
    uint8_t *data;
    int32_t len;
} iotBinaryValue_t;

union iotValue {
    int i;
    float f;
    bool b;
    const char *s;
    const iotBinaryValue_t *bin;
    iotValueOnConnectCallback_t callback;
};

typedef struct iotElementSubDescription {
    const char *type_name;
    iotElementSubUpdateCallback_t callback;
} iotElementSubDescription_t;

typedef struct iotElementDescription {
    const int type;
    const char * const *pubs;
    const int nrofPubs;
    const iotElementSubDescription_t *subs;
    const int nrofSubs;
} iotElementDescription_t;

#define IOT_VALUE_TYPE_BOOL            0
#define IOT_VALUE_TYPE_INT             1
#define IOT_VALUE_TYPE_FLOAT           2
#define IOT_VALUE_TYPE_STRING          3
#define IOT_VALUE_TYPE_BINARY          4
#define IOT_VALUE_TYPE_HUNDREDTHS      5
#define IOT_VALUE_TYPE_CELSIUS         6
#define IOT_VALUE_TYPE_PERCENT_RH      7
#define IOT_VALUE_TYPE_KPA             8
#define IOT_VALUE_TYPE_ON_CONNECT      9

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

#define IOT_VALUE_TYPE_CELSIUS_NOT_RETAINED     _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_CELSIUS )
#define IOT_VALUE_TYPE_CELSIUS_RETAINED         _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_CELSIUS )

#define IOT_VALUE_TYPE_PERCENT_RH_NOT_RETAINED  _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_PERCENT_RH )
#define IOT_VALUE_TYPE_PERCENT_RH_RETAINED      _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_PERCENT_RH )

#define IOT_VALUE_TYPE_KPA_NOT_RETAINED         _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_KPA )
#define IOT_VALUE_TYPE_KPA_RETAINED             _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_KPA )

#define IOT_VALUE_TYPE_ON_CONNECT_NOT_RETAINED  _IOT_DEFINE_TYPE_1( IOT_VALUE_NOT_RETAINED, IOT_VALUE_TYPE_ON_CONNECT )
#define IOT_VALUE_TYPE_ON_CONNECT_RETAINED      _IOT_DEFINE_TYPE_1( IOT_VALUE_RETAINED,     IOT_VALUE_TYPE_ON_CONNECT )

#define _IOT_DEFINE_TYPE(r, t) IOT_VALUE_TYPE_ ## t ## _ ## r

#define IOT_SUB_DEFAULT_NAME ""
#define IOT_PUB_USE_ELEMENT ""

#define IOT_ELEMENT_TYPE_OTHER              0
#define IOT_ELEMENT_TYPE_SENSOR_TEMPERATURE 1
#define IOT_ELEMENT_TYPE_SENSOR_HUMIDITY    2
#define IOT_ELEMENT_TYPE_SENSOR_BINARY      3
#define IOT_ELEMENT_TYPE_SWITCH             4

#define IOT_DESCRIBE_PUB(retain, type, name) _IOT_DEFINE_TYPE(retain, type) name
#define IOT_PUB_DESCRIPTIONS(pubs...) { pubs }
#define IOT_DESCRIBE_SUB(type, name, _callback) { .type_name = _IOT_DEFINE_TYPE(NOT_RETAINED, type) name, .callback=_callback }
#define IOT_SUB_DESCRIPTIONS(subs...) { subs }

#define IOT_DESCRIBE_ELEMENT(name, element_type, pub_descriptions, sub_descriptions) \
    static const char * const name ## _pubs[] = pub_descriptions; \
    static const iotElementSubDescription_t name ## _subs[] = sub_descriptions; \
    static const iotElementDescription_t name = { \
        .type = element_type, \
        .pubs = name ## _pubs,\
        .nrofPubs = sizeof(name ## _pubs) / sizeof(char*),\
        .subs = name ## _subs,\
        .nrofSubs= sizeof(name ## _subs) / sizeof(iotElementSubDescription_t)\
        }

#define IOT_DESCRIBE_ELEMENT_NO_SUBS(name, element_type, pub_descriptions) \
    static const char *name ## _pubs[] = pub_descriptions; \
    static const iotElementDescription_t name = { \
        .type = element_type, \
        .pubs = name ## _pubs,\
        .nrofPubs = sizeof(name ## _pubs) / sizeof(char*),\
        .subs = NULL,\
        .nrofSubs= 0\
        }

#define IOT_ELEMENT_FLAGS_DONT_ANNOUNCE 1

#define IOT_ELEMENT_ITERATOR_START (NULL)

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

/** Creates a new IOT Element using the specified pub/sub description and printf formatting for the name.
 */
iotElement_t iotNewElement(const iotElementDescription_t *desc, uint32_t flags, void *userContext, const char *nameFormat, ...);

/** Publish a value to the specified element and pubId.
 */
void iotElementPublish(iotElement_t element, int pubId, iotValue_t value);

/** Convert a string value to a boolean
 * Accepts "on"/"off", "true"/"false" (ignoring case) and returns 0;
 * Invalid values return 1
 */
int iotStrToBool(const char *str, bool *out);

/** Publish a message to the MQTT server.
 * Returns a negative number on error.
 */
int iotMqttPublish(const char *topic, const char *data, int len, int qos, int retain);

/** Returns true if the device is currently connected to an MQTT server.
 */
bool iotMqttIsConnected(void);

/**
 * Allows iterating over the list of registered iotElements, optionally selecting only those that should be announced
 * (ie not system element).
 *
 * iterator should be initialised to IOT_ELEMENT_ITERATOR_START on the first call and will be updated after each call.
 * element will be updated with the next element in the iterator.
 * On successfully retrieving an element, true will be returned. On no more elements, false will be returned.
 */
bool iotElementIterate(iotElementIterator_t *iterator, bool onlyAnnounced, iotElement_t *nextElement);

/**
 * Get the iotDescription_t for the specified element.
 */
const iotElementDescription_t *iotElementGetDescription(iotElement_t element);

/**
 * Get the name of the element.
 */
char *iotElementGetName(iotElement_t element);

/**
 * Get Element base topic path.
 * buffer should be a pointer to a string buffer of length specified in bufferLne or NULL.
 * On return bufferLen will be updated to number of bytes used to hold the path.
 * The function will return a pointer to buffer on success or NULL if the buffer wasn't long enough with the size
 * required in bufferLen.
 */
char *iotElementGetBasePath(iotElement_t element, char *buffer, size_t *bufferLen);

/**
 * Retrieve the publish path for a specific pubId and element.
 * buffer should be a pointer to a string buffer of length specified in bufferLne or NULL.
 * On return bufferLen will be updated to number of bytes used to hold the path.
 * The function will return a pointer to buffer on success or NULL if the buffer wasn't long enough with the size
 * required in bufferLen.
 */
char *iotElementGetPubPath(iotElement_t element, int pubId, char *buffer, size_t *bufferLen);

/**
 * Retrieve the subscription path for a specific subId and element.
 * buffer should be a pointer to a string buffer of length specified in bufferLne or NULL.
 * On return bufferLen will be updated to number of bytes used to hold the path.
 * The function will return a pointer to buffer on success or NULL if the buffer wasn't long enough with the size
 * required in bufferLen.
 */
char *iotElementGetSubPath(iotElement_t element, int subId, char *buffer, size_t *bufferLen);

/**
 * Retrieve the publish name for a specific pubId and element.
 * The function will return a pointer to a zero terminated string on success or NULL if the pubId was invalid.
 */
const char *iotElementGetPubName(iotElement_t element, int pubId);

/**
 * Retrieve the subscription name for a specific subId and element.
 * The function will return a pointer to a zero terminated string on success or NULL if the subId was invalid.
 */
const char *iotElementGetSubName(iotElement_t element, int subId);
#endif
