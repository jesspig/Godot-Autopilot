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
# gda_unit_tests — L1 纯单元测试 (不启动引擎, 不触碰 Godot API)
# ====================================================================
file(GLOB_RECURSE GDA_UNIT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")

if(GDA_UNIT_SOURCES)
    set(GDA_UNIT_ALL_SOURCES ${GDA_UNIT_SOURCES} ${GDA_UNIT_BUSINESS_SOURCES})
    list(REMOVE_DUPLICATES GDA_UNIT_ALL_SOURCES)
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

# ====================================================================
# gda_pool_ctl — L2 池生命周期工具 (start/stop/status/restart; T-03 骨架)
# ====================================================================
add_executable(gda_pool_ctl
    pool/pool_ctl_main.cpp
    runner/godot_process.cpp
    integration/mcp_test_client.cpp)

target_include_directories(gda_pool_ctl PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(gda_pool_ctl PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(gda_pool_ctl PRIVATE
    godot-cpp mcp-server mcp-http mcp-client)
set_target_properties(gda_pool_ctl PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
    LINK_SEARCH_START_STATIC ON LINK_SEARCH_END_STATIC ON)
if(MSVC)
    target_link_options(gda_pool_ctl PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>")
endif()

# ── ctest 注册: L2 用例编排（pool=五池常驻+按池直连 / per-test=legacy 逐条启动）──
# 用例文件在 configure 期经 GLOB 收集, config/ 目录新增 *.json 后重新 configure 即注册。
file(GLOB GDA_TEST_CONFIGS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/*.json")

# 路由单一事实源: pool/pools.json 由 pool/gen_pool_map.py 在 configure 期渲染成
# pool_map.cmake（GDA_POOL_MODE / GDA_POOL_NAMES / 每用例 GDA_POOL_<用例名大写且连字符转下划线>）。
# include 后在注册期静态决定每条用例的 --pool，用例↔池的分区不在 CMake 里二次维护。
set(GDA_POOL_POOLS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/pool/pools.json")
set(GDA_POOL_MAP_FILE "${CMAKE_CURRENT_BINARY_DIR}/pool_map.cmake")
set(GDA_POOL_STATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pool/state")

# 回滚开关: -DGDA_POOL_MODE=per-test 强制走 legacy（不受 pools.json 的 mode 影响）；
# 留空则跟随 pools.json。恢复跟随: cmake -UGDA_POOL_MODE --preset <preset>。
set(GDA_POOL_MODE "" CACHE STRING "L2 用例编排模式: 留空=跟随 pools.json, per-test=强制 legacy")
set(GDA_POOL_MODE_OVERRIDE "${GDA_POOL_MODE}")

if(EXISTS "${GDA_POOL_POOLS_JSON}")
    execute_process(
        COMMAND "${GDA_PYTHON_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/pool/gen_pool_map.py"
                --pools "${GDA_POOL_POOLS_JSON}"
                --out "${GDA_POOL_MAP_FILE}"
        RESULT_VARIABLE GDA_POOL_MAP_RESULT
        OUTPUT_VARIABLE GDA_POOL_MAP_STDOUT
        ERROR_VARIABLE GDA_POOL_MAP_STDERR)
    if(NOT GDA_POOL_MAP_RESULT EQUAL 0)
        message(FATAL_ERROR
            "[gda] 池路由映射生成失败 (退出码 ${GDA_POOL_MAP_RESULT}), 用例注册中止:\n"
            "${GDA_POOL_MAP_STDERR}${GDA_POOL_MAP_STDOUT}")
    endif()
    include("${GDA_POOL_MAP_FILE}")
    if(NOT GDA_POOL_MODE_OVERRIDE STREQUAL "")
        set(GDA_POOL_MODE "${GDA_POOL_MODE_OVERRIDE}")
        message(STATUS "[gda] L2 编排模式被 -DGDA_POOL_MODE 覆盖为 ${GDA_POOL_MODE}")
    endif()
    message(STATUS "[gda] L2 编排模式: ${GDA_POOL_MODE} (池: ${GDA_POOL_NAMES})")
else()
    set(GDA_POOL_MODE "per-test")
    message(STATUS "[gda] 未找到 ${GDA_POOL_POOLS_JSON}, L2 用例退化为 per-test (legacy) 注册")
endif()

if(WIN32 AND GDA_POOL_MODE STREQUAL "pool")
    # fixtures: 一次并发启动五池 → 池内用例按路由直连, isolated 用例逐例走 legacy → 停池
    #（不跑用例时 ctest 不触发）。
    add_test(NAME pool_setup
        COMMAND gda_pool_ctl start --all --state-dir "${GDA_POOL_STATE_DIR}")
    set_tests_properties(pool_setup PROPERTIES
        FIXTURES_SETUP PoolSuite
        TIMEOUT 300)

    add_test(NAME pool_cleanup
        COMMAND gda_pool_ctl stop --all --state-dir "${GDA_POOL_STATE_DIR}")
    set_tests_properties(pool_cleanup PROPERTIES
        FIXTURES_CLEANUP PoolSuite
        TIMEOUT 300)

    foreach(cfg ${GDA_TEST_CONFIGS})
        get_filename_component(TEST_NAME "${cfg}" NAME_WE)
        string(TOUPPER "${TEST_NAME}" POOL_VAR_SUFFIX)
        string(REPLACE "-" "_" POOL_VAR_SUFFIX "${POOL_VAR_SUFFIX}")
        set(POOL_VAR "GDA_POOL_${POOL_VAR_SUFFIX}")
        # isolated 用例不参与池分区, 按 legacy 形态逐例注册: 不挂 PoolSuite fixture、不占池锁,
        # 也不读 pool_map 的路由变量。
        if(TEST_NAME IN_LIST GDA_POOL_ISOLATED)
            add_test(NAME gda_runner_${TEST_NAME}
                COMMAND gda_test_runner --file ${TEST_NAME}
                        --report-dir ${CMAKE_CURRENT_BINARY_DIR}/output)
            set_tests_properties(gda_runner_${TEST_NAME} PROPERTIES TIMEOUT 600)
        else()
            if(NOT DEFINED ${POOL_VAR} OR "${${POOL_VAR}}" STREQUAL "")
                message(FATAL_ERROR
                    "[gda] 池路由缺失: 用例 ${TEST_NAME} 在 ${GDA_POOL_MAP_FILE} 中找不到变量 "
                    "${POOL_VAR}; 请检查 ${GDA_POOL_POOLS_JSON} 是否覆盖 config/*.json 全集")
            endif()
            add_test(NAME gda_runner_${TEST_NAME}
                COMMAND gda_test_runner
                        --pool ${${POOL_VAR}}
                        --pool-state-dir "${GDA_POOL_STATE_DIR}"
                        --file ${TEST_NAME}
                        --report-dir ${CMAKE_CURRENT_BINARY_DIR}/output)
            # 池模式省去逐例 Godot 启动, 单例仍可能触发一次自愈 restart, 超时与 legacy 对齐
            # RESOURCE_LOCK 按路由池分组: ctest -j 下同池用例串行、跨池并行（并行度=池数, DR-04）
            set_tests_properties(gda_runner_${TEST_NAME} PROPERTIES
                FIXTURES_REQUIRED PoolSuite
                TIMEOUT 600
                RESOURCE_LOCK "pool_${${POOL_VAR}}")
        endif()
    endforeach()
else()
    # legacy 回滚路径: 每份 config 一条 --file 用例, 逐例自起 Godot, 无 fixtures。
    # 非 Windows 平台无法跑 L2 池（pool_setup 必失败）, 一律降级到 legacy 单用例注册。
    if(NOT WIN32)
        message(STATUS "[gda] 非 Windows 平台：L2 按 legacy 单用例注册")
    endif()
    foreach(cfg ${GDA_TEST_CONFIGS})
        get_filename_component(TEST_NAME "${cfg}" NAME_WE)
        add_test(NAME gda_runner_${TEST_NAME}
            COMMAND gda_test_runner --file ${TEST_NAME}
                    --report-dir ${CMAKE_CURRENT_BINARY_DIR}/output)
        # 单文件含遍历约 2-4 分钟, 超时防挂死
        set_tests_properties(gda_runner_${TEST_NAME} PROPERTIES TIMEOUT 600)
    endforeach()
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

# ── ctest 注册: L1 源清单守卫（零依赖，读源码断言，无需构建产物） ──
# 解析 cmake/sources/ 下插件清单 + src/tools/*_ops.cpp glob 断言并集 == src/ 全部 .cpp，
# L1 清单引用的 src/ 源 ⊆ 插件并集；解析规则见 guard/source_manifest_guard.py 模块 docstring。
add_test(NAME source_manifest_guard
    COMMAND "${GDA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/guard/source_manifest_guard.py")

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
