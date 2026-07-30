#include "group_ops.hpp"
#include "core/log_system.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>

namespace godot_self_driving {
namespace group_ops {

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

    auto* node = root->get_node_or_null(godot::NodePath(godot::String(clean.c_str())));
    if (!node) return nullptr;

    auto* p = node->get_parent();
    while (p) {
        if (p == root) return node;
        p = p->get_parent();
    }
    return nullptr;
}

} // namespace

mcp::JsonValue handle_add_node_to_group(const mcp::JsonValue& args) {
    auto* np = args.Find("node_path");
    auto* gp = args.Find("group_name");

    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: node_path");
        return e;
    }
    if (!gp || !gp->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: group_name");
        return e;
    }

    std::string node_path = np->GetString();
    std::string group_name = gp->GetString();

    auto* node = find_node(node_path);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path);
        return e;
    }

    node->add_to_group(godot::StringName(group_name.c_str()));

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("added");
    r["node_path"] = mcp::JsonValue(node_path);
    r["group"] = mcp::JsonValue(group_name);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        "group_add_node_to_group: " + node_path + " -> " + group_name);
    return r;
}

mcp::JsonValue handle_remove_node_from_group(const mcp::JsonValue& args) {
    auto* np = args.Find("node_path");
    auto* gp = args.Find("group_name");

    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: node_path");
        return e;
    }
    if (!gp || !gp->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: group_name");
        return e;
    }

    std::string node_path = np->GetString();
    std::string group_name = gp->GetString();

    auto* node = find_node(node_path);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path);
        return e;
    }

    node->remove_from_group(godot::StringName(group_name.c_str()));

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("removed");
    r["node_path"] = mcp::JsonValue(node_path);
    r["group"] = mcp::JsonValue(group_name);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        "group_remove_node_from_group: " + node_path + " -> " + group_name);
    return r;
}

mcp::JsonValue handle_has_node_in_group(const mcp::JsonValue& args) {
    auto* np = args.Find("node_path");
    auto* gp = args.Find("group_name");

    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: node_path");
        return e;
    }
    if (!gp || !gp->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: group_name");
        return e;
    }

    std::string node_path = np->GetString();
    std::string group_name = gp->GetString();

    auto* node = find_node(node_path);
    if (!node) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("node not found: " + node_path);
        return e;
    }

    bool in_group = node->is_in_group(godot::StringName(group_name.c_str()));

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(in_group);
    return r;
}

} // namespace group_ops
} // namespace godot_self_driving
