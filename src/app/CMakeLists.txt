set(APP_TARGET ${ACLASS_PROJECT_NAME})

add_executable(${APP_TARGET} main.c egw_app.c egw_modbus_master.c egw_modbus_slave.c egw_route.c egw_persist_thread.c)

target_link_libraries(${APP_TARGET} PRIVATE
    egw_core egw_transport egw_protocol egw_ptable
    pthread
)

set_target_properties(${APP_TARGET} PROPERTIES
    INSTALL_RPATH "\$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH ON
    SKIP_BUILD_RPATH OFF
)

install(TARGETS ${APP_TARGET}
    RUNTIME DESTINATION bin
    COMPONENT ${APP_TARGET}
)
