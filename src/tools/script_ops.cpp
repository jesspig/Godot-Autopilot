#include "script_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include "../util/scene_path.hpp"
#include "tools/property_ops.hpp"
#include "tools/scene_ops.hpp"
#include "tools/debugger_ops.hpp"
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <sstream>
#include <string>

namespace godot_self_driving {
namespace script_ops {

namespace {

constexpr const char* NODE_PATH_HINT = " — valid path forms: with scene root name (e.g. 'GameManager/HUD'), without root name (e.g. 'HUD'), with 'root/' prefix (e.g. 'root/GameManager/HUD'), or absolute (e.g. '/root/GameManager/HUD')";

// "Node not found" 运行时错误追加的用法指引：执行节点在 /root 下，不在编辑场景内；
// 必须通过 SceneRoot（编辑场景根节点）以无根名前缀的相对路径访问场景节点。
// 与 code_exec_ops.cpp 中同名常量逐字一致（DRY 第三次重复时才提取）。
constexpr const char* NODE_NOT_FOUND_HINT = "\n[hint] Node path resolution: the execution node lives under /root, NOT inside the edited scene. Use SceneRoot.get_node(\"Child\") to reach edited-scene nodes (SceneRoot is the scene root node itself — no root-name prefix, e.g. SceneRoot.get_node(\"Player\") or SceneRoot.get_node(\"Player/CollisionShape2D\")).";

constexpr int PROPERTY_USAGE_CATEGORY = 0x80;
constexpr int PROPERTY_USAGE_INTERNAL = 0x08;

constexpr const char* SINGLE_EXPR_BLOCKING_KEYWORDS[] = {
    "if", "for", "while", "match", "func", "return", "var", "const",
    "class", "static", "break", "continue", "pass", "await", "try",
    "assert", "super", "self",
};

bool is_plain_assignment(const std::string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '=') continue;
        char prev = i > 0 ? text[i - 1] : '\0';
        char next = i + 1 < text.size() ? text[i + 1] : '\0';
        bool prev_is_op = prev == '=' || prev == '!' || prev == '<' || prev == '>';
        bool next_is_op = next == '=' || next == '!' || next == '<' || next == '>';
        if (!prev_is_op && !next_is_op) return true;
    }
    return false;
}

bool is_single_expression(const std::string& text) {
    if (text.find('\n') != std::string::npos) return false;
    size_t start = text.find_first_not_of(" \t");
    if (start == std::string::npos || text[start] == '#') return false;
    if (is_plain_assignment(text)) return false;
    size_t word_end = start;
    while (word_end < text.size()) {
        char c = text[word_end];
        if (c == ' ' || c == '\t' || c == '(' || c == ':') break;
        ++word_end;
    }
    std::string first_word = text.substr(start, word_end - start);
    for (const char* keyword : SINGLE_EXPR_BLOCKING_KEYWORDS) {
        if (first_word == keyword) return false;
    }
    return true;
}

mcp::JsonValue serialize_resource(const godot::Ref<godot::Resource>& res) {
    if (res.is_null()) return mcp::JsonValue(nullptr);
    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["class"] = mcp::JsonValue(util::to_std(res->get_class()));
    j["path"] = mcp::JsonValue(util::to_std(res->get_path()));
    j["object_id"] = mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
    j["object_id_str"] = mcp::JsonValue(std::to_string(static_cast<int64_t>(res->get_instance_id())));
    return j;
}

std::string truncate_capture_text(const std::string& text) {
    constexpr size_t MAX_CAPTURE_BYTES = 8192;
    if (text.size() > MAX_CAPTURE_BYTES) {
        return text.substr(0, MAX_CAPTURE_BYTES)
            + "\n...(truncated, total " + std::to_string(text.size()) + " bytes)";
    }
    return text;
}

// 缓存失效先例与 resource_ops.cpp:869-874 逐字一致（ResourceCache 条目随 Resource 析构移除，
// 失效对象可能因 path_cache 驻留，set_path("") 使缓存条目移除）。返回是否实际清除了缓存条目。
bool invalidate_cached_resource(const std::string& path) {
    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader || !loader->has_cached(godot::String(path.c_str()))) {
        return false;
    }
    auto cached = loader->get_cached_ref(godot::String(path.c_str()));
    if (cached.is_valid()) {
        cached->set_path("");
    }
    return true;
}

