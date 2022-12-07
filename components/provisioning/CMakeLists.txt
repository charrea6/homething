idf_component_register(SRCS "config.c" "provisioning.c" "${CMAKE_BINARY_DIR}/static_files.c" "components_json.c"
                    INCLUDE_DIRS "include" 
                    PRIV_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}
                    REQUIRES "esp_http_server" "json" "wifi" "iot" "uzlib" "tinycbor" "utils") 

file(GLOB static_file_deps static/*)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/static_files.c
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/genstatic.py ${CMAKE_CURRENT_SOURCE_DIR}/static ${CMAKE_BINARY_DIR}/settings.js -o ${CMAKE_BINARY_DIR}/static_files.c -c ${CMAKE_CURRENT_SOURCE_DIR}/static.min
    DEPENDS genstatic.py ${static_file_deps} ${CMAKE_BINARY_DIR}/settings.js
)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/settings.h ${CMAKE_BINARY_DIR}/settings.js
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/gensettings.py ${CMAKE_BINARY_DIR}/config/sdkconfig.json ${CMAKE_CURRENT_SOURCE_DIR}/settings.yml ${CMAKE_BINARY_DIR}/provisioningSettings.h ${CMAKE_BINARY_DIR}/settings.js
    DEPENDS gensettings.py settings.yml ${CMAKE_BINARY_DIR}/config/sdkconfig.json)