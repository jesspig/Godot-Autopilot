#include "script_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include "tools/property_ops.hpp"
#include "tools/scene_ops.hpp"
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
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

constexpr const char* NODE_PATH_HINT = " — expected scene-relative path like 'Level1/Player' or absolute '/root/Level1/Player'";

constexpr int PROPERTY_USAGE_CATEGORY = 0x80;
constexpr int PROPERTY_USAGE_INTERNAL = 0x08;

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

godot::Node* resolve_node(const std::string& path_str) {
    auto* editor = godot::EditorInterface::get_singleton();
    godot::Node* root = nullptr;
    if (editor) {
        root = editor->get_edited_scene_root();
    }
    if (!root) return nullptr;
    std::string clean = path_str;
    if (!clean.empty() && clean[0] == '/') {
        clean = clean.substr(1);
    }
    if (clean.empty() || clean == to_std(root->get_name())) {
        return root;
    }
    return root->get_node_or_null(godot::NodePath(clean.c_str()));
}

mcp::JsonValue serialize_resource(const godot::Ref<godot::Resource>& res) {
    if (res.is_null()) return mcp::JsonValue(nullptr);
    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["class"] = mcp::JsonValue(to_std(res->get_class()));
    j["path"] = mcp::JsonValue(to_std(res->get_path()));
    j["object_id"] = mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
    j["object_id_str"] = mcp::JsonValue(std::to_string(static_cast<int64_t>(res->get_instance_id())));
    return j;
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

    // GDScript wrapping (single execution path)
    auto* editor = godot::EditorInterface::get_singleton();
    godot::Node* scene_root = editor ? editor->get_edited_scene_root() : nullptr;

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
    wrapped = "@tool\nextends Node\n\nfunc _run():\n";
    if (!cleaned.empty()) {
        std::istringstream stream(cleaned);
        std::string line;
        bool first_line = true;
        while (std::getline(stream, line)) {
            if (!first_line) wrapped += "\n";
            first_line = false;
            size_t content_start = line.find_first_not_of(" \t");
            if (content_start == std::string::npos) {
                wrapped += "    ";
            } else {
                wrapped += "    " + line;
            }
        }
    }
    wrapped += "\n";

    godot::Ref<godot::GDScript> script;
    script.instantiate();
    if (script.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to create GDScript instance");
        return e;
    }

    script->set_source_code(godot::String(wrapped.c_str()));
    godot::Error parse_err2 = script->reload();
    if (parse_err2 != godot::OK) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("GDScript compilation failed: ERR_PARSE_ERROR (code " + std::to_string(static_cast<int>(parse_err2)) + ")");
        return e;
    }

    godot::Node* temp_node = memnew(godot::Node);
    temp_node->set_script(godot::Variant(script));

    bool temp_added = false;
    if (scene_root) {
        scene_root->add_child(temp_node);
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

    godot::Variant result = temp_node->call(fn_name);

    if (temp_added && temp_node->get_parent()) temp_node->get_parent()->remove_child(temp_node);
    memdelete(temp_node);

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(result);
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

    if (!overwrite && godot::FileAccess::file_exists(godot::String(path.c_str()))) {
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
    godot::Error reload_err = script->reload();
    if (reload_err != godot::OK) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("script compilation failed: ERR_PARSE_ERROR (code " + std::to_string(static_cast<int>(reload_err)) + ")");
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

    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["path"] = mcp::JsonValue(path);
    j["class"] = mcp::JsonValue("GDScript");
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

    godot::Node* node = resolve_node(node_path);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT);
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

    godot::Node* node = resolve_node(node_path);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT);
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
        godot::Node* node = resolve_node(node_path);
        if (!node) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT);
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

    godot::Node* node = resolve_node(node_path);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT);
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

    godot::Node* node = resolve_node(node_path);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path + NODE_PATH_HINT);
        return e;
    }

    godot::Ref<godot::Script> script = node->get_script();
    if (script.is_valid() && !script->can_instantiate()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("script is not instantiable in the editor: " + to_std(script->get_path()) +
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
            item["name"] = mcp::JsonValue(to_std(dict["name"].operator godot::String()));
        }
        if (dict.has("type")) {
            int type_id = static_cast<int>(dict["type"]);
            item["type_id"] = mcp::JsonValue(static_cast<int64_t>(type_id));
            item["type"] = mcp::JsonValue(to_std(godot::Variant::get_type_name(
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
