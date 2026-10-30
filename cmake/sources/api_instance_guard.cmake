# ====================================================================
# API 基座 — 同工程多实例守卫源文件清单（变量 GDA_API_INSTANCE_GUARD_SOURCES）
#
# 由 tests/CMakeLists.txt 与根 CMakeLists.txt include 后展开。
# 路径相对仓库根（add_library 在根目录调用，相对路径按根解析）。
#
# 并行子任务分支请勿改本文件：新增编译单元登记到 cmake/sources/ 下
# 对应的分支清单，避免归并冲突。
# ====================================================================
set(GDA_API_INSTANCE_GUARD_SOURCES
    src/core/instance_guard.cpp)
