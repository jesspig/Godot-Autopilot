#include "code_exec_ops.hpp"
#include "core/log_system.hpp"
#include "register_all.hpp"
#include "tools/debugger_ops.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <chrono>
#include <cctype>
#include <functional>
#include <sstream>
#include <string>

namespace {

// 包装脚本头部行数：wrapped 中用户源码第 1 行对应包装第 offset+1 行，
// 引擎错误行号减去 offset 即得用户源码行号。
constexpr int WRAP_HEADER_LINES_SINGLE = 4; // "@tool\nextends Node\n\nfunc <name>():\n"
constexpr int WRAP_HEADER_LINES_MULTI = 3;  // "@tool\nextends Node\n\n"

// 将引擎错误文本中 "gdscript://<name>.gd:<line>" 的行号换算为用户源码行号
// （<line> 减去 offset），换算结果 >0 时替换并附注，≤0 时保留原值（指向包装头）。
// tag 非空时仅换算资源名与 tag 匹配的引用；传空串换算所有引用。
std::string map_line_numbers(const std::string& err_text, int offset, const std::string& tag) {
    std::string mapped;
    mapped.reserve(err_text.size());
    const std::string marker = "gdscript://";
    size_t pos = 0;
    while (true) {
        size_t mark = err_text.find(marker, pos);
        if (mark == std::string::npos) {
            mapped.append(err_text, pos, std::string::npos);
            break;
        }
        mapped.append(err_text, pos, mark - pos);
        size_t name_start = mark + marker.size();
        size_t name_end = err_text.find('.', name_start);
        bool is_gd_colon = name_end != std::string::npos
            && err_text.compare(name_end, 4, ".gd:") == 0;
        bool name_matches = tag.empty()
            || err_text.compare(name_start, tag.size(), tag) == 0;
        if (!is_gd_colon || !name_matches) {
            mapped.append(marker);
            pos = name_start;
            continue;
        }
        size_t num_start = name_end + 4;
        size_t num_end = num_start;
        while (num_end < err_text.size()
               && std::isdigit(static_cast<unsigned char>(err_text[num_end]))) {
            ++num_end;
        }
        if (num_end == num_start) {
            mapped.append(marker);
            pos = name_start;
            continue;
        }
        std::string name = err_text.substr(name_start, name_end - name_start);
        int original = std::stoi(err_text.substr(num_start, num_end - num_start));
        std::string replacement;
        if (original - offset > 0) {
            replacement = "gdscript://" + name + ".gd:"
                + std::to_string(original - offset)
                + " (mapped to user source line " + std::to_string(original - offset) + ")";
        } else {
            replacement = "gdscript://" + name + ".gd:" + std::to_string(original);
        }
        mapped += replacement;
        pos = num_end;
    }
    return mapped;
}

// 临时节点 RAII 守卫：析构时从父节点移除并释放，并将外部指针置空防止双重释放。
// 持有指针引用（而非值拷贝），构造/析构后外部指针自动归空。
struct TempNodeGuard {
    godot::Node*& node;

    explicit TempNodeGuard(godot::Node*& n) : node(n) {}

    ~TempNodeGuard() {
        cleanup();
    }

