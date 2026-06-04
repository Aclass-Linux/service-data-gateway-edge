add_executable(gateway main.c)

set_target_properties(gateway PROPERTIES
    BUILD_RPATH "\$ORIGIN/../lib"
    INSTALL_RPATH "\$ORIGIN/../lib"
)

install(TARGETS gateway RUNTIME DESTINATION bin COMPONENT gateway)