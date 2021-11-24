idf_component_register(SRCS "iot.c" "mqtt.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "wifi" "mqtt" "notifications" "utils") 
