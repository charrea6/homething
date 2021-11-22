idf_component_register(SRCS "download.c" "updater.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "iot" "app_update" "mbedtls")
