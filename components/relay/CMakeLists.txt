idf_component_register(SRCS "lockout.c" "timeout.c" "relays.c" "relay.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "gpiox" "iot" "notifications")
