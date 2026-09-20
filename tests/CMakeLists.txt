# ====================================================================
# GDA 测试基础设施 (由根 CMakeLists.txt 在 GDA_ENABLE_TESTS=ON 时引入)
# 编译器探测/编译选项/C++17 已在根 CMakeLists.txt 完成, 测试目标继承。
# ====================================================================

# ── googletest (GIT_TAG 与 mcp-cpp-sdk 使用的 v1.15.2 一致) ──
# 若 SDK 侧已开启 MCP_BUILD_TESTS 并先声明了 googletest, 此处跳过避免重复声明。
include(FetchContent)
if(NOT TARGET GTest::gtest_main)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

# 业务子集源码 (从仓库根引用; ops 实现文件经 make_tools() 被 register_all.cpp 引用, 链接必需)
# resource_registry.cpp 被 code_exec_ops.cpp / resource_ops.cpp 引用 (grep 确认)
file(GLOB GDA_TOOLS_OPS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/tools/*_ops.cpp")

# 链接依赖 (grep 确认)：
#   variant_json.cpp      — ops 大量调用 VariantJson::serialize/deserialize
#   scene_dirty_tracker.cpp — editor/group/property/scene/tilemap ops 调用
#   readback_util.cpp     — property/resource ops 调用 util::check_readback
#   scene_verify.cpp      — editor_ops.cpp 的 save 收据与 verify_scene_saved 调用
#                           scene_verify::compare_tree_with_text/read_text_file
#                           (tools/*_ops.cpp 由上方 GLOB 自动纳入，
#                           含 script_patch_ops.cpp 与 scene_spec_ops.cpp；
#                           game_bridge_verify.cpp 为游戏侧运行时，
#                           仅被 game_bridge.cpp 引用，不进 L1 业务源)
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
    ${GDA_TOOLS_OPS_SOURCES})

# ====================================================================
# gda_unit_tests — L1 纯单元测试 (不启动引擎, 不触碰 Godot API)
# ====================================================================
file(GLOB_RECURSE GDA_UNIT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")

if(GDA_UNIT_SOURCES)
    add_executable(gda_unit_tests ${GDA_UNIT_SOURCES} ${GDA_UNIT_BUSINESS_SOURCES})

    # skill 内容头 skill_content_embedded.h 由 cmake/skill_gen.cmake 构建期生成
    add_dependencies(gda_unit_tests gda_skill_embed_header)

    target_include_directories(gda_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
    # mcp-client: register_all_test.cpp 使用 McpClient::Create/ListTools/Close
    # 做 ListTools 往返断言 (SDK 测试链接组合为 mcp-client + mcp-server)
    target_link_libraries(gda_unit_tests PRIVATE
        godot-cpp mcp-server mcp-client GTest::gtest_main)
    if(MSVC)
        target_link_options(gda_unit_tests PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>")
    endif()

    set_target_properties(gda_unit_tests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        LINK_SEARCH_START_STATIC ON
        LINK_SEARCH_END_STATIC ON)

    gtest_discover_tests(gda_unit_tests)
endif()

# ====================================================================
# gda_test_runner — L2 引擎驱动测试执行器 (自管 Godot 进程, 不做 gtest 发现)
# ====================================================================
add_executable(gda_test_runner
    runner/main.cpp
    runner/godot_process.cpp
    runner/config_loader.cpp
    runner/assert_engine.cpp
    runner/pipeline_executor.cpp
    runner/traversal.cpp
    runner/runner_report.cpp
    integration/mcp_test_client.cpp)

target_include_directories(gda_test_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(gda_test_runner PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(gda_test_runner PRIVATE
    godot-cpp mcp-server mcp-http mcp-client)
set_target_properties(gda_test_runner PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
    LINK_SEARCH_START_STATIC ON LINK_SEARCH_END_STATIC ON)
if(MSVC)
    target_link_options(gda_test_runner PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>")
endif()

# ── ctest 注册: 每份 config/*.json 一条 gda_runner_<name> 用例 ──
# 用例文件在 configure 期经 GLOB 收集, config/ 目录新增 *.json 后重新 configure 即注册。
file(GLOB GDA_TEST_CONFIGS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/*.json")

foreach(cfg ${GDA_TEST_CONFIGS})
    get_filename_component(TEST_NAME "${cfg}" NAME_WE)
    add_test(NAME gda_runner_${TEST_NAME}
        COMMAND gda_test_runner --file ${TEST_NAME}
                --report-dir ${CMAKE_CURRENT_BINARY_DIR}/output)
    # 单文件含遍历约 2-4 分钟, 超时防挂死
    set_tests_properties(gda_runner_${TEST_NAME} PROPERTIES TIMEOUT 600)
endforeach()

# ── ctest 注册: 迁移守卫（零依赖，读源码断言，无需构建产物） ──
add_test(NAME migration_guard
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/guard/migration_guard.cmake)

# ── ctest 注册: 注释守卫（零依赖，读源码断言，无需构建产物） ──
# Python 解释器复用根 cmake/skill_gen.cmake 的 GDA_PYTHON_EXECUTABLE
#（find_program NAMES py python python3，py 优先；WindowsApps 存根下裸 python3 不可用）
add_test(NAME comment_guard
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/comment_guard.py")
