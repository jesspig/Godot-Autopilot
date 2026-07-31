#include "scene_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

namespace godot_self_driving {
namespace scene_ops {

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

godot::Node* find_node(const std::string& path_str) {
    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) return nullptr;
    auto* root = editor->get_edited_scene_root();
    if (!root) return nullptr;
    std::string clean = path_str;
    if (!clean.empty() && clean[0] == '/') {
        clean = clean.substr(1);
    }
    if (clean.empty() || clean == to_std(root->get_name())) {
        return root;
    }
    godot::NodePath np(godot::String(clean.c_str()));
    auto* node = root->get_node_or_null(np);
    if (!node) {
        // Strip root name prefix if present (e.g. "Game/Player" → "Player" when root is "Game")
        std::string root_name = to_std(root->get_name());
        if (clean.size() > root_name.size() + 1 &&
            clean.compare(0, root_name.size(), root_name) == 0 &&
            clean[root_name.size()] == '/') {
            std::string sub = clean.substr(root_name.size() + 1);
            if (!sub.empty()) {
                node = root->get_node_or_null(godot::NodePath(godot::String(sub.c_str())));
            }
        }
    }
    if (!node) return nullptr;
    if (node == root) return node;
    auto* p = node->get_parent();
    while (p) {
        if (p == root) return node;
        p = p->get_parent();
    }
    return root;
}

void node_to_json(godot::Node* node, const std::string& root_prefix, mcp::JsonValue& j) {
    if (!node) return;
    j["name"] = mcp::JsonValue(to_std(node->get_name()));
    j["type"] = mcp::JsonValue(to_std(node->get_class()));
    // 从 scene root 到 node 的完整路径
    godot::Node* root = godot::EditorInterface::get_singleton()
                            ? godot::EditorInterface::get_singleton()->get_edited_scene_root()
                            : nullptr;
    godot::Node* n = node;
    std::vector<std::string> parts;
    while (n && n != root) {
        parts.push_back(to_std(n->get_name()));
        n = n->get_parent();
    }
    std::reverse(parts.begin(), parts.end());
    std::string path;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) path += "/";
        path += parts[i];
    }
    if (path.empty()) path = to_std(node->get_name());
    j["path"] = mcp::JsonValue(path);
    j["children"] = mcp::JsonValue(mcp::JsonValue::array_tag);
    auto children = node->get_children();
    for (int i = 0; i < children.size(); i++) {
        auto* child = godot::Object::cast_to<godot::Node>(children[i]);
        if (child) {
            mcp::JsonValue child_j(mcp::JsonValue::object_tag);
            node_to_json(child, root_prefix, child_j);
            j["children"].PushBack(std::move(child_j));
        }
    }
}

} // namespace

mcp::JsonValue handle_create(const mcp::JsonValue& args) {
    std::string name = "NewNode";
    auto* n = args.Find("name");
    if (n && n->IsString()) name = n->GetString();

    std::string type = "Node";
    auto* t = args.Find("type");
    if (t && t->IsString()) type = t->GetString();

    auto* cdbs = godot::ClassDBSingleton::get_singleton();
    if (!cdbs) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ClassDB singleton not available");
        return e;
    }

    if (!cdbs->is_parent_class(godot::StringName(type.c_str()), godot::StringName("Node"))) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(type + " is not a Node subclass");
        return e;
    }

    godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
    if (obj_var.get_type() == godot::Variant::NIL) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to instantiate node type: " + type);
        return e;
    }

    auto* obj = godot::Object::cast_to<godot::Node>(obj_var);
    if (!obj) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("instantiated object is not a Node: " + type);
        return e;
    }

    obj->set_name(godot::StringName(name.c_str()));

    auto* pp = args.Find("parent_path");
    bool has_parent = pp && pp->IsString() && !pp->GetString().empty();

    auto* editor = godot::EditorInterface::get_singleton();

    if (has_parent) {
        std::string parent_path = pp->GetString();
        auto* parent = find_node(parent_path);
        if (!parent) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("parent node not found: " + parent_path);
            return e;
        }
        parent->add_child(obj);
        auto* scene_root = editor->get_edited_scene_root();
        if (scene_root) obj->set_owner(scene_root);
    } else {
        if (!editor) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("EditorInterface not available");
            return e;
        }
        auto* existing_root = editor->get_edited_scene_root();
        if (existing_root) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("scene already has a root, use parent_path to add children");
            return e;
        }
        editor->add_root_node(obj);
    }

    // 根据节点类型自动切换视口
    if (editor) {
        auto* scene_root = editor->get_edited_scene_root();
        if (scene_root) {
            godot::StringName type_sn(type.c_str());
            if (cdbs->is_parent_class(type_sn, godot::StringName("Node2D"))) {
                editor->set_main_screen_editor(godot::String("2D"));
            } else if (cdbs->is_parent_class(type_sn, godot::StringName("Node3D"))) {
                editor->set_main_screen_editor(godot::String("3D"));
            }
        }
    }

    std::string result_path = name;
    if (editor) {
        auto* scene_root = editor->get_edited_scene_root();
        if (scene_root) {
            std::string abs_path = to_std(obj->get_path());
            std::string root_pref = to_std(scene_root->get_path());
            if (abs_path == root_pref) {
                result_path = name;
            } else if (abs_path.find(root_pref + "/") == 0) {
                result_path = abs_path.substr(root_pref.size() + 1);
            } else {
                result_path = abs_path;
            }
        } else {
            result_path = name;
        }
    } else {
        result_path = name;
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["path"] = mcp::JsonValue(result_path);
    inner["undo"] = mcp::JsonValue("delete node " + result_path + " (scene_node_delete)");
    r["result"] = std::move(inner);
    return r;
}

