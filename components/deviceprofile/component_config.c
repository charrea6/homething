/* AUTO-GENERATED DO NOT EDIT */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "cbor.h"
#include "component_config.h"

#define SUPPORTED_VERSION 2
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

typedef int (*validateAndSet_t)(CborValue *, struct field *, void *);

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

static int findChoice(const struct choice *choices, char *input, int *output)
{
    int i;
    for (i = 0; choices[i].str != NULL; i++) {
        if (strcmp(choices[i].str, input) == 0) {
            *output = choices[i].val;
            return 0;
        }
    }
    return -1;
}
static int validateAndSetChoice(CborValue *value, struct field *field, void *output)
{
    char *text = NULL;
    size_t textLen = 0;
    if (!cbor_value_is_text_string(value)) {
        return -1;
    }
    if (cbor_value_dup_text_string(value, &text, &textLen, NULL) != CborNoError) {
        return -1;
    }

    if (findChoice(field->choices, text, output)) {
        free(text);
        return -1;
    }
    free(text);
    return 0;
}

static int validateAndSetUInt(CborValue *value, struct field *field, void *output)
{
    if (!cbor_value_is_unsigned_integer(value)) {
        return -1;
    }
    uint64_t uintVal;
    if (cbor_value_get_uint64(value, &uintVal) != CborNoError) {
        return -1;
    }
    *((uint32_t*)output) = (uint32_t)uintVal;
    return 0;
}

static int validateAndSetGPIOPin(CborValue *value, struct field *field, void *output)
{
    uint32_t uintVal;
    if (validateAndSetUInt(value, field, &uintVal)) {
        return -1;
    }

    *((uint8_t*)output) = (uint8_t)uintVal;
    return 0;
}

static int validateAndSetGPIOLevel(CborValue *value, struct field *field, void *output)
{
    uint32_t uintVal;
    if (validateAndSetUInt(value, field, &uintVal)) {
        return -1;
    }

    *((uint8_t*)output) = (uint8_t)uintVal;
    return 0;
}

#if (defined(CONFIG_SI7021)) || (defined(CONFIG_TSL2561)) || (defined(CONFIG_BME280))
static int validateAndSetI2CAddr(CborValue *value, struct field *field, void *output)
{
    uint32_t uintVal;
    if (validateAndSetUInt(value, field, &uintVal)) {
        return -1;
    }

    *((uint8_t*)output) = (uint8_t)uintVal;
    return 0;
}
#endif



static int validateAndSetString(CborValue *value, struct field *field, void *output)
{
    if (!cbor_value_is_text_string(value)) {
        return -1;
    }
    size_t textLen;
    if (cbor_value_dup_text_string(value, output, &textLen, NULL) != CborNoError) {
        return -1;
    }
    return 0;
}

/**** switch ****/
static const struct choice Choices_Switch_TypeStrings[] = {
    { "momemetary", DeviceProfile_Choices_Switch_Type_Momemetary },
    { "toggle", DeviceProfile_Choices_Switch_Type_Toggle },
    { "onOff", DeviceProfile_Choices_Switch_Type_Onoff },
    { "contact", DeviceProfile_Choices_Switch_Type_Contact },
    { "motion", DeviceProfile_Choices_Switch_Type_Motion },
    { NULL, 0 }
};

