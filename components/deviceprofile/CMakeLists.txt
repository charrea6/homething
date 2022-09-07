idf_component_register(SRCS "deviceprofile.c" "component_config.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "nvs_flash" "tinycbor" "utils")