    void cleanup() {
        if (node == nullptr) return;
        if (node->get_parent()) {
            node->get_parent()->remove_child(node);
        }
        memdelete(node);
        node = nullptr;
    }
};

// 从引擎错误文本首行解析结构化字段（file/line/message）。
// 支持形如 "SCRIPT ERROR at gdscript://<name>.gd:<n> - <msg>"
// 与 "[HH:MM:SS] [ERROR] <file>:<line> - <message>" 的引擎文本。
// 无法可靠解析的字段省略；全部无法解析时返回空对象（调用方据此省略）。
mcp::JsonValue extract_structured_error(const std::string& err_text) {
    mcp::JsonValue out(mcp::JsonValue::object_tag);

    std::string line = err_text.substr(0, err_text.find('\n'));
    if (line.empty()) return out;

    size_t sep = line.find(" - ");
    std::string head = (sep == std::string::npos) ? line : line.substr(0, sep);
    std::string message = (sep == std::string::npos) ? "" : line.substr(sep + 3);

    // 从右向左找 "<file>:<digits>"：冒号前字符非数字（排除 [HH:MM:SS] 时间戳），
    // 冒号后紧跟数字。
    for (size_t i = head.size(); i > 0; --i) {
        size_t colon = i - 1;
        if (head[colon] != ':' || colon == 0) continue;
        char before = head[colon - 1];
        if (std::isdigit(static_cast<unsigned char>(before))) continue;
        if (i >= head.size() || !std::isdigit(static_cast<unsigned char>(head[i]))) continue;
        size_t num_end = i;
        while (num_end < head.size()
               && std::isdigit(static_cast<unsigned char>(head[num_end]))) {
            ++num_end;
        }
        size_t file_start = colon;
        while (file_start > 0
               && head[file_start - 1] != ' ' && head[file_start - 1] != '\t'
               && head[file_start - 1] != '[' && head[file_start - 1] != ']') {
            --file_start;
        }
        out["file"] = mcp::JsonValue(head.substr(file_start, colon - file_start));
        out["line"] = mcp::JsonValue(static_cast<int64_t>(
            std::stoi(head.substr(i, num_end - i))));
        break;
    }

    if (!message.empty()) {
        out["message"] = mcp::JsonValue(message);
    }
    return out;
}

} // namespace

namespace godot_self_driving {
namespace code_exec_ops {

mcp::JsonValue handle_batch_execute(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "batch_execute called");

    auto* ops = args.Find("operations");
    if (!ops || !ops->IsArray()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: operations (array)");
        return e;
    }

    bool stop_on_error = true;
    auto* stop = args.Find("stop_on_error");
    if (stop && stop->IsBool()) stop_on_error = stop->GetBool();

    mcp::JsonValue results(mcp::JsonValue::array_tag);
    int succeeded = 0;
    int failed = 0;
    bool stopped = false;
    size_t stopped_after = 0;

    const auto& arr = ops->GetArray();
    for (size_t i = 0; i < arr.size(); ++i) {
        mcp::JsonValue result_item(mcp::JsonValue::object_tag);
        result_item["index"] = mcp::JsonValue(static_cast<int64_t>(i));

        const auto& op = arr[i];
        if (!op.IsObject()) {
            result_item["status"] = mcp::JsonValue("error");
            result_item["error"] = mcp::JsonValue("operation is not an object");
            results.PushBack(std::move(result_item));
            ++failed;
            if (stop_on_error) {
                stopped = true;
                stopped_after = i + 1;
                break;
            }
            continue;
        }

        auto* tool = op.Find("tool");
        if (!tool || !tool->IsString()) {
            result_item["status"] = mcp::JsonValue("error");
            result_item["error"] = mcp::JsonValue("missing required field: tool");
            results.PushBack(std::move(result_item));
            ++failed;
            if (stop_on_error) {
                stopped = true;
                stopped_after = i + 1;
                break;
            }
            continue;
        }

        std::string tool_name = tool->GetString();
        result_item["tool"] = mcp::JsonValue(tool_name);

        mcp::JsonValue tool_args(mcp::JsonValue::object_tag);
        if (auto* a = op.Find("args")) {
            tool_args = *a;
        }

        mcp::JsonValue handler_result = call_handler(tool_name, tool_args);

        if (auto* err = handler_result.Find("error")) {
            result_item["status"] = mcp::JsonValue("error");
            result_item["error"] = *err;
            results.PushBack(std::move(result_item));
            ++failed;
            if (stop_on_error) {
                stopped = true;
                stopped_after = i + 1;
                break;
            }
        } else {
            result_item["status"] = mcp::JsonValue("ok");
            result_item["data"] = std::move(handler_result);
            results.PushBack(std::move(result_item));
            ++succeeded;
        }
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["results"] = std::move(results);
    int64_t executed = succeeded + failed;
    int64_t skipped = static_cast<int64_t>(arr.size()) - executed;
    r["total"] = mcp::JsonValue(executed);
    r["succeeded"] = mcp::JsonValue(static_cast<int64_t>(succeeded));
    r["failed"] = mcp::JsonValue(static_cast<int64_t>(failed));
    r["skipped"] = mcp::JsonValue(skipped);
    if (stopped) {
        r["note"] = mcp::JsonValue("stopped at operation " + std::to_string(stopped_after)
            + "/" + std::to_string(arr.size()) + " (stop_on_error=true); "
            + std::to_string(skipped)
            + " remaining operations were not executed. Set stop_on_error=false to run all operations.");
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "batch_execute completed");
    return r;
}

mcp::JsonValue handle_code_execute(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "code_execute called");

