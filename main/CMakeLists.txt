idf_build_get_property(project_ver PROJECT_VER)
configure_file(${COMPONENT_DIR}/version.c.in version.c)

idf_component_register(SRCS "homeassistant.c" "led_strips.c" "led.c" "switches.c" "profile.c" "user_main.c" "relays.c" "sensors.c" ${CMAKE_BINARY_DIR}/version.c
                    INCLUDE_DIRS ""
                    REQUIRES "json" "gpiox" "iotDevice" "iot" "switch" "humidityfan" "updater" 
                    "provisioning" "notifications" "deviceprofile" "logging" "i2cdev" "bmp280" 
                    "si7021" "dht" "ds18x20" "notificationled" "led_strip_spi" "draytonscr")