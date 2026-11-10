# ====================================================================
# API 基座 — 单元与边界测试传输源清单（变量 GDA_UNIT_TRANSPORT_SOURCES）
#
# 由并行子任务分支「API 传输」独占维护：新增测试编译单元只登记到本文件，
# 不改 tests/CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_UNIT_TRANSPORT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/transport/port_resolver.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/http_route_table.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/http_stream_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/godot_transport.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/mcp_http_shim.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/api_surface.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/api_endpoints.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/api_server_routes.cpp
    ${CMAKE_SOURCE_DIR}/src/resources/resource_error_mapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/api_auth.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/api_token.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/ipc_endpoint.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/ipc_transport.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/ipc_endpoint_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/http_transport_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/mcp_http_shim_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/mcp_http_shim_dispatch_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/api_server_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/api_auth_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/api_execute_chain_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/api_endpoints_test.cpp)
