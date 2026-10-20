# ====================================================================
# API 基座 — L1 传输测试源清单（变量 GDA_UNIT_TRANSPORT_SOURCES）
#
# 由并行子任务分支「API 传输」独占维护：新增测试编译单元只登记到本文件，
# 不改 tests/CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_UNIT_TRANSPORT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/port_resolver.cpp
    ${CMAKE_SOURCE_DIR}/src/core/http_route_table.cpp
    ${CMAKE_SOURCE_DIR}/src/core/http_stream_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/godot_transport.cpp
    ${CMAKE_SOURCE_DIR}/src/core/mcp_http_shim.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/http_transport_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/mcp_http_shim_test.cpp)