    auto* src = args.Find("source_code");
    if (!src || !src->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: source_code");
        return e;
    }

    std::string source_code = src->GetString();
    std::string func_name = "_run";
    if (auto* fn = args.Find("function_name")) {
        if (fn->IsString()) func_name = fn->GetString();
    }

    int timeout_ms = 5000;
    if (auto* tm = args.Find("timeout_ms")) {
        if (tm->IsInt()) timeout_ms = static_cast<int>(tm->GetInt());
    }

    bool auto_owner = true;
    if (auto* ao = args.Find("auto_owner")) {
        if (ao->IsBool()) auto_owner = ao->GetBool();
    }

    auto start_time = std::chrono::steady_clock::now();

    // Detect if source_code contains func definitions on non-commented lines
    bool has_func_def = false;
    {
        std::istringstream stream(source_code);
        std::string line;
        while (std::getline(stream, line)) {
            size_t pos = line.find_first_not_of(" \t");
            if (pos == std::string::npos || line[pos] == '#') continue;
            if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/') continue;
            if (line.compare(pos, 5, "func ") == 0) {
                has_func_def = true;
                break;
            }
        }
    }

    // Block unsafe close_scene calls that would destroy the executing node
    {
        std::istringstream stream(source_code);
        std::string line;
        while (std::getline(stream, line)) {
            size_t pos = line.find_first_not_of(" \t");
            if (pos == std::string::npos || line[pos] == '#') continue;
            if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/') continue;
            if (line.find("close_scene(") != std::string::npos ||
                line.find("close_scene (") != std::string::npos) {
                mcp::JsonValue e(mcp::JsonValue::object_tag);
                e["error"] = mcp::JsonValue("calling EditorInterface.close_scene() from code_execute is unsafe (it destroys the executing node and crashes the editor) — use the editor_close_scene MCP tool instead");
                return e;
            }
        }
    }

    // Clean extends lines from user code to prevent conflicts with wrapper
    auto clean_extends = [](const std::string& code) -> std::string {
        std::string result;
        std::istringstream stream(code);
        std::string line;
        bool first = true;
        while (std::getline(stream, line)) {
            if (!first) result += "\n";
            first = false;
            size_t pos = line.find_first_not_of(" \t");
            if (pos != std::string::npos && line.compare(pos, 8, "extends ") == 0) {
                result += "# " + line;
            } else {
                result += line;
            }
        }
        return result;
    };

