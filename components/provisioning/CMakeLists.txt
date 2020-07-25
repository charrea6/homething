idf_component_register(SRCS "provisioning.c" "captdns.c" "${CMAKE_BINARY_DIR}/static_files.c"
                    INCLUDE_DIRS "include" 
                    PRIV_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
                    REQUIRES "esp_http_server" "wifi" "iot") 

file(GLOB static_file_deps static/*)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/static_files.c
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/genstatic.py ${CMAKE_CURRENT_SOURCE_DIR}/static > ${CMAKE_BINARY_DIR}/static_files.c
    DEPENDS genstatic.py ${static_file_deps}
)