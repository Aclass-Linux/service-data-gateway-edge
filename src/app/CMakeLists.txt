# ---- 主入口可执行程序 ----
add_executable(data-gateway-edge
  main.c
)

# 链接项目库（C++ 标准由根 CMakeLists.txt 统一管理）
target_link_libraries(data-gateway-edge
  PRIVATE
    modbus
    core
)

# 安装到系统 PATH
install(TARGETS data-gateway-edge
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
