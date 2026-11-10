# ====================================================================
# API 基座 — 传输分支源文件清单（变量 GDA_API_TRANSPORT_SOURCES）
#
# 由并行子任务分支「API 传输」独占维护：新增编译单元只登记到本文件，
# 不改根 CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_API_TRANSPORT_SOURCES
    src/core/transport/port_resolver.cpp
    src/core/transport/http_route_table.cpp
    src/core/transport/http_transport.cpp
    src/core/transport/http_stream_registry.cpp
    src/core/transport/godot_transport.cpp
    src/core/transport/mcp_http_shim.cpp
    src/core/transport/api_surface.cpp
    src/core/transport/api_endpoints.cpp
    src/core/transport/api_server.cpp
    src/core/transport/api_server_routes.cpp
    src/core/transport/api_auth.cpp
    src/core/transport/api_token.cpp
    src/core/transport/ipc_endpoint.cpp
    src/core/transport/ipc_transport.cpp)