struct field fields_Switch[] = {
    {
        .key = "pin",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_SwitchConfig, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "type",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_SwitchConfig, type),
        .choices = Choices_Switch_TypeStrings,
        .validateAndSet = validateAndSetChoice
    },
    {
        .key = "relay",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_SwitchConfig, relay),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "icon",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_SwitchConfig, icon),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_SwitchConfig, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_SwitchConfig, id),
        .validateAndSet = validateAndSetString
    },
};
/**** relay ****/
struct field fields_Relay[] = {
    {
        .key = "pin",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_RelayConfig, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "level",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_RelayConfig, level),
        .validateAndSet = validateAndSetGPIOLevel
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_RelayConfig, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_RelayConfig, id),
        .validateAndSet = validateAndSetString
    },
};
/**** dht22 ****/
#if defined(CONFIG_DHT22)
struct field fields_Dht22[] = {
    {
        .key = "pin",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Dht22Config, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Dht22Config, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Dht22Config, id),
        .validateAndSet = validateAndSetString
    },
};
#endif
/**** si7021 ****/
#if defined(CONFIG_SI7021)
struct field fields_Si7021[] = {
    {
        .key = "sda",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Si7021Config, sda),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "scl",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Si7021Config, scl),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "addr",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Si7021Config, addr),
        .validateAndSet = validateAndSetI2CAddr
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Si7021Config, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Si7021Config, id),
        .validateAndSet = validateAndSetString
    },
};
#endif
/**** tsl2561 ****/
#if defined(CONFIG_TSL2561)
struct field fields_Tsl2561[] = {
    {
        .key = "sda",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Tsl2561Config, sda),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "scl",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Tsl2561Config, scl),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "addr",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Tsl2561Config, addr),
        .validateAndSet = validateAndSetI2CAddr
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Tsl2561Config, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Tsl2561Config, id),
        .validateAndSet = validateAndSetString
    },
};
#endif
/**** bme280 ****/
#if defined(CONFIG_BME280)
struct field fields_Bme280[] = {
    {
        .key = "sda",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Bme280Config, sda),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "scl",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Bme280Config, scl),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "addr",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Bme280Config, addr),
        .validateAndSet = validateAndSetI2CAddr
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Bme280Config, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Bme280Config, id),
        .validateAndSet = validateAndSetString
    },
};
#endif
/**** ds18x20 ****/
#if defined(CONFIG_DS18x20)
struct field fields_Ds18x20[] = {
    {
        .key = "pin",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_Ds18x20Config, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Ds18x20Config, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_Ds18x20Config, id),
        .validateAndSet = validateAndSetString
    },
};
#endif
/**** led ****/
struct field fields_Led[] = {
    {
        .key = "pin",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_LedConfig, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_LedConfig, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_LedConfig, id),
        .validateAndSet = validateAndSetString
    },
};
/**** led_strip_spi ****/
struct field fields_LedStripSpi[] = {
    {
        .key = "numberOfLEDs",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_LedStripSpiConfig, numberOfLEDs),
        .validateAndSet = validateAndSetUInt
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_LedStripSpiConfig, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_LedStripSpiConfig, id),
        .validateAndSet = validateAndSetString
    },
};
/**** draytonscr ****/
#if defined(CONFIG_DRAYTONSCR)
struct field fields_Draytonscr[] = {
    {
        .key = "pin",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_DraytonscrConfig, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "onCode",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_DraytonscrConfig, onCode),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "offCode",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_DraytonscrConfig, offCode),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_DraytonscrConfig, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_DraytonscrConfig, id),
        .validateAndSet = validateAndSetString
    },
};
#endif
/**** humidistat ****/
#if defined(CONFIG_HUMIDISTAT)
struct field fields_Humidistat[] = {
    {
        .key = "sensor",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_HumidistatConfig, sensor),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "relay",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_HumidistatConfig, relay),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_HumidistatConfig, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_HumidistatConfig, id),
        .validateAndSet = validateAndSetString
    },
};
#endif
/**** thermostat ****/
#if defined(CONFIG_THERMOSTAT)
struct field fields_Thermostat[] = {
    {
        .key = "sensor",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_ThermostatConfig, sensor),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "relay",
        .flags =  FIELD_FLAG_DEFAULT,
        .dataOffset = offsetof(struct DeviceProfile_ThermostatConfig, relay),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "name",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_ThermostatConfig, name),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "id",
        .flags =  FIELD_FLAG_OPTIONAL,
        .dataOffset = offsetof(struct DeviceProfile_ThermostatConfig, id),
        .validateAndSet = validateAndSetString
    },
};
#endif

