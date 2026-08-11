#pragma once
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

#include "pipeline_executor.hpp"

namespace gda_test {

struct TraversalStats {
    size_t total = 0;        // 工具总数（def 解析）
    size_t excluded = 0;     // 排除数（35 清单）
    size_t passed = 0;       // 通过（不崩溃 + 断言通过/警告容忍）
    size_t failed = 0;       // 失败
    std::vector<std::string> excluded_names;
    std::vector<std::string> warnings;   // 如"schema 必填但空参未报错"清单
};

// 解析 tool_defs.def（路径 PROJECT_ROOT/src/tools/tool_defs.def）：
// 逐行匹配 TOOL_ENTRY(，提取第 2 个引号字段；失败（文件缺失/无法解析）抛 std::runtime_error
std::vector<std::string> parse_domain_tool_names();

// 35 工具排除清单（迁移自 tool_schema_contract_test.cpp 的 kPersistentSideEffectTools）：
// 13 持久副作用（editor_set_main_scene/editor_set_plugin_enabled/project_settings_save/
//   input_map_action_add_event/input_map_persist/editor_settings_set/editor_save_scene/
//   editor_save_all_scenes/editor_save_scene_as/editor_new_text_resource/file_write/
//   script_create/resource_save）
// + 22 用户可见副作用（os_alert/display_dialog_show/os_create_process/os_execute/os_kill/
//   os_shell_open/os_move_to_trash/os_set_environment/display_tts_speak/display_tts_stop/
//   display_clipboard_set/display_mouse_set_mode/display_mouse_warp/
//   display_window_set_title/display_window_set_position/display_window_set_size/
//   display_window_set_mode/display_window_set_flag/display_window_move_to_foreground/
//   display_window_request_attention/display_window_create/display_window_delete）
bool is_excluded_tool(const std::string& name);

// 执行遍历（mode: "empty_args" | "heuristic_smoke"）
// 每工具：跳过排除清单（记入 excluded_names）→ 调用 → 崩溃由调用方检测
// 返回统计；out_steps 追加每个被调用工具的 StepResult（passed/detail）
TraversalStats run_traversal(McpTestClient& client, const std::string& mode,
                             std::vector<StepResult>& out_steps);

} // namespace gda_test
