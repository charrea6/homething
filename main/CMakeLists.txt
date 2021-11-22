idf_component_register(SRCS "homeassistant.c" "led_strips.c" "led.c" "switches.c" "profile.c" "user_main.c" "relays.c" "sensors.c"
                    INCLUDE_DIRS ""
                    REQUIRES "json" "gpiox" "iot" "switch" "humidityfan" "updater" "provisioning" "notifications" "deviceprofile" "logging" "i2cdev" "bmp280" "si7021" "dht" "ds18x20" "notificationled" "led_strip_spi")