struct component componentDefinitions[] = {
    {
        .name = "switch",
        .structSize = sizeof(struct DeviceProfile_SwitchConfig),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, switchConfig),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, switchCount),
        .fields = fields_Switch,
        .fieldsCount = sizeof(fields_Switch) / sizeof(struct field)
    },
    {
        .name = "relay",
        .structSize = sizeof(struct DeviceProfile_RelayConfig),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, relayConfig),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, relayCount),
        .fields = fields_Relay,
        .fieldsCount = sizeof(fields_Relay) / sizeof(struct field)
    },
#if defined(CONFIG_DHT22)
    {
        .name = "dht22",
        .structSize = sizeof(struct DeviceProfile_Dht22Config),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, dht22Config),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, dht22Count),
        .fields = fields_Dht22,
        .fieldsCount = sizeof(fields_Dht22) / sizeof(struct field)
    },
#endif
#if defined(CONFIG_SI7021)
    {
        .name = "si7021",
        .structSize = sizeof(struct DeviceProfile_Si7021Config),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, si7021Config),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, si7021Count),
        .fields = fields_Si7021,
        .fieldsCount = sizeof(fields_Si7021) / sizeof(struct field)
    },
#endif
#if defined(CONFIG_TSL2561)
    {
        .name = "tsl2561",
        .structSize = sizeof(struct DeviceProfile_Tsl2561Config),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, tsl2561Config),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, tsl2561Count),
        .fields = fields_Tsl2561,
        .fieldsCount = sizeof(fields_Tsl2561) / sizeof(struct field)
    },
#endif
#if defined(CONFIG_BME280)
    {
        .name = "bme280",
        .structSize = sizeof(struct DeviceProfile_Bme280Config),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, bme280Config),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, bme280Count),
        .fields = fields_Bme280,
        .fieldsCount = sizeof(fields_Bme280) / sizeof(struct field)
    },
#endif
#if defined(CONFIG_DS18x20)
    {
        .name = "ds18x20",
        .structSize = sizeof(struct DeviceProfile_Ds18x20Config),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, ds18x20Config),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, ds18x20Count),
        .fields = fields_Ds18x20,
        .fieldsCount = sizeof(fields_Ds18x20) / sizeof(struct field)
    },
#endif
    {
        .name = "led",
        .structSize = sizeof(struct DeviceProfile_LedConfig),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, ledConfig),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, ledCount),
        .fields = fields_Led,
        .fieldsCount = sizeof(fields_Led) / sizeof(struct field)
    },
    {
        .name = "led_strip_spi",
        .structSize = sizeof(struct DeviceProfile_LedStripSpiConfig),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, ledStripSpiConfig),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, ledStripSpiCount),
        .fields = fields_LedStripSpi,
        .fieldsCount = sizeof(fields_LedStripSpi) / sizeof(struct field)
    },
#if defined(CONFIG_DRAYTONSCR)
    {
        .name = "draytonscr",
        .structSize = sizeof(struct DeviceProfile_DraytonscrConfig),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, draytonscrConfig),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, draytonscrCount),
        .fields = fields_Draytonscr,
        .fieldsCount = sizeof(fields_Draytonscr) / sizeof(struct field)
    },
#endif
#if defined(CONFIG_HUMIDISTAT)
    {
        .name = "humidistat",
        .structSize = sizeof(struct DeviceProfile_HumidistatConfig),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, humidistatConfig),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, humidistatCount),
        .fields = fields_Humidistat,
        .fieldsCount = sizeof(fields_Humidistat) / sizeof(struct field)
    },
#endif
#if defined(CONFIG_THERMOSTAT)
    {
        .name = "thermostat",
        .structSize = sizeof(struct DeviceProfile_ThermostatConfig),
        .arrayOffset = offsetof(struct DeviceProfile_DeviceConfig, thermostatConfig),
        .arrayCountOffset = offsetof(struct DeviceProfile_DeviceConfig, thermostatCount),
        .fields = fields_Thermostat,
        .fieldsCount = sizeof(fields_Thermostat) / sizeof(struct field)
    },
#endif
};

