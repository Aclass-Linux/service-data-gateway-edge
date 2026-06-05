set(APP_TARGET ${ACLASS_PROJECT_NAME})

add_executable(${APP_TARGET} main.c)

target_link_libraries(${APP_TARGET} PRIVATE egw_core)

# 使用 CMake 3.20+ 的推荐方式
set_target_properties(${APP_TARGET} PROPERTIES
    INSTALL_RPATH "\$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH ON  # 构建时使用安装 RPATH
    SKIP_BUILD_RPATH OFF         # 确保构建时也有 RPATH
)

install(TARGETS ${APP_TARGET}
    RUNTIME DESTINATION bin
    COMPONENT ${APP_TARGET}
)