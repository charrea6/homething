idf_component_register(SRCS "gpiox.c"
                    INCLUDE_DIRS "include" 
                    REQUIRES "nvs_flash" "i2cdev" "pcf8574" )
