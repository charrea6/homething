idf_component_register(SRCS "libyaml/src/api.c"
                            "libyaml/src/dumper.c"
                            "libyaml/src/emitter.c"
                            "libyaml/src/loader.c"
                            "libyaml/src/parser.c"
                            "libyaml/src/reader.c"
                            "libyaml/src/scanner.c"
                            "libyaml/src/writer.c"
                    INCLUDE_DIRS "libyaml/include" 
                    PRIV_INCLUDE_DIRS "libyaml/src"
                    )

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING="0.2.5"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-value)