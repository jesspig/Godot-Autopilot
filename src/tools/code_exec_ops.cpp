#include "code_exec_ops.hpp"
#include "core/log_system.hpp"
#include "register_all.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <chrono>
#include <sstream>
#include <string>

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
            if (stop_on_error) break;
            continue;
        }

        auto* tool = op.Find("tool");
        if (!tool || !tool->IsString()) {
            result_item["status"] = mcp::JsonValue("error");
            result_item["error"] = mcp::JsonValue("missing required field: tool");
            results.PushBack(std::move(result_item));
            ++failed;
            if (stop_on_error) break;
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
            if (stop_on_error) break;
        } else {
            result_item["status"] = mcp::JsonValue("ok");
            result_item["data"] = std::move(handler_result);
            results.PushBack(std::move(result_item));
            ++succeeded;
        }
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["results"] = std::move(results);
    r["total"] = mcp::JsonValue(static_cast<int64_t>(succeeded + failed));
    r["succeeded"] = mcp::JsonValue(static_cast<int64_t>(succeeded));
    r["failed"] = mcp::JsonValue(static_cast<int64_t>(failed));
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

    auto start_time = std::chrono::steady_clock::now();

    // Detect if source_code contains func definitions on non-commented lines
    bool has_func_def = false;
    {
        std::istringstream stream(source_code);
        std::string line;
        while (std::getline(stream, line)) {
            size_t pos = line.find_first_not_of(" \t");
            if (pos == std::string::npos || line[pos] == '#') continue;
            if (line.compare(pos, 5, "func ") == 0) {
                has_func_def = true;
                break;
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

        bool use_tab_style = uses_tabs && !uses_spaces;
        std::string prefix = use_tab_style ? "\t" : "    ";

        // Detect if user code already has indentation on continuation lines
        bool has_indent = false;
        for (size_t i = 0; i < cleaned.size(); i++) {
            if (cleaned[i] == '\n' && i + 1 < cleaned.size() && (cleaned[i+1] == ' ' || cleaned[i+1] == '\t')) {
                has_indent = true;
                break;
            }
        }

        wrapped = "@tool\nextends Node\n\nfunc " + func_name + "():\n";
        if (!cleaned.empty()) {
            if (has_indent) {
                wrapped += cleaned;
            } else {
                wrapped += prefix;
                for (size_t i = 0; i < cleaned.size(); i++) {
                    if (cleaned[i] == '\n') {
                        wrapped += '\n';
                        wrapped += prefix;
                    } else {
                        wrapped += cleaned[i];
                    }
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

    godot::Error parse_err = script->reload();
    if (parse_err != godot::OK) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("GDScript compilation failed with error code: "
            + std::to_string(static_cast<int>(parse_err)));
        return e;
    }

    godot::Node* temp_node = memnew(godot::Node);
    temp_node->set_script(godot::Variant(script));

    // Temporarily add to scene tree so get_tree() is available
    godot::EditorInterface* editor_4 = godot::EditorInterface::get_singleton();
    godot::Node* scene_root_4 = editor_4 ? editor_4->get_edited_scene_root() : nullptr;
    bool temp_added_4 = false;
    if (scene_root_4) {
        scene_root_4->add_child(temp_node);
        temp_added_4 = true;
    }

    godot::StringName fn_name(func_name.c_str());
    if (!temp_node->has_method(fn_name)) {
        if (temp_added_4 && temp_node->get_parent()) {
            temp_node->get_parent()->remove_child(temp_node);
        }
        memdelete(temp_node);
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("function not found in compiled script: " + func_name);
        return e;
    }

    bool timeout_hit = false;
    godot::Variant result;
    auto check_time = [&]() {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        if (elapsed >= timeout_ms) timeout_hit = true;
    };

    check_time();
    if (timeout_hit) {
        if (temp_added_4 && temp_node->get_parent()) {
            temp_node->get_parent()->remove_child(temp_node);
        }
        memdelete(temp_node);
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("Execution timed out after " + std::to_string(timeout_ms) + " ms");
        return e;
    }

    result = temp_node->call(fn_name);

    if (temp_added_4 && temp_node->get_parent()) {
        temp_node->get_parent()->remove_child(temp_node);
    }
    memdelete(temp_node);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(result);
    r["execution_time_ms"] = mcp::JsonValue(static_cast<int64_t>(elapsed));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "code_execute completed");
    return r;
}

} // namespace code_exec_ops
} // namespace godot_self_driving
