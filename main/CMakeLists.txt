idf_component_register(SRCS "switches.c" "profile.c" "user_main.c" "relays.c" "sensors.c"
                    INCLUDE_DIRS ""
                    REQUIRES "gpiox" "iot" "dht22" "switch" "humidityfan" "updater" "provisioning" "notifications" "deviceprofile" "i2cdev" "bmp280")