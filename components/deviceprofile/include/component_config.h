/* AUTO-GENERATED DO NOT EDIT */
#ifndef _COMPONENT_CONFIG_H_
#define _COMPONENT_CONFIG_H_
#include <stdint.h>
#include <stdbool.h>

/* Enum Mappings */
enum DeviceProfile_Choices_Switch_Type {
    DeviceProfile_Choices_Switch_Type_Momentary,
    DeviceProfile_Choices_Switch_Type_Toggle,
    DeviceProfile_Choices_Switch_Type_Onoff,
    DeviceProfile_Choices_Switch_Type_Contact,
    DeviceProfile_Choices_Switch_Type_Motion,
    DeviceProfile_Choices_Switch_Type_ChoiceCount
};

/* Config structures */
typedef struct DeviceProfile_SwitchConfig {
    uint8_t pin;
    enum DeviceProfile_Choices_Switch_Type type;
    char *relay;
    char *icon;
    uint32_t noiseFilter;
    char *name;
    char *id;
} DeviceProfile_SwitchConfig_t;

typedef struct DeviceProfile_RelayConfig {
    uint8_t pin;
    uint8_t level;
    char *name;
    char *id;
} DeviceProfile_RelayConfig_t;

typedef struct DeviceProfile_Dht22Config {
    uint8_t pin;
    char *name;
    char *id;
} DeviceProfile_Dht22Config_t;

typedef struct DeviceProfile_Si7021Config {
    uint8_t sda;
    uint8_t scl;
    uint8_t addr;
    char *name;
    char *id;
} DeviceProfile_Si7021Config_t;

typedef struct DeviceProfile_Tsl2561Config {
    uint8_t sda;
    uint8_t scl;
    uint8_t addr;
    char *name;
    char *id;
} DeviceProfile_Tsl2561Config_t;

typedef struct DeviceProfile_Bme280Config {
    uint8_t sda;
    uint8_t scl;
    uint8_t addr;
    char *name;
    char *id;
} DeviceProfile_Bme280Config_t;

typedef struct DeviceProfile_Ds18x20Config {
    uint8_t pin;
    float temperatureCorrection;
    char *name;
    char *id;
} DeviceProfile_Ds18x20Config_t;

typedef struct DeviceProfile_LedConfig {
    uint8_t pin;
    char *name;
    char *id;
} DeviceProfile_LedConfig_t;

typedef struct DeviceProfile_LedStripSpiConfig {
    uint32_t numberOfLEDs;
    char *name;
    char *id;
} DeviceProfile_LedStripSpiConfig_t;

typedef struct DeviceProfile_DraytonscrConfig {
    uint8_t pin;
    char *onCode;
    char *offCode;
    char *name;
    char *id;
} DeviceProfile_DraytonscrConfig_t;

typedef struct DeviceProfile_HumidistatConfig {
    char *sensor;
    char *relay;
    char *name;
    char *id;
} DeviceProfile_HumidistatConfig_t;

typedef struct DeviceProfile_ThermostatConfig {
    char *sensor;
    char *relay;
    char *name;
    char *id;
} DeviceProfile_ThermostatConfig_t;

typedef struct DeviceProfile_GpioxConfig {
    uint8_t sda;
    uint8_t scl;
    uint32_t number;
    char *name;
    char *id;
} DeviceProfile_GpioxConfig_t;

typedef struct DeviceProfile_RelayLockoutConfig {
    char *relay;
    char *name;
    char *id;
} DeviceProfile_RelayLockoutConfig_t;

typedef struct DeviceProfile_RelayTimeoutConfig {
    char *relay;
    uint32_t timeout;
    bool value;
    char *name;
    char *id;
} DeviceProfile_RelayTimeoutConfig_t;

typedef struct DeviceProfile_DeviceConfig {
    DeviceProfile_SwitchConfig_t *switchConfig;
    uint32_t switchCount;
    DeviceProfile_RelayConfig_t *relayConfig;
    uint32_t relayCount;
    DeviceProfile_Dht22Config_t *dht22Config;
    uint32_t dht22Count;
    DeviceProfile_Si7021Config_t *si7021Config;
    uint32_t si7021Count;
    DeviceProfile_Tsl2561Config_t *tsl2561Config;
    uint32_t tsl2561Count;
    DeviceProfile_Bme280Config_t *bme280Config;
    uint32_t bme280Count;
    DeviceProfile_Ds18x20Config_t *ds18x20Config;
    uint32_t ds18x20Count;
    DeviceProfile_LedConfig_t *ledConfig;
    uint32_t ledCount;
    DeviceProfile_LedStripSpiConfig_t *ledStripSpiConfig;
    uint32_t ledStripSpiCount;
    DeviceProfile_DraytonscrConfig_t *draytonscrConfig;
    uint32_t draytonscrCount;
    DeviceProfile_HumidistatConfig_t *humidistatConfig;
    uint32_t humidistatCount;
    DeviceProfile_ThermostatConfig_t *thermostatConfig;
    uint32_t thermostatCount;
    DeviceProfile_GpioxConfig_t *gpioxConfig;
    uint32_t gpioxCount;
    DeviceProfile_RelayLockoutConfig_t *relayLockoutConfig;
    uint32_t relayLockoutCount;
    DeviceProfile_RelayTimeoutConfig_t *relayTimeoutConfig;
    uint32_t relayTimeoutCount;
} DeviceProfile_DeviceConfig_t;
#endif