    std::string wrapped;
    if (has_func_def) {
        // Multi-function mode: prepend @tool + extends Node, do NOT wrap in a function
        std::string cleaned = clean_extends(source_code);
        wrapped = "@tool\nextends Node\n\n" + cleaned + "\n";

        // Ensure func_name exists in user code; if not, append a stub
        bool has_named_func = false;
        {
            std::istringstream stream(cleaned);
            std::string line;
            while (std::getline(stream, line)) {
                size_t pos = line.find_first_not_of(" \t");
                if (pos == std::string::npos || line[pos] == '#') continue;
                if (line.compare(pos, 5, "func ") == 0) {
                    size_t name_start = line.find_first_not_of(" \t", pos + 5);
                    if (name_start == std::string::npos) continue;
                    size_t name_end = line.find('(', name_start);
                    if (name_end == std::string::npos) continue;
                    std::string fname = line.substr(name_start, name_end - name_start);
                    size_t last = fname.find_last_not_of(" \t");
                    if (last != std::string::npos) fname = fname.substr(0, last + 1);
                    if (fname == func_name) {
                        has_named_func = true;
                        break;
                    }
                }
            }
        }
        if (!has_named_func) {
            wrapped += "func " + func_name + "():\n    pass\n";
        }
    } else {
        std::string cleaned = clean_extends(source_code);

        // 单函数模式不允许出现 func 定义。has_func_def 已忽略缩进覆盖 "func " 前缀，
        // 此处兜底其漏掉的非标准写法（如 "func" 后跟 tab 或左括号），使失败显式可归因。
        {
            std::istringstream stream(cleaned);
            std::string line;
            while (std::getline(stream, line)) {
                size_t pos = line.find_first_not_of(" \t");
                if (pos == std::string::npos || line[pos] == '#') continue;
                if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/') continue;
                if (line.compare(pos, 5, "func ") == 0
                    || line.compare(pos, 5, "func\t") == 0
                    || (line.compare(pos, 4, "func") == 0
                        && pos + 4 < line.size() && line[pos + 4] == '(')) {
                    return util::error_detail(
                        "func definition detected in single-function mode",
                        "source_code",
                        "no func definitions while in single-function mode",
                        "top-level func definitions require multi-function mode; use editor script_create or wrap in a lambda");
                }
            }
        }

        bool uses_tabs = false;
        bool uses_spaces = false;
        {
            std::istringstream stream(cleaned);
            std::string line;
            while (std::getline(stream, line)) {
                size_t pos = line.find_first_not_of(" \t");
                if (pos == std::string::npos || pos == 0) continue;
                std::string indent = line.substr(0, pos);
                if (indent.find('\t') != std::string::npos) uses_tabs = true;
                if (indent.find("    ") != std::string::npos) uses_spaces = true;
            }
        }

        if (uses_tabs && uses_spaces) {
            return util::error_detail(
                "mixed tab/space indentation detected in source",
                "source_code",
                "consistent indentation",
                "reindent source with only tabs or only spaces; note the wrapper requires the same indentation style throughout");
        }

        bool use_tab_style = uses_tabs && !uses_spaces;
        std::string prefix = use_tab_style ? "\t" : "    ";

        wrapped = "@tool\nextends Node\n\nfunc " + func_name + "():\n";
        if (!cleaned.empty()) {
            std::istringstream stream(cleaned);
            std::string line;
            bool first_line = true;
            while (std::getline(stream, line)) {
                if (!first_line) wrapped += "\n";
                first_line = false;

                // Find original indentation
                size_t content_start = line.find_first_not_of(" \t");
                if (content_start == std::string::npos) {
                    // Empty/whitespace-only line → just the base prefix
                    wrapped += prefix;
                } else {
                    // Line with content: base prefix + original content (preserving relative indent)
                    wrapped += prefix + line;
                }
            }
        }
        wrapped += "\n";
    }

    godot::Ref<godot::GDScript> script;
    script.instantiate();
    if (script.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to create GDScript instance");
        return e;
    }

    script->set_source_code(godot::String(wrapped.c_str()));

    size_t compile_log_before = debugger_ops::capture_log_count();
    godot::Error parse_err = script->reload();
    if (parse_err != godot::OK) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        int code = static_cast<int>(parse_err);
        std::string err_name = "ERR_UNKNOWN";
        if (code == 43) err_name = "ERR_PARSE_ERROR";
        std::string message = "GDScript compilation failed: "
            + err_name + " (code " + std::to_string(code) + ")";
        std::string compile_err = debugger_ops::capture_new_error_text(compile_log_before);
        if (!compile_err.empty()) {
            int map_offset = has_func_def ? WRAP_HEADER_LINES_MULTI : WRAP_HEADER_LINES_SINGLE;
            std::string mapped_err = map_line_numbers(compile_err, map_offset, "");
            if (mapped_err.size() > 8192) {
                message += "\n" + mapped_err.substr(0, 8192)
                    + "\n...(truncated, total " + std::to_string(mapped_err.size()) + " bytes)";
            } else {
                message += "\n" + mapped_err;
            }
            message += "\nerror lines above were mapped from the generated wrapper script";
        }
        message += "\nwrapped source:\n" + wrapped;
        e["error"] = mcp::JsonValue(message);
        return e;
    }

    size_t log_before = debugger_ops::capture_log_count();

    // Record child count before execution for leak detection
    int child_count_before = 0;
    godot::Node* scene_root_for_leak = nullptr;
    {
        auto* editor_for_leak = godot::EditorInterface::get_singleton();
        if (editor_for_leak) {
            scene_root_for_leak = editor_for_leak->get_edited_scene_root();
        }
    }
    if (scene_root_for_leak) {
        child_count_before = scene_root_for_leak->get_child_count();
    }

