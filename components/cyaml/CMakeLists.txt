idf_component_register(SRCS "libcyaml/src/load.c"
                            "libcyaml/src/save.c"
                            "libcyaml/src/free.c"
                            "libcyaml/src/mem.c"
                            "libcyaml/src/utf8.c"
                            "libcyaml/src/util.c"
                    INCLUDE_DIRS "libcyaml/include" 
                    PRIV_INCLUDE_DIRS "libcyaml/src"
                    REQUIRES yaml
                    )

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    -DVERSION_MAJOR=1 -DVERSION_MINOR=3 -DVERSION_PATCH=1 -Dstatic_assert=_Static_assert
)
                