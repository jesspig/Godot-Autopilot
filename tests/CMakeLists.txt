# ====================================================================
# GDA 测试基础设施 (由根 CMakeLists.txt 在 GDA_ENABLE_TESTS=ON 时引入)
# 编译器探测/编译选项/C++17 已在根 CMakeLists.txt 完成, 测试目标继承。
# ====================================================================

# ── googletest (v1.15.2 / b514bdc, third_party submodule) ──
# gda_require_submodule 与 GDA_THIRD_PARTY_DIR 由根 include(Dependencies) 提供。
# 守卫保留: 若 SDK 侧已开启 MCP_BUILD_TESTS 并先声明了 googletest, 此处跳过避免重复声明。
if(NOT TARGET GTest::gtest_main)
    gda_require_submodule(googletest)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    add_subdirectory("${GDA_THIRD_PARTY_DIR}/googletest" "${GDA_SUBMODULE_BIN_DIR}/googletest")
endif()

include(GoogleTest)

# 业务子集源码 (从仓库根引用; ops 实现文件经 make_tools() 被 register_all.cpp 引用, 链接必需)
# resource_registry.cpp 被 code_exec_ops.cpp / resource_ops.cpp 引用 (grep 确认)
file(GLOB_RECURSE GDA_TOOLS_OPS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/tools/domains/*/*_ops.cpp"
    "${CMAKE_SOURCE_DIR}/src/tools/base/*_ops.cpp")

# 链接依赖 (grep 确认)：
#   variant_json.cpp      — ops 大量调用 VariantJson::serialize/deserialize
#   scene_dirty_tracker.cpp — editor/group/property/scene/tilemap ops 调用
#   readback_util.cpp     — property/resource ops 调用 util::check_readback
#   scene_verify.cpp      — editor_ops.cpp 的 save 收据与 verify_scene_saved 调用
#                           scene_verify::compare_tree_with_text/read_text_file
#                           (tools/*_ops.cpp 由上方 GLOB 自动纳入，
#                           含 script_patch_ops.cpp 与 scene_spec_ops.cpp；
#                           game_bridge_verify.cpp 为游戏侧运行时，
#                           仅被 game_bridge.cpp 引用，不进 单元与边界测试业务源)
#
# 源清单聚合：基础业务源在 cmake/sources/gda_unit_sources.cmake，
# 各并行分支的测试源在 cmake/sources/api_unit_*.cmake，
# 两者在此汇入 GDA_UNIT_BUSINESS_SOURCES。
include(${CMAKE_SOURCE_DIR}/cmake/sources/gda_unit_sources.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sources/api_unit_contract.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sources/api_unit_storage.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sources/api_unit_transport.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sources/api_unit_cli.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sources/api_unit_docs.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sources/api_unit_instance_guard.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sources/api_probe.cmake)

set(GDA_UNIT_BUSINESS_SOURCES
    ${GDA_UNIT_BUSINESS_SOURCES}
    ${GDA_UNIT_CONTRACT_SOURCES}
    ${GDA_UNIT_STORAGE_SOURCES}
    ${GDA_UNIT_TRANSPORT_SOURCES}
    ${GDA_UNIT_CLI_SOURCES}
    ${GDA_UNIT_DOCS_SOURCES}
    ${GDA_UNIT_INSTANCE_GUARD_SOURCES})

# ====================================================================
# gda_unit_tests — 单元测试层 (不启动引擎, 不触碰 Godot API)
# ====================================================================
file(GLOB_RECURSE GDA_UNIT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")

if(GDA_UNIT_SOURCES)
    set(GDA_UNIT_ALL_SOURCES ${GDA_UNIT_SOURCES} ${GDA_UNIT_BUSINESS_SOURCES})
    list(REMOVE_DUPLICATES GDA_UNIT_ALL_SOURCES)
    # 边界层用例经 api_unit_*.cmake 登记在业务源里，须从单元目标剔除避免重复注册
    list(FILTER GDA_UNIT_ALL_SOURCES EXCLUDE REGEX "/tests/boundary/")
    add_executable(gda_unit_tests ${GDA_UNIT_ALL_SOURCES})

    # skill 内容头 skill_content_embedded.h 由 cmake/skill_gen.cmake 构建期生成
    add_dependencies(gda_unit_tests gda_skill_embed_header)

    target_include_directories(gda_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests ${CMAKE_BINARY_DIR}/generated)
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

    gtest_discover_tests(gda_unit_tests PROPERTIES LABELS "unit")
endif()

# ====================================================================
# gda_boundary_tests — 边界测试层 (参数校验 / 拒绝路径 / 预算与超时 / 错误映射)
# 源清单走 glob, 新增 boundary/*_test.cpp 后重新 configure 即注册。
# 与单元层共用业务源, 仅排除 tests/ 下的用例文件避免重复注册。
# ====================================================================
file(GLOB GDA_BOUNDARY_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/boundary/*_test.cpp")

if(GDA_BOUNDARY_TEST_SOURCES)
    set(GDA_BOUNDARY_BUSINESS_SOURCES ${GDA_UNIT_BUSINESS_SOURCES})
    list(FILTER GDA_BOUNDARY_BUSINESS_SOURCES EXCLUDE REGEX "/tests/(unit|boundary)/")
    add_executable(gda_boundary_tests
        ${GDA_BOUNDARY_TEST_SOURCES}
        ${GDA_BOUNDARY_BUSINESS_SOURCES})

    add_dependencies(gda_boundary_tests gda_skill_embed_header)

    target_include_directories(gda_boundary_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests ${CMAKE_BINARY_DIR}/generated)
    target_link_libraries(gda_boundary_tests PRIVATE
        godot-cpp mcp-server mcp-http mcp-client GTest::gtest_main)
    if(MSVC)
        target_link_options(gda_boundary_tests PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>")
    endif()

    set_target_properties(gda_boundary_tests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        LINK_SEARCH_START_STATIC ON
        LINK_SEARCH_END_STATIC ON)

    gtest_discover_tests(gda_boundary_tests PROPERTIES LABELS "boundary")
endif()

# ====================================================================
# gda_integration_tests — 集成测试层 (进程内, 零引擎零项目绑定)
# 源清单走 glob, 新增 integration/*_test.cpp 后重新 configure 即注册。
# ====================================================================
file(GLOB GDA_INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/*_test.cpp")

if(GDA_INTEGRATION_TEST_SOURCES)
    set(GDA_INTEGRATION_BUSINESS_SOURCES ${GDA_UNIT_BUSINESS_SOURCES})
    list(FILTER GDA_INTEGRATION_BUSINESS_SOURCES EXCLUDE REGEX "/tests/(unit|boundary)/")
    add_executable(gda_integration_tests
        ${GDA_INTEGRATION_TEST_SOURCES}
        ${GDA_INTEGRATION_BUSINESS_SOURCES})

    add_dependencies(gda_integration_tests gda_skill_embed_header)

    target_include_directories(gda_integration_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests ${CMAKE_BINARY_DIR}/generated)
    target_link_libraries(gda_integration_tests PRIVATE
        godot-cpp mcp-server mcp-http mcp-client GTest::gtest_main)
    if(MSVC)
        target_link_options(gda_integration_tests PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>")
    endif()

    set_target_properties(gda_integration_tests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        LINK_SEARCH_START_STATIC ON
        LINK_SEARCH_END_STATIC ON)

    gtest_discover_tests(gda_integration_tests PROPERTIES LABELS "integration")
endif()


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

# ── ctest 注册: 主线程契约守卫（零依赖，读源码断言 Godot API 直接调用）──
# 扫描 src/ 下的 *.cpp/*.hpp：外线程文件里的 Godot API 调用默认违规，
# 豁免表与线程上下文分类表以显式常量写在 guard/main_thread_guard.py 内。
add_test(NAME main_thread_guard
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/main_thread_guard.py")

# ── ctest 注册: 版本号一致性守卫（零依赖，读源码断言，全部版本钉与 VERSION 同值） ──
add_test(NAME version_sync_guard
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/version_sync_guard.py")

# ── ctest 注册: 测试源清单守卫（零依赖，读源码断言，无需构建产物） ──
# 解析 cmake/sources/ 下插件清单 + src/tools/*_ops.cpp glob 断言并集 == src/ 全部 .cpp，
# 测试源清单引用的 src/ 源 ⊆ 插件并集；解析规则见 guard/source_manifest_guard.py 模块 docstring。
add_test(NAME source_manifest_guard
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/source_manifest_guard.py")

# ── ctest 注册: 工具域守卫（零依赖，读源码断言域规格与实现同名前缀） ──
# 断言 src/tools/<X>_tools.hpp 存在同前缀 <X>_ops.cpp / <X>_*_ops.cpp，
# 且规格文件引用的每个 <ns>::handle_* 都有定义；豁免与判据见 guard/tool_domain_guard.py。
add_test(NAME tool_domain_guard
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/tool_domain_guard.py")

# ── ctest 注册: CLI 契约 mock conformance（纯 stdlib + 自带 mock server）──
# bun 缺失时测试文件自行打印 SKIP 并 exit 0，ctest 视为通过；
# 有 bun 时由 bun 驱动 gda.ts 跑全部断言。
add_test(NAME cli_contract
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cli_contract.py")

# ── ctest 注册: 新 CLI TS 单测（bun test cli/tests，cwd 为仓库根）──
# bun 缺失时测试文件自行打印 SKIP 并 exit 0，ctest 视为通过。
add_test(NAME cli_unit
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_cli_unit.py")
set_tests_properties(cli_unit PROPERTIES TIMEOUT 300)

# --- ctest: guard self-tests (built-in negative samples, --self-test mode) ---
add_test(NAME migration_guard_self_test
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DSELF_TEST=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/guard/migration_guard.cmake)
set_tests_properties(migration_guard_self_test PROPERTIES
    RESOURCE_LOCK migration_guard_self_test)

add_test(NAME comment_guard_self_test
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/comment_guard.py" --self-test)

add_test(NAME main_thread_guard_self_test
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/main_thread_guard.py" --self-test)

add_test(NAME tool_domain_guard_self_test
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/tool_domain_guard.py" --self-test)

# ====================================================================
# ctest 层级标签：unit / boundary / integration 由 gtest_discover_tests 的
# PROPERTIES LABELS 标注；此处补齐源码守卫（guard）与 CLI 脚本项（cli）。
# 全量默认门仍是裸 ctest；分层执行用 ctest -L <label>。
# ====================================================================
set_tests_properties(
    migration_guard
    comment_guard
    main_thread_guard
    version_sync_guard
    source_manifest_guard
    tool_domain_guard
    migration_guard_self_test
    comment_guard_self_test
    main_thread_guard_self_test
    tool_domain_guard_self_test
    PROPERTIES LABELS "guard")

set_tests_properties(cli_contract cli_unit PROPERTIES LABELS "cli")