    // ── 临时节点生命周期：块作用域 + RAII 守卫，正常/异常/提前返回路径必清理 ──
    godot::Variant result;
    std::string new_error_text;
    bool temp_added = false;
    {
        godot::Node* temp_node = nullptr;
        TempNodeGuard temp_guard(temp_node);

        temp_node = memnew(godot::Node);
        temp_node->set_script(godot::Variant(script));

        // 挂到 SceneTree root 下的隔离位置使 get_tree() 可用（不再污染编辑场景根）；
        // root 不可用（理论不存在）时仅作防御回退到编辑场景根。
        godot::Node* parent_node = nullptr;
        {
            auto* engine = godot::Engine::get_singleton();
            auto* main_loop = engine ? engine->get_main_loop() : nullptr;
            auto* tree = godot::Object::cast_to<godot::SceneTree>(main_loop);
            if (tree) {
                parent_node = godot::Object::cast_to<godot::Node>(tree->get_root());
            }
        }
        if (!parent_node) {
            auto* editor = godot::EditorInterface::get_singleton();
            parent_node = editor ? editor->get_edited_scene_root() : nullptr;
        }
        if (parent_node) {
            parent_node->add_child(temp_node);
            temp_added = true;
        }

        godot::StringName fn_name(func_name.c_str());
        if (!temp_node->has_method(fn_name)) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("function not found in compiled script: " + func_name);
            return e;
        }

        bool timeout_hit = false;
        auto check_time = [&]() {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= timeout_ms) timeout_hit = true;
        };

        check_time();
        if (timeout_hit) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("Execution timed out after " + std::to_string(timeout_ms) + " ms");
            return e;
        }

        result = temp_node->call(fn_name);
        new_error_text = debugger_ops::capture_new_error_text(log_before);
    } // 块结束：temp_guard 析构，移除并释放临时节点

    // ── Leak cleanup: remove or retain nodes users added directly to scene_root (without owner) ──
    int auto_owner_set = 0;
    if (scene_root_for_leak) {
        int child_count_after = scene_root_for_leak->get_child_count();
        if (child_count_after > child_count_before) {
            if (auto_owner) {
                std::function<void(godot::Node*)> set_owner_recursive = [&](godot::Node* parent) {
                    for (int i = 0; i < parent->get_child_count(); ++i) {
                        godot::Node* child = parent->get_child(i);
                        if (child->get_owner() == nullptr) {
                            child->set_owner(scene_root_for_leak);
                            ++auto_owner_set;
                        }
                        set_owner_recursive(child);
                    }
                };
                for (int i = 0; i < scene_root_for_leak->get_child_count(); ++i) {
                    set_owner_recursive(scene_root_for_leak->get_child(i));
                }
            } else {
                auto children = scene_root_for_leak->get_children();
                for (int i = children.size() - 1; i >= 0; i--) {
                    auto* child = godot::Object::cast_to<godot::Node>(children[i]);
                    if (child && child->get_owner() == nullptr) {
                        scene_root_for_leak->remove_child(child);
                        memdelete(child);
                    }
                }
            }
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(result);
    r["execution_time_ms"] = mcp::JsonValue(static_cast<int64_t>(elapsed));
    r["auto_owner_set"] = mcp::JsonValue(static_cast<int64_t>(auto_owner_set));
    r["wrapped_source"] = mcp::JsonValue(wrapped);
    if (!new_error_text.empty()) {
        r["runtime_error"] = mcp::JsonValue(true);
        if (new_error_text.size() > 8192) {
            r["error_details"] = mcp::JsonValue(new_error_text.substr(0, 8192)
                + "\n...(truncated, total " + std::to_string(new_error_text.size()) + " bytes)");
        } else {
            r["error_details"] = mcp::JsonValue(new_error_text);
        }
        mcp::JsonValue structured = extract_structured_error(new_error_text);
        if (structured.IsObject() && !structured.Empty()) {
            r["structured_error"] = std::move(structured);
        }
    }
    if (!temp_added) {
        r["note"] = mcp::JsonValue("temporary node was not added to any scene tree — get_tree() will be null");
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "code_execute completed");
    return r;
}

} // namespace code_exec_ops
} // namespace godot_self_driving
