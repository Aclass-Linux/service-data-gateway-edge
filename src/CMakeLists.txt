# ---- src 子模块聚合 ----
# 各模块按依赖顺序排列：下层 → 上层 → 入口
add_subdirectory(modbus)
add_subdirectory(core)
add_subdirectory(app)
