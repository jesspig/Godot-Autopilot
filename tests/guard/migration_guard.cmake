# ====================================================================
# 迁移守卫 (migration guard) — 读源码断言工具声明迁移不可回退
#
# 用法:
#   cmake -DSOURCE_DIR=<仓库根> -P tests/guard/migration_guard.cmake
#
# 清单 tests/guard/migrated_domains.txt:
#   一行一个已迁移域文件名 (不含路径与扩展名, 如 scene);
#   # 开头为注释行, 空行忽略; 特殊行 strict 启用严格模式。
#
# 规则:
#   1. 每个已迁移域: src/tools/<domain>_tools.hpp 必须存在且不含
#      GDA_TOOL_CLASS / GDA_TOOL_CLASS_SIDE (后者含前者子串, 一并命中);
#   2. strict 追加: 任意 src/tools/*_tools.hpp 不得含旧宏;
#   3. strict 追加: src/tools/schema_*_ops.cpp 不得残留;
#   4. strict 追加: src/tools/*.hpp 与 *.cpp 不得含 tool_input_schema
#      或 tools/tool_decl.hpp 引用。
# 不依赖构建产物, configure 后可直接以 ctest 运行。
#
# 自测模式:
#   cmake -DSOURCE_DIR=<仓库根> -DSELF_TEST=ON -P tests/guard/migration_guard.cmake
#   以 ${CMAKE_BINARY_DIR}/test-output/migration_guard_self_test 为夹具根
#   合成守卫清单与 *_tools.hpp 样本, 经 CMAKE_SCRIPT_MODE_FILE 三连自调用
#   分别验证: 坏样本非零退出 / 清单引用不存在域非零退出 / 干净样本零退出
#   且输出含 migration-guard OK。夹具全部位于构建目录, 不触碰真实源码。
# ====================================================================

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "[migration-guard] SOURCE_DIR: 未定义, 请通过 -DSOURCE_DIR=<仓库根> 传入")
endif()

# ── 自测模式: 合成夹具自证守卫行为, 断言失败即 FATAL_ERROR ──
if(SELF_TEST)
    set(gda_self_root "${CMAKE_BINARY_DIR}/test-output/migration_guard_self_test")
    get_filename_component(gda_self_root "${gda_self_root}" ABSOLUTE)
    set(gda_self_list "${gda_self_root}/tests/guard/migrated_domains.txt")
    set(gda_self_good "${gda_self_root}/src/tools/good_tools.hpp")
    set(gda_self_bad "${gda_self_root}/src/tools/bad2_tools.hpp")
    file(MAKE_DIRECTORY "${gda_self_root}/tests/guard" "${gda_self_root}/src/tools")

    # 场景一: strict 模式下 bad2_tools.hpp 残留旧宏, 期望非零退出
    file(WRITE "${gda_self_list}" "good\nstrict\n")
    file(WRITE "${gda_self_good}" "#pragma once\n")
    file(WRITE "${gda_self_bad}" "#pragma once\n#define GDA_TOOL_CLASS Side\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${gda_self_root}" -P "${CMAKE_SCRIPT_MODE_FILE}"
        RESULT_VARIABLE gda_self_rc OUTPUT_QUIET ERROR_QUIET)
    if(gda_self_rc EQUAL 0)
        message(FATAL_ERROR "[migration-guard] self-test FAIL: 非零退出/退出码 0 (残留旧宏样本未被拦截)")
    endif()

    # 场景二: 清单引用不存在的域 ghost, 期望非零退出
    file(WRITE "${gda_self_list}" "good\nghost\n")
    file(WRITE "${gda_self_good}" "#pragma once\n")
    file(REMOVE "${gda_self_bad}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${gda_self_root}" -P "${CMAKE_SCRIPT_MODE_FILE}"
        RESULT_VARIABLE gda_self_rc OUTPUT_QUIET ERROR_QUIET)
    if(gda_self_rc EQUAL 0)
        message(FATAL_ERROR "[migration-guard] self-test FAIL: 非零退出/退出码 0 (清单引用不存在域未被拦截)")
    endif()

    # 场景三: 仅干净 good_tools.hpp + strict, 期望零退出且输出含 migration-guard OK
    file(WRITE "${gda_self_list}" "good\nstrict\n")
    file(WRITE "${gda_self_good}" "#pragma once\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${gda_self_root}" -P "${CMAKE_SCRIPT_MODE_FILE}"
        RESULT_VARIABLE gda_self_rc
        OUTPUT_VARIABLE gda_self_out ERROR_VARIABLE gda_self_err)
    if(NOT gda_self_rc EQUAL 0)
        message(FATAL_ERROR "[migration-guard] self-test FAIL: 退出码 0/退出码 ${gda_self_rc} (干净样本被误报)")
    endif()
    if(NOT "${gda_self_out}${gda_self_err}" MATCHES "migration-guard\] OK")
        message(FATAL_ERROR "[migration-guard] self-test FAIL: 输出含 [migration-guard] OK/未命中 (干净样本输出异常)")
    endif()

    file(REMOVE_RECURSE "${gda_self_root}")
    return()
endif()

set(GDA_GUARD_LIST "${SOURCE_DIR}/tests/guard/migrated_domains.txt")
if(NOT EXISTS "${GDA_GUARD_LIST}")
    message(FATAL_ERROR "[migration-guard] tests/guard/migrated_domains.txt: 清单文件不存在")
