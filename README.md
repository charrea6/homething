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

ESP8266_RTOS_SDK (https://github.com/espressif/ESP8266_RTOS_SDK commit be7e3fd760297cc85ec139906853cf344b5e96a9 onwards )

xtensa toolchain (see README in ESP8266_RTOS_SDK for details)

python and all modules from requirements.txt installed.

A wifi network

An MQTT server to connect the homething to.

## How to build
After installing ESP8266_RTOS_SDK and the toolchain, ensure that the following shell environment variables are pointing to the correct locations:

    export PATH=<location of toolchain>/bin/:$PATH
    export IDF_PATH=<location of ESP8266_RTOS_SDK>

Then run `git submodule init --recursive` to ensure that all git submodules are cloned/initialised correctly.

Homething uses cmake for building and the simplest way to build and flash is to use idf.py (from the ESP8266_RTOS_SDK, tools/idf.py).

First run `idf.py menuconfig` to set the serial port, under Serial Flasher config. 
Then set the partition table to "Factory App, two OTA definitions" under "Partition Table".
On the "Thing Configuration" page select what features this build will include (lights, temperature, motion etc)

Under Updater configuration, set the HTTP server address and port along with the path to use to download OTA updates.

Finally exit the configurtion and run `idf.py build` to actually build the project.

Once the build has completed you can flash the ESP8266 using `idf.py flash` and check that the build is working using `idf.py monitor`

## Configuring your device
To configure your device first create an ini file that contains at least the details of the devices connected to your thing, see INI Config File section below.

Once you've created that file flash it your ESP8266 using the idf.py command flashconfig

    idf.py -p /dev/ttyUSB0 flashconfig myconfig.ini

## INI Config File
To configure your device first create an ini file that contains at least a [thing] section.
This section describes how many lights, doorbells, temperature sensors, humidity fans and motion sensors 
the device should control.
ie
    [thing]
    lights=X
    fans=Y
    doorbells=Z
    temperaturesensors=A
    motionsensors=B

The other setting that is included in the thing section is the `relayOnLevel` which describes what TTL level should be used to turn on any relays that are connected to your device, use 0 for low and 1 for high.

### Lights
Each light should be configure using a [lightN] where N is a number from 0 to number of lights - 1.
For example if the device is controlling 7 lights, the first light will be named light0 and the last light6.
The light secion has the following setting that need to be defined:

setting | description
--------|------------
switch  | GPIO pin that is used to connect to the light swith
relay   | GPIO pin that is connected to the relay

### Doorbell
_TODO_

### Motion Sensors
_TODO_

### Temperature Sensors
_TODO_

### Humidity Fans
_TODO_

## MQTT Topics

Homething publishes and subscribes to topics under the prefix "homething/<mac>", where <mac> is 12 character unique MAC of the device.

Under this prefix the following topics will always be published:
Topic         | Details 
--------------|---------
device/ip     | IP Address of the device
device/uptime | Number of seconds this device has been running for.
sw/version    | Running software version.
sw/profile    | Build profile, what devices are supported by the running software.
sw/status     | Used to notify of software update status.

The following subscriptions are also always available:
Topic     | Details 
----------|---------
sw/update | Used to perform an Over The Air (OTA) update, simple send the directory name, on the web server set when configuring, containing the OTA files to update to.

### Lights
_TODO_

### Doorbell
_TODO_

### Motion Sensors
_TODO_

### Temperature Sensors
_TODO_

### Humidity Fans
_TODO_

## Icon

FavIcon from https://www.pngkit.com/view/u2w7r5q8r5o0o0r5_free-sweet-icons-easy-home-icon-blue-png/
