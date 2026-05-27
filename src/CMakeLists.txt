# ---- src 子模块聚合 ----
# 各模块按层排列：下层 → 上层 → 入口
add_subdirectory(protocol)
add_subdirectory(transport)
add_subdirectory(core)
add_subdirectory(app)
