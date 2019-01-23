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

python2.7 and pyserial installed

A wifi network

An MQTT server to connect the homething to.

## How to build
After installing ESP8266_RTOS_SDK and the toolchain, ensure that the following shell environment variables are pointing to the correct locations:

export PATH=<location of toolchain>/bin/:$PATH
export IDF_PATH=<location of ESP8266_RTOS_SDK>

Next run `make menuconfig`

In serial flasher config, setup the port that you have connected your ESP8266 to and the details of the flash it uses.

In the IOT Configuration, set the Wifi network name (SSID), wifi password and the details of the MQTT server.

Next on the Thing Configuration page select what features this build will include (lights, temperature, motion etc)

In Partion Table, under Partition table select "Factory App, two OTA definitions" (this is currently tested only on a NodeMCU with > 1MB flash) 

Under Updater configuration, set the HTTP server address and port along with the path to use to download OTA updates.

Finally exit the configurtion, then type `make all`

Once the build has completed you can flash the ESP8266 and check that the build is working using `make flash monitor`

## MQTT Topics

TDB

