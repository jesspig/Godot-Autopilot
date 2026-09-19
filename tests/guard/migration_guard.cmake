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
# ====================================================================

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "[migration-guard] SOURCE_DIR: 未定义, 请通过 -DSOURCE_DIR=<仓库根> 传入")
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
