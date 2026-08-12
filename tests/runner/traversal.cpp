#include "traversal.hpp"

#include "integration/mcp_test_client.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

#ifndef PROJECT_ROOT
#error "PROJECT_ROOT 编译宏未定义（tests/CMakeLists.txt 已为 gda_test_runner 配置）"
#endif

namespace {

// ── 34 工具排除清单 ──
// 迁移自 tool_schema_contract_test.cpp 的 kPersistentSideEffectTools。
// 历史事故：set_editor_main_scene 曾把 application/run/main_scene 写成 "test"
// 写入 Example/project.godot；save_editor_scene 空参生成 Example/NewNode.tscn；
// show_os_alert 弹系统模态对话框；set_display_clipboard 覆盖系统剪贴板；
// speak_display_tts 系统朗读。两类工具在空参与冒烟两个遍历中一律跳过。
const char* const kExcludedSideEffectTools[] = {
    // ── 持久磁盘副作用（写 project.godot / editor_settings / .tscn / 文件） ──
    "set_editor_main_scene",
    "set_editor_plugin_enabled",
    "save_project_settings",
    "add_input_map_action_event",
    "save_input_map",
    "set_editor_settings",
    "save_editor_scene",
    "save_editor_scenes",
    "save_editor_scene_as",
    "write_file",
    "create_script",
    "save_resource",

    // ── 用户可见副作用（弹窗/进程/环境变量/音频/剪贴板/鼠标/窗口） ──
    "show_os_alert",
    "show_display_dialog",
    "create_os_process",
    "execute_os_process",
    "kill_os_process",
    "open_os_path",
    "move_os_file_to_trash",
    "set_os_environment",
    "speak_display_tts",
    "stop_display_tts",
    "set_display_clipboard",
    "set_display_mouse_mode",
    "warp_display_mouse",
    "set_display_window_title",
    "set_display_window_position",
    "set_display_window_size",
    "set_display_window_mode",
    "set_display_window_flag",
    "move_display_window_to_foreground",
    "request_display_window_attention",
    "create_display_window",
    "delete_display_window",
};

const char* kModeEmptyArgs = "empty_args";
const char* kModeHeuristicSmoke = "heuristic_smoke";

bool has_error_field(const mcp::JsonValue& j) {
    return j.IsObject() && j.Find("error") != nullptr;
}

std::vector<std::string> schema_required_params(const mcp::JsonValue& detail) {
    std::vector<std::string> out;
    const auto* tool = detail.Find("tool");
    if (!tool || !tool->IsObject())
        return out;
    const auto* schema = tool->Find("input_schema");
    if (!schema || !schema->IsObject())
        return out;
    const auto* req = schema->Find("required");
    if (!req || !req->IsArray())
        return out;
    for (const auto& v : req->GetArray()) {
        if (v.IsString())
            out.push_back(v.GetString());
    }
    return out;
}

std::vector<std::pair<std::string, std::string>>
schema_property_types(const mcp::JsonValue& detail) {
    std::vector<std::pair<std::string, std::string>> out;
    const auto* tool = detail.Find("tool");
    if (!tool || !tool->IsObject())
        return out;
    const auto* schema = tool->Find("input_schema");
    if (!schema || !schema->IsObject())
        return out;
    const auto* props = schema->Find("properties");
    if (!props || !props->IsObject())
        return out;
    for (const auto& [key, value] : props->GetObject()) {
        std::string type = "string";
        if (value.IsObject()) {
            if (const auto* t = value.Find("type")) {
                if (t->IsString())
                    type = t->GetString();
            }
        }
        out.emplace_back(key, type);
    }
    return out;
}

mcp::JsonValue heuristic_value(const std::string& type) {
    if (type == "integer")
        return mcp::JsonValue(static_cast<int64_t>(0));
    if (type == "number")
        return mcp::JsonValue(0.0);
    if (type == "boolean")
        return mcp::JsonValue(false);
    if (type == "array")
        return mcp::JsonValue(mcp::JsonValue::array_tag);
    if (type == "object")
        return mcp::JsonValue(mcp::JsonValue::object_tag);
    return mcp::JsonValue("test");
}

// 经 call_tool 元工具代理调用领域工具；崩溃后响应文本可能为空/非 JSON。
mcp::JsonValue call_domain_tool(gda_test::McpTestClient& client,
                                const std::string& name,
                                const mcp::JsonValue& args,
                                std::string* raw_out) {
    mcp::JsonValue envelope(mcp::JsonValue::object_tag);
    envelope["name"] = mcp::JsonValue(name);
    envelope["arguments"] = args;
    const std::string text = client.call_tool("call_tool", envelope.Dump());
    if (raw_out)
        *raw_out = text;
    return mcp::JsonValue::Parse(text);
}

void print_stats(const std::string& mode, const gda_test::TraversalStats& s,
                 size_t results, size_t errors, size_t missing_req,
                 size_t skipped,
                 const std::vector<std::string>& non_object_names) {
    std::cout << "\n=== 工具遍历: " << mode << " ===\n"
              << "  调用总数: " << s.total << " | 通过: " << s.passed
              << " | 失败: " << s.failed
              << " | result: " << results << " | error: " << errors
              << " | error 含 'missing required': " << missing_req
              << " | 跳过(无 schema properties): " << skipped
              << " | 排除(副作用): " << s.excluded << "\n";
    for (const auto& n : s.warnings) {
        std::cout << "  [schema 必填但空参未报错] " << n << "\n";
    }
    for (const auto& n : non_object_names) {
        std::cout << "  [响应非 JSON 对象] " << n << "\n";
    }
    for (const auto& n : s.excluded_names) {
        std::cout << "  [排除-副作用，跳过调用] " << n << "\n";
    }
}

} // namespace

