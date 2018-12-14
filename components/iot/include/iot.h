#ifndef _IOT_H_
#define _IOT_H_
#include <stdbool.h>

#define IOT_MAX_ELEMENT 4
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
    bool updateRequired;
}iotElementPub_t;

typedef struct {
    const char *name;
    iotElementSub_t subs[IOT_MAX_SUB];
    iotElementPub_t pubs[IOT_MAX_PUB];
    bool updateRequired;
}iotElement_t;

void iotInit(const char *roomPath);
void iotStart();

void iotElementAdd(const char *name, iotElement_t **ppElement);
void iotElememtSubAdd(iotElement_t *element, const char *name, enum iotValueType_e type, iotElementSubUpdateCallback_t callback, void *userData, iotElementSub_t **ppSub);
void iotElementPubAdd(iotElement_t *element, const char *name, enum iotValueType_e type, iotValue_t initial, iotElementPub_t **ppPub);
void iotElementPubUpdate(iotElementPub_t *pub, iotValue_t value);



#endif
