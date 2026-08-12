#pragma once
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

#include "pipeline_executor.hpp"

namespace gda_test {

struct TraversalStats {
    size_t total = 0;        // 工具总数（def 解析）
    size_t excluded = 0;     // 排除数（34 清单）
    size_t passed = 0;       // 通过（不崩溃 + 断言通过/警告容忍）
    size_t failed = 0;       // 失败
    std::vector<std::string> excluded_names;
    std::vector<std::string> warnings;   // 如"schema 必填但空参未报错"清单
};

// 解析 tool_defs.def（路径 PROJECT_ROOT/src/tools/tool_defs.def）：
// 逐行匹配 TOOL_ENTRY(，提取第 2 个引号字段；失败（文件缺失/无法解析）抛 std::runtime_error
std::vector<std::string> parse_domain_tool_names();

// 34 工具排除清单（迁移自 tool_schema_contract_test.cpp 的 kPersistentSideEffectTools）：
// 12 持久副作用（set_editor_main_scene/set_editor_plugin_enabled/save_project_settings/
//   add_input_map_action_event/save_input_map/set_editor_settings/save_editor_scene/
//   save_editor_scenes/save_editor_scene_as/write_file/
//   create_script/save_resource）
// + 22 用户可见副作用（show_os_alert/show_display_dialog/create_os_process/
//   execute_os_process/kill_os_process/open_os_path/move_os_file_to_trash/
//   set_os_environment/speak_display_tts/stop_display_tts/set_display_clipboard/
//   set_display_mouse_mode/warp_display_mouse/set_display_window_title/
//   set_display_window_position/set_display_window_size/set_display_window_mode/
//   set_display_window_flag/move_display_window_to_foreground/
//   request_display_window_attention/create_display_window/delete_display_window）
bool is_excluded_tool(const std::string& name);

// 执行遍历（mode: "empty_args" | "heuristic_smoke"）
// 每工具：跳过排除清单（记入 excluded_names）→ 调用 → 崩溃由调用方检测
// 返回统计；out_steps 追加每个被调用工具的 StepResult（passed/detail）
TraversalStats run_traversal(McpTestClient& client, const std::string& mode,
                             std::vector<StepResult>& out_steps);

} // namespace gda_test
