# ====================================================================
# API 基座 — L1 存储测试源清单（变量 GDA_UNIT_STORAGE_SOURCES）
#
# 由并行子任务分支「API 存储」独占维护：新增测试编译单元只登记到本文件，
# 不改 tests/CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_UNIT_STORAGE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/storage_paths.cpp
    ${CMAKE_SOURCE_DIR}/src/core/legacy_store.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/storage_layout_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/legacy_compat_test.cpp)
