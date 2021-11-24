idf_component_register(SRCS "iotDevice.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "wifi" "nvs_flash" "iot" "deviceprofile" "tinycbor" "notifications" "utils") 
