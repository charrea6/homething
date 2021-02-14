idf_component_register(SRCS "switches.c" "profile.c" "user_main.c" "relays.c" "sensors.c"
                    INCLUDE_DIRS ""
                    REQUIRES "gpiox" "iot" "switch" "humidityfan" "updater" "provisioning" "notifications" "deviceprofile" "logging" "i2cdev" "bmp280" "si7021" "dht" "ds18x20" "notificationled")