idf_component_register(SRCS "iot.c" "mqtt.c" "device.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "wifi" "nvs_flash" "mqtt" "deviceprofile" "tinycbor" "notifications" "utils") 