// 顶层 func 定义检测，移植自 code_exec_ops.cpp:291-304（跳过注释行）
bool has_top_level_func_def(const std::string& code) {
    std::istringstream stream(code);
    std::string line;
    while (std::getline(stream, line)) {
        size_t pos = line.find_first_not_of(" \t");
        if (pos == std::string::npos || line[pos] == '#') continue;
        if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/') continue;
        if (line.compare(pos, 5, "func ") == 0) return true;
    }
    return false;
}

// 函数名解析移植自 code_exec_ops.cpp:349-370：检测用户代码是否定义了指定顶层函数
bool defines_function_named(const std::string& code, const std::string& target) {
    std::istringstream stream(code);
    std::string line;
    while (std::getline(stream, line)) {
        size_t pos = line.find_first_not_of(" \t");
        if (pos == std::string::npos || line[pos] == '#') continue;
        if (line.compare(pos, 5, "func ") != 0) continue;
        size_t name_start = line.find_first_not_of(" \t", pos + 5);
        if (name_start == std::string::npos) continue;
        size_t name_end = line.find('(', name_start);
        if (name_end == std::string::npos) continue;
        std::string fname = line.substr(name_start, name_end - name_start);
        size_t last = fname.find_last_not_of(" \t");
        if (last != std::string::npos) fname = fname.substr(0, last + 1);
        if (fname == target) return true;
    }
    return false;
}

} // namespace