int deserializeComponent(struct component *componentDef, CborValue *map, void *structPtr)
{
    int i;
    for (i = 0; i < componentDef->fieldsCount; i++) {
        CborValue value;
        if (cbor_value_map_find_value(map, componentDef->fields[i].key, &value) != CborNoError) {
            return -1;
        }
        if (!cbor_value_is_valid(&value)) {
            if (componentDef->fields[i].flags == FIELD_FLAG_OPTIONAL) {
                continue;
            } else {
                return -1;
            }
        }

        if (componentDef->fields[i].validateAndSet(&value,
                &componentDef->fields[i],
                structPtr + componentDef->fields[i].dataOffset)) {
            return -1;
        }
    }
    return 0;
}

int deserializeComponents(struct DeviceProfile_DeviceConfig *config, struct component *componentDef, CborValue *array)
{
    CborValue iter;
    size_t len;
    void *structs = NULL, *current;
    int i;

    if (cbor_value_get_array_length(array, &len) != CborNoError) {
        return -1;
    }

    structs = calloc(len, componentDef->structSize);
    if (structs == NULL) {
        return -1;
    }
    current = structs;
    cbor_value_enter_container(array, &iter);
    for (i = 0; i < len; i ++) {
        if (cbor_value_get_type(&iter) != CborMapType) {
            goto error;
        }

        if (deserializeComponent(componentDef, &iter, current)) {
            goto error;
        }
        cbor_value_advance(&iter);
        current += componentDef->structSize;
    }

    void **arrayEntry = ((void*)config) + componentDef->arrayOffset;
    size_t *arrayCount = ((void*)config) + componentDef->arrayCountOffset;
    *arrayEntry = structs;
    *arrayCount = len;
    return 0;
error:
    free(structs);
    return -1;
}

static int nextUint32(CborValue *it, uint32_t *result)
{
    uint64_t uintValue;
    if (cbor_value_at_end(it)) {
        return  -1;
    }
    if (cbor_value_is_unsigned_integer(it)) {
        if (cbor_value_get_uint64(it, &uintValue) != CborNoError) {
            return -1;
        }
        *result = (uint32_t)uintValue;
        cbor_value_advance(it);
    } else {
        return -1;
    }
    return 0;
}


int deviceProfileDeserialize(const uint8_t *profile, size_t profileLen, struct DeviceProfile_DeviceConfig *config)
{
    CborParser parser;
    CborValue root, array;
    int i;
    uint32_t version;

    memset(config, 0, sizeof(struct DeviceProfile_DeviceConfig));

    if (cbor_parser_init(profile, profileLen, 0, &parser, &root) != CborNoError) {
        ESP_LOGE(TAG, "Failed to init cbor parser");
        return -1;
    }

    if (cbor_value_get_type(&root) != CborArrayType) {
        ESP_LOGE(TAG, "Incorrect root element type, %d expected %d", cbor_value_get_type(&root), CborArrayType);
        return -1;
    }

    cbor_value_enter_container(&root, &array);
    if (nextUint32(&array, &version)) {
        ESP_LOGE(TAG, "Failed to get version");
        return -1;
    }

    if (version != SUPPORTED_VERSION) {
        ESP_LOGE(TAG, "Unsupported version: %d", version);
        return -1;
    }

    ESP_LOGI(TAG, "Profile version %u", version);

    if (cbor_value_get_type(&array) != CborMapType) {
        ESP_LOGE(TAG, "Incorrect element type, %d expected %d", cbor_value_get_type(&array), CborMapType);
        return -1;
    }

    for (i = 0; i < sizeof(componentDefinitions) / sizeof(struct component); i ++) {
        CborValue value;
        CborError err = cbor_value_map_find_value(&array, componentDefinitions[i].name, &value);

        if (err!= CborNoError) {
            ESP_LOGE(TAG, "Find value returned error, %d", err);
            return -1;
        }

        if (cbor_value_get_type(&value) == CborArrayType) {
            deserializeComponents(config, &componentDefinitions[i], &value);
        }
    }
    return 0;
}