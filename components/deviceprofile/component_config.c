/* AUTO-GENERATED DO NOT EDIT */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "cbor.h"
#include "component_config.h"

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
    for (i = 0; choices[i].str != NULL; i++){
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

static int validateAndSetI2CAddr(CborValue *value, struct field *field, void *output)
{
    uint32_t uintVal;
    if (validateAndSetUInt(value, field, &uintVal)) {
        return -1;
    }

    *((uint8_t*)output) = (uint8_t)uintVal;
    return 0;
}


static int validateAndSetInt(CborValue *value, struct field *field, void *output)
{
    if (!cbor_value_is_integer(value)) {
        return -1;
    }
    int64_t intVal;
    if (cbor_value_get_int64(value, &intVal) != CborNoError) {
        return -1;
    }
    *((int32_t*)output) = (int32_t)intVal;
    return 0;
}

static int validateAndSetBool(CborValue *value, struct field *field, void *output)
{
    if (!cbor_value_is_boolean(value)) {
        return -1;
    }
    
    if (cbor_value_get_boolean(value, output) != CborNoError){
        return -1;
    }
    return 0;
}

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
    { "momemetary", Choices_Switch_Type_Momemetary },
    { "toggle", Choices_Switch_Type_Toggle },
    { "onOff", Choices_Switch_Type_Onoff },
    { "contact", Choices_Switch_Type_Contact },
    { "motion", Choices_Switch_Type_Motion },
    { NULL, 0 }
};

struct field fields_Switch[] = {
    {
        .key = "pin",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Switch, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "type",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Switch, type),
        .choices = Choices_Switch_TypeStrings,
        .validateAndSet = validateAndSetChoice
    },
    {
        .key = "icon",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Switch, icon),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Switch, name),
        .validateAndSet = validateAndSetString
    },
};

/**** relay ****/
struct field fields_Relay[] = {
    {
        .key = "pin",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Relay, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "level",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Relay, level),
        .validateAndSet = validateAndSetGPIOLevel
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Relay, name),
        .validateAndSet = validateAndSetString
    },
};

/**** dht22 ****/
struct field fields_Dht22[] = {
    {
        .key = "pin",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Dht22, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Dht22, name),
        .validateAndSet = validateAndSetString
    },
};

/**** si7021 ****/
struct field fields_Si7021[] = {
    {
        .key = "sda",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Si7021, sda),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "scr",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Si7021, scr),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "addr",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Si7021, addr),
        .validateAndSet = validateAndSetI2CAddr
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Si7021, name),
        .validateAndSet = validateAndSetString
    },
};

/**** tsl2561 ****/
struct field fields_Tsl2561[] = {
    {
        .key = "sda",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Tsl2561, sda),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "scr",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Tsl2561, scr),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "addr",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Tsl2561, addr),
        .validateAndSet = validateAndSetI2CAddr
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Tsl2561, name),
        .validateAndSet = validateAndSetString
    },
};

/**** bme280 ****/
struct field fields_Bme280[] = {
    {
        .key = "sda",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Bme280, sda),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "scr",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Bme280, scr),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "addr",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Bme280, addr),
        .validateAndSet = validateAndSetI2CAddr
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Bme280, name),
        .validateAndSet = validateAndSetString
    },
};

/**** ds18x20 ****/
struct field fields_Ds18X20[] = {
    {
        .key = "pin",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Ds18X20, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Ds18X20, name),
        .validateAndSet = validateAndSetString
    },
};

/**** led ****/
struct field fields_Led[] = {
    {
        .key = "pin",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Led, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Led, name),
        .validateAndSet = validateAndSetString
    },
};

/**** led_strip_spi ****/
struct field fields_LedStripSpi[] = {
    {
        .key = "numberOfLEDs",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_LedStripSpi, numberOfLEDs),
        .validateAndSet = validateAndSetUInt
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_LedStripSpi, name),
        .validateAndSet = validateAndSetString
    },
};

/**** draytonscr ****/
struct field fields_Draytonscr[] = {
    {
        .key = "pin",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Draytonscr, pin),
        .validateAndSet = validateAndSetGPIOPin
    },
    {
        .key = "onCode",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Draytonscr, onCode),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "offCode",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Draytonscr, offCode),
        .validateAndSet = validateAndSetString
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Draytonscr, name),
        .validateAndSet = validateAndSetString
    },
};

/**** humidistat ****/
struct field fields_Humidistat[] = {
    {
        .key = "sensor",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Humidistat, sensor),
        .validateAndSet = NULL
    },
    {
        .key = "controller",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Humidistat, controller),
        .validateAndSet = NULL
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Humidistat, name),
        .validateAndSet = validateAndSetString
    },
};

/**** thermostat ****/
struct field fields_Thermostat[] = {
    {
        .key = "sensor",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Thermostat, sensor),
        .validateAndSet = NULL
    },
    {
        .key = "controller",  
        .flags =  FIELD_FLAG_DEFAULT, 
        .dataOffset = offsetof(struct Config_Thermostat, controller),
        .validateAndSet = NULL
    },
    {
        .key = "name",  
        .flags =  FIELD_FLAG_OPTIONAL, 
        .dataOffset = offsetof(struct Config_Thermostat, name),
        .validateAndSet = validateAndSetString
    },
};


