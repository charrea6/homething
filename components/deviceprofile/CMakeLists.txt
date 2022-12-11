idf_component_register(SRCS "deviceprofile.c" "deserialize.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "nvs_flash" "json" "utils")