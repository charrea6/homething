idf_component_register(SRCS "user_main.c"
                    INCLUDE_DIRS ""
                    REQUIRES "gpiox" "iot" "dht22" "switch" "humidityfan" "light" "updater" "motion" "doorbell" "provisioning")