struct component componentDefinitions[] = {
    {
        .name = "switch",
        .structSize = sizeof(struct Config_Switch),
        .arrayOffset = offsetof(struct DeviceConfig, switchConfig),
        .arrayCountOffset = offsetof(struct DeviceConfig, switchCount),
        .fields = fields_Switch,
        .fieldsCount = sizeof(fields_Switch) / sizeof(struct field)
    },
    {
        .name = "relay",
        .structSize = sizeof(struct Config_Relay),
        .arrayOffset = offsetof(struct DeviceConfig, relayConfig),
        .arrayCountOffset = offsetof(struct DeviceConfig, relayCount),
        .fields = fields_Relay,
        .fieldsCount = sizeof(fields_Relay) / sizeof(struct field)
    },
    {
        .name = "dht22",
        .structSize = sizeof(struct Config_Dht22),
        .arrayOffset = offsetof(struct DeviceConfig, dht22Config),
        .arrayCountOffset = offsetof(struct DeviceConfig, dht22Count),
        .fields = fields_Dht22,
        .fieldsCount = sizeof(fields_Dht22) / sizeof(struct field)
    },
    {
        .name = "si7021",
        .structSize = sizeof(struct Config_Si7021),
        .arrayOffset = offsetof(struct DeviceConfig, si7021Config),
        .arrayCountOffset = offsetof(struct DeviceConfig, si7021Count),
        .fields = fields_Si7021,
        .fieldsCount = sizeof(fields_Si7021) / sizeof(struct field)
    },
    {
        .name = "tsl2561",
        .structSize = sizeof(struct Config_Tsl2561),
        .arrayOffset = offsetof(struct DeviceConfig, tsl2561Config),
        .arrayCountOffset = offsetof(struct DeviceConfig, tsl2561Count),
        .fields = fields_Tsl2561,
        .fieldsCount = sizeof(fields_Tsl2561) / sizeof(struct field)
    },
    {
        .name = "bme280",
        .structSize = sizeof(struct Config_Bme280),
        .arrayOffset = offsetof(struct DeviceConfig, bme280Config),
        .arrayCountOffset = offsetof(struct DeviceConfig, bme280Count),
        .fields = fields_Bme280,
        .fieldsCount = sizeof(fields_Bme280) / sizeof(struct field)
    },
    {
        .name = "ds18x20",
        .structSize = sizeof(struct Config_Ds18X20),
        .arrayOffset = offsetof(struct DeviceConfig, ds18x20Config),
        .arrayCountOffset = offsetof(struct DeviceConfig, ds18x20Count),
        .fields = fields_Ds18X20,
        .fieldsCount = sizeof(fields_Ds18X20) / sizeof(struct field)
    },
    {
        .name = "led",
        .structSize = sizeof(struct Config_Led),
        .arrayOffset = offsetof(struct DeviceConfig, ledConfig),
        .arrayCountOffset = offsetof(struct DeviceConfig, ledCount),
        .fields = fields_Led,
        .fieldsCount = sizeof(fields_Led) / sizeof(struct field)
    },
    {
        .name = "led_strip_spi",
        .structSize = sizeof(struct Config_LedStripSpi),
        .arrayOffset = offsetof(struct DeviceConfig, ledStripSpiConfig),
        .arrayCountOffset = offsetof(struct DeviceConfig, ledStripSpiCount),
        .fields = fields_LedStripSpi,
        .fieldsCount = sizeof(fields_LedStripSpi) / sizeof(struct field)
    },
    {
        .name = "draytonscr",
        .structSize = sizeof(struct Config_Draytonscr),
        .arrayOffset = offsetof(struct DeviceConfig, draytonscrConfig),
        .arrayCountOffset = offsetof(struct DeviceConfig, draytonscrCount),
        .fields = fields_Draytonscr,
        .fieldsCount = sizeof(fields_Draytonscr) / sizeof(struct field)
    },
    {
        .name = "humidistat",
        .structSize = sizeof(struct Config_Humidistat),
        .arrayOffset = offsetof(struct DeviceConfig, humidistatConfig),
        .arrayCountOffset = offsetof(struct DeviceConfig, humidistatCount),
        .fields = fields_Humidistat,
        .fieldsCount = sizeof(fields_Humidistat) / sizeof(struct field)
    },
    {
        .name = "thermostat",
        .structSize = sizeof(struct Config_Thermostat),
        .arrayOffset = offsetof(struct DeviceConfig, thermostatConfig),
        .arrayCountOffset = offsetof(struct DeviceConfig, thermostatCount),
        .fields = fields_Thermostat,
        .fieldsCount = sizeof(fields_Thermostat) / sizeof(struct field)
    },
};

int deserializeComponent(struct component *componentDef, CborValue *map, void *structPtr)
{
    int i;
    for (i = 0; i < componentDef->fieldsCount; i++)
    {
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

int deserializeComponents(struct DeviceConfig *config, struct component *componentDef, CborValue *array) {
    CborValue iter;
    size_t len;
    void *structs = NULL, *current;
    int i;

    if (cbor_value_get_array_length(array, &len) != CborNoError) {
        return -1;
    }
    
    structs = calloc(len, componentDef->structSize);
    if (structs == NULL){
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

int DeviceProfile_deserialize(const uint8_t *profile, size_t profileLen, struct DeviceConfig *config)
{
    CborParser parser;
    CborValue root;
    int i;

    memset(config, 0, sizeof(struct DeviceConfig));

    if (cbor_parser_init(profile, profileLen, 0, &parser, &root) != CborNoError) {
        return -1;
    }

    if (cbor_value_get_type(&root) != CborMapType) {
        return -1;
    }

    for (i = 0; i < sizeof(componentDefinitions) / sizeof(struct component); i ++) {
        CborValue value;
        if (cbor_value_map_find_value(&root, componentDefinitions[i].name, &value) != CborNoError) {
            return -1;
        }
        if (cbor_value_get_type(&value) != CborArrayType) {
            return -1;
        }
        deserializeComponents(config, &componentDefinitions[i], &value);
    }
    return 0;
}