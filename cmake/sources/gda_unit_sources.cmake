# ====================================================================
# API 基座 — L1 业务源清单（变量 GDA_UNIT_BUSINESS_SOURCES）
#
# 由 tests/CMakeLists.txt include 后展开进 gda_unit_tests 目标。
# 路径为绝对路径（引用自仓库根）。
#
# 并行子任务分支请勿改本文件：新增测试侧编译单元登记到 cmake/sources/ 下
# 对应的分支清单（api_unit_contract / api_unit_storage /
# api_unit_transport / api_unit_cli / api_unit_docs）。
# ====================================================================
set(GDA_UNIT_BUSINESS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/log_system.cpp
    ${CMAKE_SOURCE_DIR}/src/core/log_persist.cpp
    ${CMAKE_SOURCE_DIR}/src/core/resource_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/scene_dirty_tracker.cpp
    ${CMAKE_SOURCE_DIR}/src/core/export_guard.cpp
    ${CMAKE_SOURCE_DIR}/src/core/editor_readiness.cpp
    ${CMAKE_SOURCE_DIR}/src/core/editor_coords.cpp
    ${CMAKE_SOURCE_DIR}/src/core/trace_recorder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/util/bm25_index.cpp
    ${CMAKE_SOURCE_DIR}/src/util/error_util.cpp
    ${CMAKE_SOURCE_DIR}/src/util/readback_util.cpp
    ${CMAKE_SOURCE_DIR}/src/util/scene_verify.cpp
    ${CMAKE_SOURCE_DIR}/src/util/variant_json.cpp
    ${CMAKE_SOURCE_DIR}/src/util/client_config_gen.cpp
    ${CMAKE_SOURCE_DIR}/src/util/skill_gen.cpp
    ${CMAKE_SOURCE_DIR}/src/util/skill_content_generated.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/tool_catalog.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/schema_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/register_all.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/dispatch.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/debugger_access.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/editor_ui_actions.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/tool_args.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/tool_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/tool_invoke.cpp
    ${CMAKE_SOURCE_DIR}/src/core/gda_home.cpp
    ${CMAKE_SOURCE_DIR}/src/core/project_store.cpp
    ${CMAKE_SOURCE_DIR}/src/core/instance_registry.cpp
    ${CMAKE_SOURCE_DIR}/tests/runner/config_loader.cpp
    ${GDA_TOOLS_OPS_SOURCES})

# API 基座自带的 L1 用例（tests/unit 下的 glob 已自动收集，此处显式登记
# 便于评审核对；tests/CMakeLists.txt 汇入时去重，不会重复编译）。
set(GDA_UNIT_API_BASE_TESTS
    ${CMAKE_SOURCE_DIR}/tests/unit/gda_home_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/project_store_test.cpp
    ${CMAKE_SOURCE_DIR}/tests/unit/instance_registry_test.cpp)

set(GDA_UNIT_BUSINESS_SOURCES
    ${GDA_UNIT_BUSINESS_SOURCES}
    ${GDA_UNIT_API_BASE_TESTS})