mcp::JsonValue handle_execute_gdscript(const mcp::JsonValue& args) {
    auto* it_expr = args.Find("expression");
    if (!it_expr || !it_expr->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: expression");
        return e;
    }
    std::string expression = it_expr->GetString();

    std::string cleaned;
    {
        std::istringstream stream(expression);
        std::string line;
        bool first = true;
        while (std::getline(stream, line)) {
            if (!first) cleaned += "\n";
            first = false;
            size_t pos = line.find_first_not_of(" \t");
            if (pos != std::string::npos && line.compare(pos, 8, "extends ") == 0) {
                cleaned += "# " + line;
            } else {
                cleaned += line;
            }
        }
    }

    std::string wrapped;
    if (has_top_level_func_def(cleaned)) {
        // 多函数模式（移植自 code_exec_ops.cpp:342-373）：顶层放置用户代码，不包进 _run。
        // 本工具无 function_name 参数，入口固定为 _run；未定义 _run 时显式报错。
        if (!defines_function_named(cleaned, "_run")) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("multi-function mode requires a func _run() entry point（或将函数改为 lambda 变量）");
            return e;
        }
        wrapped = "@tool\nextends Node\n\nvar SceneRoot := EditorInterface.get_edited_scene_root()\n\n" + cleaned + "\n";
    } else if (is_single_expression(expression)) {
        wrapped = "@tool\nextends Node\n\nfunc _run():\n";
        // 注入 SceneRoot 便捷变量（编辑场景根节点）：执行节点在 /root 下，get_node() 找不到编辑场景节点
        wrapped += "    var SceneRoot := EditorInterface.get_edited_scene_root()\n";
        wrapped += "    return " + cleaned + "\n";
        wrapped += "\n";
    } else {
        // 缩进风格检测（移植自 code_exec_ops.cpp:399-422）：按用户风格选择包装前缀
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
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("mixed tab/space indentation detected in source — reindent source with only tabs or only spaces; note the wrapper requires the same indentation style throughout");
            return e;
        }
        std::string prefix = (uses_tabs && !uses_spaces) ? "\t" : "    ";
        wrapped = "@tool\nextends Node\n\nfunc _run():\n";
        // 注入 SceneRoot 便捷变量（编辑场景根节点）：执行节点在 /root 下，get_node() 找不到编辑场景节点
        wrapped += prefix + "var SceneRoot := EditorInterface.get_edited_scene_root()\n";
        if (!cleaned.empty()) {
            std::istringstream stream(cleaned);
            std::string line;
            bool first_line = true;
            while (std::getline(stream, line)) {
                if (!first_line) wrapped += "\n";
                first_line = false;
                size_t content_start = line.find_first_not_of(" \t");
                if (content_start == std::string::npos) {
                    wrapped += prefix;
                } else {
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
    godot::Error parse_err2 = script->reload();
    if (parse_err2 != godot::OK) {
        std::string message = "GDScript compilation failed: ERR_PARSE_ERROR (code "
            + std::to_string(static_cast<int>(parse_err2)) + ")";
        std::string compile_err = debugger_ops::capture_new_error_text(compile_log_before);
        if (!compile_err.empty()) {
            message += "\n" + truncate_capture_text(compile_err);
        }
        message += "\nwrapped source:\n" + wrapped;
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(message);
        return e;
    }

    godot::Node* temp_node = memnew(godot::Node);
    temp_node->set_script(godot::Variant(script));

    bool temp_added = false;
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

    godot::StringName fn_name("_run");
    if (!temp_node->has_method(fn_name)) {
        if (temp_added && temp_node->get_parent()) temp_node->get_parent()->remove_child(temp_node);
        memdelete(temp_node);
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("unexpected error: compiled script missing _run method");
        return e;
    }

    size_t log_before = debugger_ops::capture_log_count();
    godot::Variant result = temp_node->call(fn_name);
    std::string new_output_text = debugger_ops::capture_new_output_text(log_before);
    std::string new_error_text = debugger_ops::capture_new_error_text(log_before);

    if (temp_added && temp_node->get_parent()) temp_node->get_parent()->remove_child(temp_node);
    memdelete(temp_node);

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(result);
    if (!new_output_text.empty()) {
        r["output"] = mcp::JsonValue(truncate_capture_text(new_output_text));
    }
    if (!new_error_text.empty()) {
        std::string errors = truncate_capture_text(new_error_text);
        if (new_error_text.find("Node not found") != std::string::npos) {
            errors += NODE_NOT_FOUND_HINT;
        }
        r["errors"] = mcp::JsonValue(errors);
    }
    return r;
}

mcp::JsonValue handle_load(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load(godot::String(path.c_str()));
    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to load script: " + path);
        return e;
    }

    godot::Ref<godot::Script> script = res;
    if (script.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("loaded resource is not a Script: " + path);
        return e;
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = serialize_resource(res);
    return r;
}

mcp::JsonValue handle_create(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    auto* it_code = args.Find("source_code");
    if (!it_code || !it_code->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: source_code");
        return e;
    }
    std::string path = it_path->GetString();
    std::string source_code = it_code->GetString();

    bool overwrite = false;
    auto* it_overwrite = args.Find("overwrite");
    if (it_overwrite && it_overwrite->IsBool()) overwrite = it_overwrite->GetBool();

    bool file_exists_on_disk = godot::FileAccess::file_exists(godot::String(path.c_str()));
    if (!overwrite && file_exists_on_disk) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("file already exists: " + path + " — pass overwrite=true to replace it");
        return e;
    }

    godot::Ref<godot::GDScript> script;
    script.instantiate();
    if (script.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to create GDScript");
        return e;
    }

    script->set_source_code(godot::String(source_code.c_str()));
    script->set_path_cache(godot::String(path.c_str()));
    size_t compile_log_before = debugger_ops::capture_log_count();
    godot::Error reload_err = script->reload();
    if (reload_err != godot::OK) {
        // 编译失败对象不应因 path_cache 驻留缓存：清理后下次 load() 才能读到磁盘真实内容
        invalidate_cached_resource(path);
        std::string message = "script compilation failed: ERR_PARSE_ERROR (code " + std::to_string(static_cast<int>(reload_err)) + ")";
        std::string compile_err = debugger_ops::capture_new_error_text(compile_log_before);
        if (!compile_err.empty()) {
            if (compile_err.size() > 8192) {
                message += "\n" + compile_err.substr(0, 8192)
                    + "\n...(truncated, total " + std::to_string(compile_err.size()) + " bytes)";
            } else {
                message += "\n" + compile_err;
            }
        }
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(message);
        return e;
    }

    auto* saver = godot::ResourceSaver::get_singleton();
    if (!saver) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceSaver not available");
        return e;
    }

    std::string dir_path = path;
    size_t last_slash = dir_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        dir_path = dir_path.substr(0, last_slash);
        auto dir = godot::DirAccess::open(godot::String("res://"));
        if (dir.is_valid()) {
            dir->make_dir_recursive(godot::String(dir_path.c_str()));
        }
    }

    godot::Error err = saver->save(script, godot::String(path.c_str()));
    if (err != godot::OK) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to save script, error code: " + std::to_string(static_cast<int>(err)));
        return e;
    }

    // 覆盖重写后旧版脚本对象可能仍在 ResourceCache：set_path("") 使缓存条目随析构移除
    bool cache_invalidated = invalidate_cached_resource(path);
    auto* editor = godot::EditorInterface::get_singleton();
    if (editor) {
        auto* efs = editor->get_resource_filesystem();
        if (efs) {
            // 先例：resource_ops.cpp:503-505；仅刷新文件系统记录，全局类注册由引擎异步排队
            efs->update_file(godot::String(path.c_str()));
        }
    }

    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["path"] = mcp::JsonValue(path);
    j["class"] = mcp::JsonValue("GDScript");
    j["overwritten"] = mcp::JsonValue(file_exists_on_disk);
    if (cache_invalidated) {
        j["cache_invalidated"] = mcp::JsonValue(true);
    }
    godot::StringName global_name = script->get_global_name();
    if (global_name != godot::StringName()) {
        j["global_class_name"] = mcp::JsonValue(util::to_std(godot::String(global_name)));
        // update_file 仅排队全局类注册（引擎 editor_file_system.cpp:2519-2521 异步执行），
        // 故提示延迟而非全量 scan()（scan 异步且有副作用）
        j["global_class_hint"] = mcp::JsonValue("global class registration may be delayed — cross-script references may need preload() or a filesystem rescan");
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(j);
    return r;
}

