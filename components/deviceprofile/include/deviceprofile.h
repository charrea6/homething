#ifndef _DEVICEPROFILE_H_
#define _DEVICEPROFILE_H_

#include <stddef.h>
#include <stdint.h>
#include "cbor.h"

typedef enum {
    DeviceProfile_EntryType_GPIOSwitch = 0,
    DeviceProfile_EntryType_Relay,
    DeviceProfile_EntryType_DHT22,
    DeviceProfile_EntryType_SI7021,
    DeviceProfile_EntryType_TSL2561,
    DeviceProfile_EntryType_BME280,
    DeviceProfile_EntryType_DS18B20,
    DeviceProfile_EntryType_LED,
    DeviceProfile_EntryType_LEDStripSPI,
    DeviceProfile_EntryType_Max
} DeviceProfile_EntryType_e;

typedef enum {
    DeviceProfile_SwitchType_Toggle = 0,
    DeviceProfile_SwitchType_OnOff,
    DeviceProfile_SwitchType_Momentary,
    DeviceProfile_SwitchType_Motion,
    DeviceProfile_SwitchType_Contact,
    DeviceProfile_SwitchType_Max
} DeviceProfile_SwitchType_e;

typedef enum {
    DeviceProfile_RelayController_None = 0,
    DeviceProfile_RelayController_Switch,
    DeviceProfile_RelayController_Temperature,
    DeviceProfile_RelayController_Humidity,
    DeviceProfile_RelayController_Max
} DeviceProfile_RelayController_e;

typedef struct {
    uint8_t sda;
    uint8_t scl;
    uint8_t addr;
} DeviceProfile_I2CDetails_t;

typedef struct {
    CborParser parser;
    CborValue it;
    CborValue arrayIt;
} DeviceProfile_Parser_t;


int deviceProfileGetProfile(uint8_t **profile, size_t *profileLen);
int deviceProfileSetProfile(const uint8_t *profile, size_t profileLen);
int deviceProfileValidateProfile(const uint8_t *profile, size_t profileLen);

int deviceProfileParseProfile(const uint8_t *profile, size_t profileLen, DeviceProfile_Parser_t *parser);
int deviceProfileParserNextEntry(DeviceProfile_Parser_t *parser, CborValue *entry, DeviceProfile_EntryType_e *entryType);
int deviceProfileParserCloseEntry(DeviceProfile_Parser_t *parser, CborValue *entry);

int deviceProfileParserEntryGetUint32(CborValue *parserEntry, uint32_t *result);
int deviceProfileParserEntryGetI2CDetails(CborValue *parserEntry, DeviceProfile_I2CDetails_t *details);

#endif
