# homething 
*Internet of things for the home*

Homething is a firmware written for the ESP8266 to provide automation and monitoring for 'home' related tasks.
For example, homething provideds the ability to:
* control lights
* monitor temperature
* turn on a fan depending on humidity
* work as doorbell
* detection motion using PIR etc

Homething uses MQTT for control and status and can be updated over the air (OTA) using an HTTP server.

## Requirments
An ESP8266 board such as the NodeMCU.

ESP8266_ROTS_SDK (https://github.com/espressif/ESP8266_RTOS_SDK commit 91c7c1091154067822a74b824889277702e2dd88 onwards )
xtensa toolchain (see README in ESP8266_RTOS_SDK for details)

A wifi network
An MQTT server to connect the homething to.

