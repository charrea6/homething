/* AUTO-GENERATED DO NOT EDIT */
#ifndef _COMPONENT_CONFIG_H_
#define _COMPONENT_CONFIG_H_
#include <stdint.h>

/* Enum Mappings */
enum Choices_Switch_Type {
    Choices_Switch_Type_Momemetary,
    Choices_Switch_Type_Toggle,
    Choices_Switch_Type_Onoff,
    Choices_Switch_Type_Contact,
    Choices_Switch_Type_Motion,
};

/* Config structures */
struct Config_Switch {
    uint8_t pin;
    enum Choices_Switch_Type type;
    char *icon;
    char *name;
};

struct Config_Relay {
    uint8_t pin;
    uint8_t level;
    char *name;
};

struct Config_Dht22 {
    uint8_t pin;
    char *name;
};

struct Config_Si7021 {
    uint8_t sda;
    uint8_t scr;
    uint8_t addr;
    char *name;
};

struct Config_Tsl2561 {
    uint8_t sda;
    uint8_t scr;
    uint8_t addr;
    char *name;
};

struct Config_Bme280 {
    uint8_t sda;
    uint8_t scr;
    uint8_t addr;
    char *name;
};

struct Config_Ds18X20 {
    uint8_t pin;
    char *name;
};

struct Config_Led {
    uint8_t pin;
    char *name;
};

struct Config_LedStripSpi {
    uint32_t numberOfLEDs;
    char *name;
};

struct Config_Draytonscr {
    uint8_t pin;
    char *onCode;
    char *offCode;
    char *name;
};

struct Config_Humidistat {
    char *sensor;
    char *controller;
    char *name;
};

struct Config_Thermostat {
    char *sensor;
    char *controller;
    char *name;
};

struct DeviceConfig {
    struct Config_Switch *switchConfig;
    uint32_t switchCount;
    struct Config_Relay *relayConfig;
    uint32_t relayCount;
    struct Config_Dht22 *dht22Config;
    uint32_t dht22Count;
    struct Config_Si7021 *si7021Config;
    uint32_t si7021Count;
    struct Config_Tsl2561 *tsl2561Config;
    uint32_t tsl2561Count;
    struct Config_Bme280 *bme280Config;
    uint32_t bme280Count;
    struct Config_Ds18X20 *ds18x20Config;
    uint32_t ds18x20Count;
    struct Config_Led *ledConfig;
    uint32_t ledCount;
    struct Config_LedStripSpi *ledStripSpiConfig;
    uint32_t ledStripSpiCount;
    struct Config_Draytonscr *draytonscrConfig;
    uint32_t draytonscrCount;
    struct Config_Humidistat *humidistatConfig;
    uint32_t humidistatCount;
    struct Config_Thermostat *thermostatConfig;
    uint32_t thermostatCount;
};
#endif