mcp::JsonValue handle_attach_to_node(const mcp::JsonValue& args) {
    auto* it_node = args.Find("node_path");
    auto* it_script = args.Find("script_path");
    if (!it_node || !it_node->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: node_path");
        return e;
    }
    if (!it_script || !it_script->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: script_path");
        return e;
    }
    std::string node_path = it_node->GetString();
    std::string script_path = it_script->GetString();

    godot::Node* root = nullptr;
    if (auto* editor = godot::EditorInterface::get_singleton()) {
        root = editor->get_edited_scene_root();
    }
    std::string hint;
    godot::Node* node = util::resolve_scene_node(node_path, root, &hint);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT + " — " + hint);
        return e;
    }

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load(godot::String(script_path.c_str()));
    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to load script: " + script_path);
        return e;
    }

    godot::Ref<godot::Script> script = res;
    if (script.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("loaded resource is not a Script: " + script_path);
        return e;
    }

    auto* editor = godot::EditorInterface::get_singleton();
    auto* undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
    if (undo_redo) {
        undo_redo->create_action("Attach Script");
        undo_redo->add_do_method(node, godot::StringName("set_script"), godot::Variant(script));
        undo_redo->add_undo_method(node, godot::StringName("set_script"), node->get_script());
        undo_redo->commit_action();
    } else {
        node->set_script(godot::Variant(script));
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("script attached to " + node_path);
    bool instantiable = script->can_instantiate();
    r["instantiated"] = mcp::JsonValue(instantiable);
    if (!instantiable) {
        r["note"] = mcp::JsonValue("script is not instantiable in the editor: method calls will fail until the game is run — mark the script with @tool to run in the editor");
    }
    return r;
}

