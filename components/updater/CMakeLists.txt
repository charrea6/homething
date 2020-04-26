idf_component_register(SRCS "download.c" "updater.c" ${CMAKE_BINARY_DIR}/version.c
                    INCLUDE_DIRS "include"
                    REQUIRES "iot" "app_update" "mbedtls")