endif()

# ── 解析清单 (CRLF/CR 归一后按行拆分) ──
file(READ "${GDA_GUARD_LIST}" gda_guard_raw)
string(REPLACE "\r\n" "\n" gda_guard_raw "${gda_guard_raw}")
string(REPLACE "\r" "\n" gda_guard_raw "${gda_guard_raw}")
string(REPLACE "\n" ";" gda_guard_lines "${gda_guard_raw}")

set(gda_guard_domains "")
set(gda_guard_strict OFF)
foreach(gda_guard_line ${gda_guard_lines})
    string(STRIP "${gda_guard_line}" gda_guard_line)
    if(gda_guard_line STREQUAL "" OR gda_guard_line MATCHES "^#")
        continue()
    endif()
    if(gda_guard_line STREQUAL "strict")
        set(gda_guard_strict ON)
    else()
        # 每行第一个词为域文件名 (行内其余内容忽略)
        string(REGEX REPLACE "[ \t].*$" "" gda_guard_domain "${gda_guard_line}")
        list(APPEND gda_guard_domains "${gda_guard_domain}")
    endif()
endforeach()

# ── 已迁移域: 头文件存在且不含旧宏 ──
foreach(gda_guard_domain ${gda_guard_domains})
    set(gda_guard_rel "src/tools/${gda_guard_domain}_tools.hpp")
    set(gda_guard_abs "${SOURCE_DIR}/${gda_guard_rel}")
    if(NOT EXISTS "${gda_guard_abs}")
        message(FATAL_ERROR "[migration-guard] ${gda_guard_rel}: 文件不存在（域清单条目无效）")
    endif()
    file(READ "${gda_guard_abs}" gda_guard_content)
    string(FIND "${gda_guard_content}" "GDA_TOOL_CLASS" gda_guard_pos)
    if(NOT gda_guard_pos EQUAL -1)
        message(FATAL_ERROR "[migration-guard] ${gda_guard_rel}: 已迁移域仍含旧宏 GDA_TOOL_CLASS/GDA_TOOL_CLASS_SIDE")
    endif()
endforeach()

# ── strict: 旧机制零残留 (逐项收集后一次性列出所有违规文件) ──
set(gda_guard_violations "")
if(gda_guard_strict)
    # 1) 全部 *_tools.hpp 不得含旧宏
    file(GLOB gda_guard_headers "${SOURCE_DIR}/src/tools/*_tools.hpp")
    foreach(gda_guard_header ${gda_guard_headers})
        file(READ "${gda_guard_header}" gda_guard_content)
        string(FIND "${gda_guard_content}" "GDA_TOOL_CLASS" gda_guard_pos)
        if(NOT gda_guard_pos EQUAL -1)
            file(RELATIVE_PATH gda_guard_rel "${SOURCE_DIR}" "${gda_guard_header}")
            string(APPEND gda_guard_violations "[migration-guard] ${gda_guard_rel}: strict 模式禁止旧宏 GDA_TOOL_CLASS/GDA_TOOL_CLASS_SIDE\n")
        endif()
    endforeach()

    # 2) 旧的 schema_*_ops.cpp 不得残留
    file(GLOB gda_guard_legacy_sources "${SOURCE_DIR}/src/tools/schema_*_ops.cpp")
    foreach(gda_guard_legacy ${gda_guard_legacy_sources})
        file(RELATIVE_PATH gda_guard_rel "${SOURCE_DIR}" "${gda_guard_legacy}")
        string(APPEND gda_guard_violations "[migration-guard] ${gda_guard_rel}: strict 模式禁止残留 schema 文件\n")
    endforeach()

    # 3) 不得引用 tool_input_schema 或 tools/tool_decl.hpp
    file(GLOB gda_guard_sources "${SOURCE_DIR}/src/tools/*.hpp" "${SOURCE_DIR}/src/tools/*.cpp")
    foreach(gda_guard_source ${gda_guard_sources})
        file(READ "${gda_guard_source}" gda_guard_content)
        string(FIND "${gda_guard_content}" "tool_input_schema" gda_guard_pos_schema)
        string(FIND "${gda_guard_content}" "tools/tool_decl.hpp" gda_guard_pos_decl)
        if(NOT gda_guard_pos_schema EQUAL -1 OR NOT gda_guard_pos_decl EQUAL -1)
            file(RELATIVE_PATH gda_guard_rel "${SOURCE_DIR}" "${gda_guard_source}")
            string(APPEND gda_guard_violations "[migration-guard] ${gda_guard_rel}: strict 模式禁止 tool_input_schema / tools/tool_decl.hpp 引用\n")
        endif()
    endforeach()

    if(NOT gda_guard_violations STREQUAL "")
        message(FATAL_ERROR "${gda_guard_violations}")
    endif()
endif()

# ── 成功摘要 ──
list(LENGTH gda_guard_domains gda_guard_domain_count)
if(gda_guard_strict)
    set(gda_guard_strict_label "on")
else()
    set(gda_guard_strict_label "off")
endif()
message(STATUS "[migration-guard] OK (domains: ${gda_guard_domain_count}, strict: ${gda_guard_strict_label})")
