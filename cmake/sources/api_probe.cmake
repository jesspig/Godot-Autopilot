# ====================================================================
# API 基座 — 传输探针目标（gda_mcp_transport_probe）
#
# 由并行子任务分支「API 传输」独占维护：探针是可运行的自检程序，
# 验证自建传输可驱动 MCP 服务端；不注册为 ctest 用例，
# 由主代理在验收阶段按需执行。
# ====================================================================
add_executable(gda_mcp_transport_probe
    ${CMAKE_SOURCE_DIR}/tests/probe/mcp_transport_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/core/godot_transport.cpp
    ${CMAKE_SOURCE_DIR}/src/core/http_stream_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/mcp_http_shim.cpp)

target_include_directories(gda_mcp_transport_probe PRIVATE
    ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(gda_mcp_transport_probe PRIVATE mcp-server)

set_target_properties(gda_mcp_transport_probe PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON)