namespace gda_test {

std::vector<std::string> parse_domain_tool_names() {
    const std::string path =
        std::string(PROJECT_ROOT) + "/src/tools/tool_defs.def";
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("无法打开 " + path);
    }
    std::vector<std::string> names;
    std::string line;
    size_t line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        const size_t p = line.find("TOOL_ENTRY(");
        if (p == std::string::npos)
            continue;
        const size_t comma = line.find(',', p);
        if (comma == std::string::npos) {
            throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                     " 含 TOOL_ENTRY( 但缺逗号，无法解析工具名");
        }
        const size_t q1 = line.find('"', comma);
        if (q1 == std::string::npos) {
            throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                     " 缺工具名引号字段，无法解析工具名");
        }
        const size_t q2 = line.find('"', q1 + 1);
        if (q2 == std::string::npos) {
            throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                     " 工具名引号字段未闭合，无法解析工具名");
        }
        names.push_back(line.substr(q1 + 1, q2 - q1 - 1));
    }
    if (names.empty()) {
        throw std::runtime_error(path + " 未解析出任何工具名");
    }
    return names;
}

bool is_excluded_tool(const std::string& name) {
    for (const char* excluded : kExcludedSideEffectTools) {
        if (name == excluded)
            return true;
    }
    return false;
}

