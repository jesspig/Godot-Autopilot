# ====================================================================
# API 基座 — L1 契约测试源清单（变量 GDA_UNIT_CONTRACT_SOURCES）
#
# 由并行子任务分支「API 契约」独占维护：新增测试编译单元只登记到本文件，
# 不改 tests/CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_UNIT_CONTRACT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/tools/tool_execute.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/argument_validator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/api_envelope.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/tool_execute_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/argument_validator_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/api_envelope_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/authorization_policy_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/dispatch_semantics_test.cpp)
