#include "scene_ops.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>

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
    return root->get_node_or_null(np);
}

void node_to_json(godot::Node* node, const std::string& root_prefix, mcp::JsonValue& j) {
    if (!node) return;
    j["name"] = mcp::JsonValue(to_std(node->get_name()));
    j["type"] = mcp::JsonValue(to_std(node->get_class()));
    std::string abs_path = to_std(node->get_path());
    if (abs_path == root_prefix) {
        j["path"] = mcp::JsonValue(to_std(node->get_name()));
    } else if (abs_path.size() > root_prefix.size() && abs_path[root_prefix.size()] == '/' && abs_path.find(root_prefix) == 0) {
        j["path"] = mcp::JsonValue(abs_path.substr(root_prefix.size() + 1));
    } else {
        j["path"] = mcp::JsonValue(abs_path);
    }
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
    auto* pp = args.Find("parent_path");
    if (!pp || !pp->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: parent_path");
        return e;
    }
    std::string parent_path = pp->GetString();

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

    auto* parent = find_node(parent_path);
    if (!parent) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("parent node not found: " + parent_path);
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
    parent->add_child(obj);

    auto* editor = godot::EditorInterface::get_singleton();
    std::string abs_path = to_std(obj->get_path());
    if (editor) {
        auto* scene_root = editor->get_edited_scene_root();
        if (scene_root) {
            std::string root_pref = to_std(scene_root->get_path());
            if (abs_path == root_pref) {
                abs_path = name;
            } else if (abs_path.find(root_pref + "/") == 0) {
                abs_path = abs_path.substr(root_pref.size() + 1);
            }
        }
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["path"] = mcp::JsonValue(abs_path);
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
    node->queue_free();
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("deleted");
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