TraversalStats run_traversal(McpTestClient& client, const std::string& mode,
                             std::vector<StepResult>& out_steps) {
    if (mode != kModeEmptyArgs && mode != kModeHeuristicSmoke) {
        throw std::invalid_argument("run_traversal: 未知 mode '" + mode +
                                    "'（仅支持 empty_args / heuristic_smoke）");
    }
    const bool is_empty_args = (mode == kModeEmptyArgs);

    std::vector<std::string> names = parse_domain_tool_names();
    if (!is_empty_args) {
        std::sort(names.begin(), names.end());
    }

    TraversalStats stats;
    size_t results = 0;
    size_t errors = 0;
    size_t missing_req = 0;
    size_t skipped = 0;
    std::vector<std::string> non_object_names;

    for (const std::string& name : names) {
        if (is_excluded_tool(name)) {
            stats.excluded_names.push_back(name);
            ++stats.excluded;
            continue;
        }

        mcp::JsonValue detail_args(mcp::JsonValue::object_tag);
        detail_args["name"] = mcp::JsonValue(name);
        const std::string detail_text =
            client.call_tool("get_tool_detail", detail_args.Dump());
        const mcp::JsonValue detail = mcp::JsonValue::Parse(detail_text);
        const auto* tool = detail.IsObject() ? detail.Find("tool") : nullptr;
        if (!detail.IsObject() || !tool || !tool->IsObject()) {
            StepResult sr;
            sr.passed = false;
            sr.detail = "get_tool_detail('" + name +
                        "') 响应非 JSON 对象或缺 tool 字段: " +
                        detail_text.substr(0, 300);
            out_steps.push_back(std::move(sr));
            non_object_names.push_back(name);
            ++stats.failed;
            continue;
        }
        const auto* tool_name = tool->Find("name");
        if (!tool_name || !tool_name->IsString() ||
            tool_name->GetString() != name) {
            StepResult sr;
            sr.passed = false;
            sr.detail = "get_tool_detail('" + name + "') 返回的工具名不匹配";
            out_steps.push_back(std::move(sr));
            ++stats.failed;
            continue;
        }

        if (is_empty_args) {
            const std::vector<std::string> required =
                schema_required_params(detail);
            std::string raw;
            const mcp::JsonValue resp = call_domain_tool(
                client, name, mcp::JsonValue(mcp::JsonValue::object_tag),
                &raw);
            ++stats.total;
            if (!resp.IsObject()) {
                StepResult sr;
                sr.passed = false;
                sr.detail = "工具 '" + name +
                            "' 空参数响应非 JSON 对象: " + raw.substr(0, 300);
                out_steps.push_back(std::move(sr));
                non_object_names.push_back(name);
                ++stats.failed;
                continue;
            }
            const bool errored = has_error_field(resp);
            if (errored) {
                ++errors;
                const auto* err = resp.Find("error");
                const std::string msg =
                    err && err->IsString() ? err->GetString() : "";
                if (msg.find("missing required") != std::string::npos) {
                    ++missing_req;
                }
            } else {
                ++results;
            }
            // 历史教训：create_scene_node/get_resource_extensions/
            // reimport_resource_files 带默认值的必填参数不校验 → 仅记 warnings，
            // 不 FAIL。
            if (!required.empty() && !errored) {
                stats.warnings.push_back("schema 必填但空参未报错: " + name);
            }
            StepResult sr;
            sr.passed = true;
            sr.detail = "空参调用完成: " + name +
                        (errored ? " (error)" : " (result)");
            out_steps.push_back(std::move(sr));
            ++stats.passed;
        } else {
            const auto props = schema_property_types(detail);
            if (props.empty()) {
                // SCHEMA_NONE / 无 properties：只走空参契约，不冒烟
                ++skipped;
                continue;
            }
            mcp::JsonValue args(mcp::JsonValue::object_tag);
            for (const auto& [key, type] : props) {
                args[key] = heuristic_value(type);
            }
            std::string raw;
            const mcp::JsonValue resp = call_domain_tool(client, name, args,
                                                         &raw);
            ++stats.total;
            if (!resp.IsObject()) {
                StepResult sr;
                sr.passed = false;
                sr.detail = "工具 '" + name +
                            "' 启发式参数响应非 JSON 对象: " + raw.substr(0, 300);
                out_steps.push_back(std::move(sr));
                non_object_names.push_back(name);
                ++stats.failed;
                continue;
            }
            const bool errored = has_error_field(resp);
            if (errored) {
                ++errors;
            } else {
                ++results;
            }
            StepResult sr;
            sr.passed = true;
            sr.detail = "启发式冒烟完成: " + name +
                        (errored ? " (error)" : " (result)");
            out_steps.push_back(std::move(sr));
            ++stats.passed;
        }
    }

    print_stats(mode, stats, results, errors, missing_req, skipped,
                non_object_names);
    return stats;
}

} // namespace gda_test
