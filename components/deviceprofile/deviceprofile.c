#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "utils.h"

#include "deviceprofile.h"

#define SUPPORTED_VERSION 1

static const char TAG[] = "devprofile";
static const char THING[] = "thing";
static const char PROFILE[] = "deviceprofile";
static uint8_t *deviceProfile = NULL;
static uint32_t deviceProfileLen = 0u;

static int nextUint32(CborValue *it, uint32_t *result);

int deviceProfileGetProfile(uint8_t **profile, size_t *profileLen)
{
    nvs_handle handle;
    esp_err_t err;
    if (deviceProfile == NULL) {
        err = nvs_open(THING, NVS_READONLY, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open thing section, err %d", err);
            return -1;
        }

        err = nvs_get_blob_alloc(handle, PROFILE, (void**)&deviceProfile, &deviceProfileLen);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read device profile, err %d", err);
        }

        nvs_close(handle);
        if (err!= ESP_OK) {
            return -1;
        }
    }
    *profile = deviceProfile;
    *profileLen = deviceProfileLen;
    return 0;
}

int deviceProfileSetProfile(const uint8_t *profile, size_t profileLen)
{
    nvs_handle handle;
    esp_err_t err;
    int ret = 0;
    err = nvs_open(THING, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open thing section, err %d", err);
        return -1;
    }
    err = nvs_set_blob(handle, PROFILE, profile, profileLen);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set profile blob, err %d", err);
        ret = -1;
    }
    nvs_close(handle);
    return ret;
}

int deviceProfileValidateProfile(const uint8_t *profile, size_t profileLen)
{
    DeviceProfile_Parser_t parser;
    if (deviceProfileParseProfile(profile, profileLen, &parser)) {
        return -1;
    }

    if (cbor_value_validate_basic(&parser.it) != CborNoError) {
        return -1;
    }
    return 0;
}

int deviceProfileParseProfile(const uint8_t *profile, size_t profileLen, DeviceProfile_Parser_t *parser)
{
    uint32_t version;

    if (cbor_parser_init(profile, profileLen, 0, &parser->parser, &parser->it) != CborNoError) {
        return -1;
    }
    if (cbor_value_get_type(&parser->it) != CborArrayType) {
        return -1;
    }

    cbor_value_enter_container(&parser->it, &parser->arrayIt);
    if (nextUint32(&parser->arrayIt, &version)) {
        ESP_LOGE(TAG, "Failed to get version");
        return -1;
    }

    if (version != SUPPORTED_VERSION) {
        ESP_LOGE(TAG, "Unsupported version: %d", version);
        return -1;
    }

    return 0;
}

int deviceProfileParserNextEntry(DeviceProfile_Parser_t *parser, CborValue *entry, DeviceProfile_EntryType_e *entryType)
{
    if (cbor_value_at_end(&parser->arrayIt)) {
        return -1;
    }

    if (cbor_value_get_type(&parser->arrayIt) != CborArrayType) {
        ESP_LOGE(TAG, "NextEntry: Not an array");
        return -1;
    }

    cbor_value_enter_container(&parser->arrayIt, entry);
    if (nextUint32(entry, entryType)) {
        return -1;
    }
    return 0;
}

int deviceProfileParserEntryGetUint32(CborValue *parserEntry, uint32_t *result)
{
    return nextUint32(parserEntry, result);
}

int deviceProfileParserEntryGetI2CDetails(CborValue *parserEntry, DeviceProfile_I2CDetails_t *details)
{
    uint32_t uint;
    if (nextUint32(parserEntry, &uint) == -1) {
        return -1;
    }
    details->sda = (uint8_t) uint;
    if (nextUint32(parserEntry, &uint) == -1) {
        return -1;
    }
    details->scl = (uint8_t) uint;
    if (nextUint32(parserEntry, &uint) == -1) {
        return -1;
    }
    details->addr = (uint8_t) uint;
    return 0;
}

int deviceProfileParserEntryGetStr(CborValue *parserEntry, char **str)
{
    if (cbor_value_at_end(parserEntry)) {
        return  -1;
    }
    if (!cbor_value_is_text_string(parserEntry)) {
        return -1;
    }
    CborValue next;
    size_t len;
    if (cbor_value_dup_text_string(parserEntry, str, &len, &next)) {
        return -1;
    }
    *parserEntry = next;
    return 0;
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

int deviceProfileParserCloseEntry(DeviceProfile_Parser_t *parser, CborValue *entry)
{
    while (!cbor_value_at_end(entry)) {
        cbor_value_advance(entry);
    }
    cbor_value_leave_container(&parser->arrayIt, entry);
    return 0;
}
