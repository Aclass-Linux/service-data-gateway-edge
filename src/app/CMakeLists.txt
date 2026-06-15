set(APP_TARGET ${ACLASS_PROJECT_NAME})

add_executable(${APP_TARGET} main.c gateway_app.c gateway_engine.c)

target_include_directories(${APP_TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/third-party/libuv/include
)

target_link_libraries(${APP_TARGET} PRIVATE
    egw_core egw_transport egw_protocol egw_ptable egw_persist uv
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
