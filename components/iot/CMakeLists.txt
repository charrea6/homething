idf_component_register(SRCS "iot.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "tcpip_adapter" "nvs_flash" "mqtt") 
