# ====================================================================
# API 基座 — 单元与边界测试多实例守卫源清单（变量 GDA_UNIT_INSTANCE_GUARD_SOURCES）
#
# 由并行子任务分支独占维护：新增测试编译单元只登记到本文件，
# 不改 tests/CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_UNIT_INSTANCE_GUARD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/storage/instance_guard.cpp
    ${CMAKE_SOURCE_DIR}/tests/boundary/instance_guard_test.cpp)