mcp::JsonValue handle_delete(const mcp::JsonValue& args) {
    auto* p = args.Find("path");
    if (!p || !p->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = p->GetString();
    auto* node = find_node(path);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + path);
        return e;
    }

    auto* editor = godot::EditorInterface::get_singleton();
    auto* scene_root = editor ? editor->get_edited_scene_root() : nullptr;
    if (node == scene_root) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "cannot delete the scene root node — use editor_close_scene to close the scene, then editor_new_scene");
        return e;
    }

    std::string node_name = to_std(node->get_name());
    std::string node_type = to_std(node->get_class());
    std::string parent_path;
    auto* parent = node->get_parent();
    if (parent) {
        std::string abs_parent = to_std(parent->get_path());
        if (scene_root) {
            std::string root_pref = to_std(scene_root->get_path());
            if (abs_parent == root_pref) {
                parent_path = to_std(parent->get_name());
            } else if (abs_parent.find(root_pref + "/") == 0) {
                parent_path = abs_parent.substr(root_pref.size() + 1);
            } else {
                parent_path = abs_parent;
            }
        } else {
            parent_path = abs_parent;
        }
    }

    node->queue_free();

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("deleted");
    mcp::JsonValue undo_info(mcp::JsonValue::object_tag);
    undo_info["name"] = mcp::JsonValue(node_name);
    undo_info["type"] = mcp::JsonValue(node_type);
    undo_info["parent_path"] = mcp::JsonValue(parent_path);
    undo_info["hint"] = mcp::JsonValue(
        "recreate node " + node_name + " (" + node_type + ") under " + parent_path + " (scene_node_create)");
    r["undo"] = std::move(undo_info);
    return r;
}

mcp::JsonValue handle_instance(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "scene_instance called");

    auto* p = args.Find("path");
    if (!p || !p->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = p->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load(godot::String(path.c_str()));
    auto* packed_scene = godot::Object::cast_to<godot::PackedScene>(res.ptr());
    if (!packed_scene) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to load packed scene: " + path);
        return e;
    }

    godot::Node* instance = packed_scene->instantiate();
    if (!instance) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to instantiate scene: " + path);
        return e;
    }

    auto* editor = godot::EditorInterface::get_singleton();
    auto* scene_root = editor ? editor->get_edited_scene_root() : nullptr;

    auto* pp = args.Find("parent_path");
    godot::Node* parent = nullptr;
    if (pp && pp->IsString() && !pp->GetString().empty()) {
        parent = find_node(pp->GetString());
        if (!parent) {
            memdelete(instance);
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("parent node not found: " + pp->GetString());
            return e;
        }
    } else if (!scene_root) {
        memdelete(instance);
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("no scene open");
        return e;
    } else {
        parent = scene_root;
    }

    auto* n = args.Find("name");
    if (n && n->IsString() && !n->GetString().empty()) {
        instance->set_name(godot::StringName(n->GetString().c_str()));
    }

    parent->add_child(instance);

    bool set_owner = true;
    auto* o = args.Find("owner");
    if (o && o->IsBool()) set_owner = o->GetBool();
    if (set_owner && scene_root) {
        instance->set_owner(scene_root);
    }

    std::string instance_name = to_std(instance->get_name());
    std::string instance_type = to_std(instance->get_class());
    std::string result_path = instance_name;
    if (scene_root) {
        std::string abs_path = to_std(instance->get_path());
        std::string root_pref = to_std(scene_root->get_path());
        if (abs_path == root_pref) {
            result_path = instance_name;
        } else if (abs_path.find(root_pref + "/") == 0) {
            result_path = abs_path.substr(root_pref.size() + 1);
        } else {
            result_path = abs_path;
        }
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["path"] = mcp::JsonValue(result_path);
    inner["name"] = mcp::JsonValue(instance_name);
    inner["type"] = mcp::JsonValue(instance_type);
    inner["undo"] = mcp::JsonValue("delete node " + result_path + " (scene_node_delete)");
    r["result"] = std::move(inner);

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "scene_instance completed");
    return r;
}

mcp::JsonValue handle_get_tree(const mcp::JsonValue&) {
    auto* editor = godot::EditorInterface::get_singleton();
    godot::Node* root = nullptr;
    if (editor) {
        root = editor->get_edited_scene_root();
    }
    if (!root) {
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = mcp::JsonValue(nullptr);
        return r;
    }
    std::string root_prefix = to_std(root->get_path());
    mcp::JsonValue result(mcp::JsonValue::object_tag);
    node_to_json(root, root_prefix, result);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(result);
    return r;
}

} // namespace scene_ops
} // namespace godot_self_driving
