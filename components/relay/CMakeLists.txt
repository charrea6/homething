idf_component_register(SRCS "relays.c" "relay.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "gpiox" "iot" "notifications")
