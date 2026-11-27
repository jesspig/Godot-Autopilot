# ====================================================================
# API 基座 — 契约分支源文件清单（变量 GDA_API_CONTRACT_SOURCES）
#
# 由并行子任务分支「API 契约」独占维护：新增编译单元只登记到本文件，
# 不改根 CMakeLists.txt，避免多分支归并冲突。
# ====================================================================
set(GDA_API_CONTRACT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/protocol/gcp_envelope.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control/gcp_dispatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control/control_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control/confirmation_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control/catalog_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control/invocation_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control/job_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control/event_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control/trace_service.cpp
    ${CMAKE_SOURCE_DIR}/src/util/json_value.cpp)