mcp::JsonValue handle_detach_from_node(const mcp::JsonValue& args) {
    auto* it_node = args.Find("node_path");
    if (!it_node || !it_node->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: node_path");
        return e;
    }
    std::string node_path = it_node->GetString();

    godot::Node* root = nullptr;
    if (auto* editor = godot::EditorInterface::get_singleton()) {
        root = editor->get_edited_scene_root();
    }
    std::string hint;
    godot::Node* node = util::resolve_scene_node(node_path, root, &hint);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT + " — " + hint);
        return e;
    }

    auto* editor = godot::EditorInterface::get_singleton();
    auto* undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
    if (undo_redo) {
        undo_redo->create_action("Detach Script");
        undo_redo->add_do_method(node, godot::StringName("set_script"), godot::Variant());
        undo_redo->add_undo_method(node, godot::StringName("set_script"), node->get_script());
        undo_redo->commit_action();
    } else {
        node->set_script(godot::Variant());
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("script detached from " + node_path);
    return r;
}

mcp::JsonValue handle_get_property(const mcp::JsonValue& args) {
    auto* it_prop = args.Find("property");
    if (!it_prop || !it_prop->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: property");
        return e;
    }
    std::string property = it_prop->GetString();

    auto* it_path = args.Find("script_path");
    if (it_path && it_path->IsString()) {
        std::string script_path = it_path->GetString();
        auto* loader = godot::ResourceLoader::get_singleton();
        if (!loader) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("ResourceLoader not available");
            return e;
        }

        godot::Ref<godot::Resource> res = loader->load(godot::String(script_path.c_str()));
        if (res.is_null()) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("failed to load script: " + script_path);
            return e;
        }
        godot::Ref<godot::Script> script = res;
        if (script.is_null()) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("loaded resource is not a Script: " + script_path);
            return e;
        }

        godot::StringName prop_name(property.c_str());
        godot::Variant val = script->get_property_default_value(prop_name);
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = VariantJson::serialize(val);
        return r;
    }

    auto* it_node = args.Find("node_path");
    if (it_node && it_node->IsString()) {
        std::string node_path = it_node->GetString();
        godot::Node* root = nullptr;
        if (auto* editor = godot::EditorInterface::get_singleton()) {
            root = editor->get_edited_scene_root();
        }
        std::string hint;
        godot::Node* node = util::resolve_scene_node(node_path, root, &hint);
        if (!node) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT + " — " + hint);
            return e;
        }
        godot::StringName prop_name(property.c_str());
        godot::Variant val = node->get(prop_name);
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = VariantJson::serialize(val);
        return r;
    }

    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("must provide either script_path or node_path");
    return e;
}

mcp::JsonValue handle_set_property(const mcp::JsonValue& args) {
    auto* it_node = args.Find("node_path");
    auto* it_prop = args.Find("property");
    auto* it_val = args.Find("value");
    if (!it_node || !it_node->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: node_path");
        return e;
    }
    if (!it_prop || !it_prop->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: property");
        return e;
    }
    if (!it_val) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: value");
        return e;
    }
    std::string node_path = it_node->GetString();
    std::string property = it_prop->GetString();

    godot::Node* root = nullptr;
    if (auto* editor = godot::EditorInterface::get_singleton()) {
        root = editor->get_edited_scene_root();
    }
    std::string hint;
    godot::Node* node = util::resolve_scene_node(node_path, root, &hint);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT + " — " + hint);
        return e;
    }

    std::string type_hint;
    auto* it_hint = args.Find("type_hint");
    if (it_hint && it_hint->IsString()) {
        type_hint = it_hint->GetString();
    }

    godot::StringName prop_name(property.c_str());
    godot::Variant value = VariantJson::deserialize(*it_val, type_hint);
    node->set(prop_name, value);

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    return r;
}

