# ---- nanoMODBUS 协议库 ----
# nanoMODBUS 是纯 Modbus 协议编解码库，不内建传输层
# 硬件层（RTU/TCP 串口/网口收发）由 src/ 下的其他模块按需实现
add_library(modbus
  nanomodbus.c
)

target_include_directories(modbus
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS modbus
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
  nanomodbus.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
