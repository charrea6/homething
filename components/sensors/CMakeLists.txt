idf_component_register(SRCS "sensors.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "iot" "notifications" "deviceprofile" "i2cdev" "bmp280" "si7021" "dht" "ds18x20")