mcp::JsonValue handle_call_function(const mcp::JsonValue& args) {
    auto* it_node = args.Find("node_path");
    if (!it_node || !it_node->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: node_path");
        return e;
    }
    auto* it_func = args.Find("function");
    if (!it_func || !it_func->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: function");
        return e;
    }
    std::string node_path = it_node->GetString();
    std::string function = it_func->GetString();

    godot::Node* root = nullptr;
    if (auto* editor = godot::EditorInterface::get_singleton()) {
        root = editor->get_edited_scene_root();
    }
    std::string hint;
    godot::Node* node = util::resolve_scene_node(node_path, root, &hint);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT + " — " + hint);
        return e;
    }

    godot::Ref<godot::Script> script = node->get_script();
    if (script.is_valid() && !script->can_instantiate()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("script is not instantiable in the editor: " + util::to_std(script->get_path()) +
            " — scripts must be marked @tool to run in the editor, or run the game to execute non-tool scripts");
        return e;
    }

    godot::StringName func_name(function.c_str());
    if (!node->has_method(func_name)) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("method not found: " + function + " on node " + node_path);
        return e;
    }

    godot::Array args_arr;
    auto* it_arr = args.Find("args");
    if (!it_arr || !it_arr->IsArray()) {
        it_arr = args.Find("arguments");
    }
    if (it_arr && it_arr->IsArray()) {
        const auto& arr = it_arr->GetArray();
        for (const auto& a : arr) {
            args_arr.append(VariantJson::deserialize(a));
        }
    }

    godot::Variant result = node->callv(func_name, args_arr);
    if (result.get_type() == godot::Variant::NIL && script.is_valid() && !script->can_instantiate()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("method call failed on node " + node_path +
            ": script instance unavailable in the editor — scripts must be marked @tool to run in the editor, or run the game to execute non-tool scripts");
        return e;
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(result);
    return r;
}

mcp::JsonValue handle_reload(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    bool keep_state = false;
    auto* ks = args.Find("keep_state");
    if (ks && ks->IsBool()) keep_state = ks->GetBool();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load(godot::String(path.c_str()));
    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to load script: " + path);
        return e;
    }

    godot::Ref<godot::Script> script = res;
    if (script.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("loaded resource is not a Script: " + path);
        return e;
    }

    godot::Error err = script->reload(keep_state);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
    return r;
}

mcp::JsonValue handle_get_variable_list(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load(godot::String(path.c_str()));
    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to load script: " + path);
        return e;
    }

    godot::Ref<godot::Script> script = res;
    if (script.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("loaded resource is not a Script: " + path);
        return e;
    }

    godot::TypedArray<godot::Dictionary> props = script->get_script_property_list();
    mcp::JsonValue result(mcp::JsonValue::array_tag);
    for (int64_t i = 0; i < props.size(); i++) {
        godot::Dictionary dict = props[i];
        int usage = 0;
        if (dict.has("usage")) {
            usage = static_cast<int>(dict["usage"]);
        }
        if ((usage & PROPERTY_USAGE_CATEGORY) != 0 || (usage & PROPERTY_USAGE_INTERNAL) != 0) {
            continue;
        }
        mcp::JsonValue item(mcp::JsonValue::object_tag);
        item["usage"] = mcp::JsonValue(static_cast<int64_t>(usage));
        if (dict.has("name")) {
            item["name"] = mcp::JsonValue(util::to_std(dict["name"].operator godot::String()));
        }
        if (dict.has("type")) {
            int type_id = static_cast<int>(dict["type"]);
            item["type_id"] = mcp::JsonValue(static_cast<int64_t>(type_id));
            item["type"] = mcp::JsonValue(util::to_std(godot::Variant::get_type_name(
                static_cast<godot::Variant::Type>(type_id))));
        }
        result.PushBack(std::move(item));
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(result);
    return r;
}

} // namespace script_ops
} // namespace godot_self_driving
