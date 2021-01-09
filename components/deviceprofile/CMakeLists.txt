idf_component_register(SRCS "deviceprofile.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "nvs_flash" "tinycbor" "utils")