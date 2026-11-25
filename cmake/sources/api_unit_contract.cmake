# ====================================================================
# API 基座 — 单元与边界测试契约源清单（变量 GDA_UNIT_CONTRACT_SOURCES）
#
# 由并行子任务分支「API 契约」独占维护：新增测试编译单元只登记到本文件，
# 不改 tests/CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_UNIT_CONTRACT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/server_context.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_config.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transport/http_transport.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/debugger_prompts.cpp
    ${CMAKE_SOURCE_DIR}/src/resources/resource_handlers.cpp
    ${CMAKE_SOURCE_DIR}/src/resources/debugger_resources.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/prompt_handlers.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/prompt_create_3d_scene.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/prompt_setup_character.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/prompt_debug_physics.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/prompt_setup_input_map.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/prompt_setup_gui.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/prompt_tool_usage.cpp
    ${CMAKE_SOURCE_DIR}/src/prompts/prompt_keycode_reference.cpp
    ${CMAKE_SOURCE_DIR}/tests/boundary/authorization_policy_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/boundary/dispatch_semantics_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/protocol/gcp_envelope.cpp)

set(GDA_GCP_CONTRACT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/protocol/gcp_envelope.cpp
    ${CMAKE_SOURCE_DIR}/tests/support/gcp_contract_fixture.cpp)
