#include <esp_http_server.h>
#include "sdkconfig.h"
#include "provisioning_int.h"

static const char json[] = "{"
    "\"switch\":{"
        "\"pin\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"type\":{" 
            "\"type\":\"choice\""
            ",\"choices\":["
                "\"momemetary\""
                ",\"toggle\""
                ",\"onOff\""
                ",\"contact\""
                ",\"motion\""
            "]"
        "}"
        ",\"relay\":{" 
            "\"type\":\"id\""
            ",\"optional\":true"
        "}"
        ",\"icon\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
    ",\"relay\":{"
        "\"pin\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"level\":{" 
            "\"type\":\"gpioLevel\""
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#if defined(CONFIG_DHT22)
    ",\"dht22\":{"
        "\"pin\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#endif
#if defined(CONFIG_SI7021)
    ",\"si7021\":{"
        "\"sda\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"scl\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"addr\":{" 
            "\"type\":\"i2cAddr\""
            ",\"default\":64"
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#endif
#if defined(CONFIG_TSL2561)
    ",\"tsl2561\":{"
        "\"sda\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"scl\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"addr\":{" 
            "\"type\":\"i2cAddr\""
            ",\"default\":57"
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#endif
#if defined(CONFIG_BME280)
    ",\"bme280\":{"
        "\"sda\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"scl\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"addr\":{" 
            "\"type\":\"i2cAddr\""
            ",\"default\":118"
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#endif
#if defined(CONFIG_DS18x20)
    ",\"ds18x20\":{"
        "\"pin\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#endif
    ",\"led\":{"
        "\"pin\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
    ",\"led_strip_spi\":{"
        "\"numberOfLEDs\":{" 
            "\"type\":\"uint\""
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#if defined(CONFIG_DRAYTONSCR)
    ",\"draytonscr\":{"
        "\"pin\":{" 
            "\"type\":\"gpioPin\""
        "}"
        ",\"onCode\":{" 
            "\"type\":\"string\""
        "}"
        ",\"offCode\":{" 
            "\"type\":\"string\""
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#endif
#if defined(CONFIG_HUMIDISTAT)
    ",\"humidistat\":{"
        "\"sensor\":{" 
            "\"type\":\"id\""
        "}"
        ",\"relay\":{" 
            "\"type\":\"id\""
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#endif
#if defined(CONFIG_THERMOSTAT)
    ",\"thermostat\":{"
        "\"sensor\":{" 
            "\"type\":\"id\""
        "}"
        ",\"relay\":{" 
            "\"type\":\"id\""
        "}"
        ",\"name\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
        ",\"id\":{" 
            "\"type\":\"string\""
            ",\"optional\":true"
        "}"
    "}"
#endif
"}";

esp_err_t provisioningComponentsJsonFileHandler(httpd_req_t *req)
{
    provisioningSetContentType(req, CT_JSON);
    httpd_resp_send(req, json, sizeof(json) - 1);
    return ESP_OK;
}