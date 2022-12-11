#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "cJSON.h"
#include "component_config.h"
#include "field_types_used.h"
#define SUPPORTED_VERSION "1.0"
static const char TAG[] = "compcfg";

/* Enum Mappings */
struct choice {
	const char *str;
	int val;
};

enum field_flags {
    FIELD_FLAG_DEFAULT,
    FIELD_FLAG_OPTIONAL
};

struct field;

typedef int (*validateAndSet_t)(cJSON *, struct field *, void *);

struct field {
    char *key;
    enum field_flags flags;  
    size_t dataOffset;
    const struct choice *choices;
    validateAndSet_t validateAndSet;
};

struct component {
    char *name;
    size_t structSize;
    size_t arrayOffset;
    size_t arrayCountOffset;
    struct field *fields;
    size_t fieldsCount;
};

#ifdef FIELD_TYPE_USED_CHOICE
static int findChoice(const struct choice *choices, char *input, int *output) 
{
    int i;
    for (i = 0; choices[i].str != NULL; i++){
        if (strcmp(choices[i].str, input) == 0) {
            *output = choices[i].val;
            return 0;
        }
    }
    return -1;
}
static int validateAndSetChoice(cJSON *value, struct field *field, void *output)
{
    char *text = cJSON_GetStringValue(value);
    
    if (text == NULL) {
        return -1;
    }

    if (findChoice(field->choices, text, output)) {
        return -1;
    }
    return 0;
}
#endif

#ifdef FIELD_TYPE_USED_UINT
static int validateAndSetUInt(cJSON *value, struct field *field, void *output)
{
    if (!cJSON_IsNumber(value) || (value->valuedouble < 0)) {
        return -1;
    }

    *((uint32_t*)output) = (uint32_t)value->valuedouble;
    return 0;
}
#endif

#ifdef FIELD_TYPE_USED_INT
static int validateAndSetInt(cJSON *value, struct field *field, void *output)
{
    if (!cJSON_IsNumber(value)) {
        return -1;
    }

    *((int32_t*)output) = (int32_t)value->valuedouble;
    return 0;
}
#endif

#ifdef FIELD_TYPE_USED_GPIOPIN
static int validateAndSetGPIOPin(cJSON *value, struct field *field, void *output)
{
    uint32_t uintVal;
    if (validateAndSetUInt(value, field, &uintVal)) {
        return -1;
    }

    *((uint8_t*)output) = (uint8_t)uintVal;
    return 0;
}
#endif

#ifdef FIELD_TYPE_USED_GPIOLEVEL
static int validateAndSetGPIOLevel(cJSON *value, struct field *field, void *output)
{
    uint32_t uintVal;
    if (validateAndSetUInt(value, field, &uintVal)) {
        return -1;
    }

    *((uint8_t*)output) = (uint8_t)uintVal;
    return 0;
}
#endif

#ifdef FIELD_TYPE_USED_I2CADDR
static int validateAndSetI2CAddr(cJSON *value, struct field *field, void *output)
{
    uint32_t uintVal;
    if (validateAndSetUInt(value, field, &uintVal)) {
        return -1;
    }

    *((uint8_t*)output) = (uint8_t)uintVal;
    return 0;
}
#endif

#ifdef FIELD_TYPE_USED_BOOL
static int validateAndSetBool(cJSON *value, struct field *field, void *output)
{
    bool *outputBool = output;
    if (cJSON_IsTrue(value)) {
        *outputBool = true;
        return 0;
    }
    if (cJSON_IsFalse(value)) {
        *outputBool = false;
        return 0;
    }
    return -1;
}
#endif

#ifdef FIELD_TYPE_USED_STRING
static int validateAndSetString(cJSON *value, struct field *field, void *output)
{
    char **outputStr = output;
    char *valueStr = cJSON_GetStringValue(value);
    if (!cJSON_IsString(value)) {
        return -1;
    }
    *outputStr = strdup(valueStr);
    return 0;
}
#endif

#include "component_config_internal.h"

int deserializeComponent(struct component *componentDef, cJSON *object, void *structPtr)
{
    int i;
    for (i = 0; i < componentDef->fieldsCount; i++)
    {
        cJSON *value;
        value = cJSON_GetObjectItem(object, componentDef->fields[i].key);
        if (value == NULL) {
            if ((componentDef->fields[i].flags & FIELD_FLAG_OPTIONAL) != 0) {
                continue;
            }
            return -1;
        }
        if (componentDef->fields[i].validateAndSet(value, 
                                                   &componentDef->fields[i], 
                                                   structPtr + componentDef->fields[i].dataOffset)) {
            ESP_LOGI(TAG, "Component %s->%s failed validation", componentDef->name, componentDef->fields[i].key);
            return -1;
        }
    }
    return 0;
}

int deserializeComponents(struct DeviceProfile_DeviceConfig *config, struct component *componentDef, cJSON *array) {
    int i, len;
    void *structs = NULL, *current;

    len = cJSON_GetArraySize(array);
    
    structs = calloc(len, componentDef->structSize);
    if (structs == NULL){
        return -1;
    }
    current = structs;
    
    for (i = 0; i < len; i ++) {
        cJSON *object = cJSON_GetArrayItem(array, i);
        if ((object == NULL) || !cJSON_IsObject(object)) {
            goto error;
        }
        if (deserializeComponent(componentDef, object, current)) {
            goto error;
        }
        current += componentDef->structSize;
    }

    void **arrayEntry = ((void*)config) + componentDef->arrayOffset;
    size_t *arrayCount = ((void*)config) + componentDef->arrayCountOffset;
    *arrayEntry = structs;
    *arrayCount = (size_t)len;
    return 0;
error:
    ESP_LOGI(TAG, "Processing of %s failed", componentDef->name);
    free(structs);
    return -1;
}

static bool checkVersionSupported(cJSON *object)
{
    cJSON *versionObject = cJSON_GetObjectItem(object, "version");
    if (versionObject == NULL) {
        return false;
    }
    char *versionStr = cJSON_GetStringValue(versionObject);
    if (versionStr != NULL) {
        ESP_LOGI(TAG, "Profile version %s", versionStr);

        if (strcmp(versionStr, SUPPORTED_VERSION) == 0){
            return true;
        }
    }
    return false;
}

int deviceProfileDeserialize(const char *profile, struct DeviceProfile_DeviceConfig *config)
{
    int i;
    cJSON *object, *components;
    memset(config, 0, sizeof(struct DeviceProfile_DeviceConfig));

    object = cJSON_Parse(profile);
    if (object == NULL) {
        ESP_LOGE(TAG, "Failed to parse json");
        return -1;
    }

    if (!checkVersionSupported(object)) {
        return -1;
    }

    components = cJSON_GetObjectItem(object, "components");
    if ((components == NULL) || (!cJSON_IsObject(components))) {
        ESP_LOGE(TAG, "No components section");
        return -1;
    }

    for (i = 0; i < sizeof(componentDefinitions) / sizeof(struct component); i ++) {
        cJSON *componentArray = cJSON_GetObjectItem(components, componentDefinitions[i].name);
        if (componentArray == NULL) {
            continue;
        }
        if (!cJSON_IsArray(componentArray)) {
            ESP_LOGE(TAG, "Component %s incorrectly formatted", componentDefinitions[i].name);
            continue;
        }
        deserializeComponents(config, &componentDefinitions[i], componentArray);
    }
    return 0;
}