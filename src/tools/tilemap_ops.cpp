#include "tilemap_ops.hpp"
#include "core/log_system.hpp"
#include <mcp/JsonValue.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/tile_map.hpp>
#include <godot_cpp/classes/tile_set.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>

namespace godot_self_driving {
namespace tilemap_ops {

using JV = mcp::JsonValue;

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

JV error(const std::string& msg) {
    JV e(JV::object_tag);
    e["error"] = JV(msg);
    return e;
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

} // namespace

JV handle_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "tilemap_create called");

    std::string name = "TileMap";
    auto* n = args.Find("name");
    if (n && n->IsString()) name = n->GetString();

    auto* cdbs = godot::ClassDBSingleton::get_singleton();
    if (!cdbs) return error("ClassDB singleton not available");

    godot::Variant obj_var = cdbs->instantiate(godot::StringName("TileMap"));
    if (obj_var.get_type() == godot::Variant::NIL)
        return error("failed to instantiate TileMap");

    auto* tile_map = godot::Object::cast_to<godot::TileMap>(obj_var);
    if (!tile_map) return error("instantiated object is not a TileMap");

    tile_map->set_name(godot::StringName(name.c_str()));

    int tile_size = 16;
    auto* ts = args.Find("tile_size");
    if (ts && ts->IsInt()) tile_size = static_cast<int>(ts->GetInt());

    {
        godot::Variant ts_var = cdbs->instantiate(godot::StringName("TileSet"));
        if (ts_var.get_type() != godot::Variant::NIL) {
            godot::Ref<godot::TileSet> tile_set = ts_var;
            if (tile_set.is_valid()) {
                tile_set->set("tile_size", godot::Vector2i(tile_size, tile_size));
                tile_map->set_tileset(tile_set);
            }
        }
    }

    int quadrant_size = 16;
    auto* qs = args.Find("format");
    if (qs && qs->IsInt()) quadrant_size = static_cast<int>(qs->GetInt());
    tile_map->set("cell_quadrant_size", quadrant_size);

    auto* editor = godot::EditorInterface::get_singleton();
    auto* pp = args.Find("parent_path");
    bool has_parent = pp && pp->IsString() && !pp->GetString().empty();

    if (has_parent) {
        auto* parent = find_node(pp->GetString());
        if (!parent) return error("parent node not found: " + pp->GetString());
        parent->add_child(tile_map);
        if (editor) {
            auto* scene_root = editor->get_edited_scene_root();
            if (scene_root) tile_map->set_owner(scene_root);
        }
    } else if (editor) {
        auto* existing_root = editor->get_edited_scene_root();
        if (existing_root) {
            existing_root->add_child(tile_map);
            tile_map->set_owner(existing_root);
        } else {
            editor->add_root_node(tile_map);
        }
    }

    std::string result_path = name;
    if (editor) {
        auto* scene_root = editor->get_edited_scene_root();
        if (scene_root) {
            std::string abs = to_std(tile_map->get_path());
            std::string root_pref = to_std(scene_root->get_path());
            if (abs == root_pref) result_path = name;
            else if (abs.find(root_pref + "/") == 0) result_path = abs.substr(root_pref.size() + 1);
            else result_path = abs;
        }
    }

    JV r(JV::object_tag);
    JV info(JV::object_tag);
    info["class"] = JV("TileMap");
    info["name"] = JV(name);
    info["path"] = JV(result_path);
    info["object_id"] = JV(static_cast<int64_t>(tile_map->get_instance_id()));
    info["object_id_str"] = JV(std::to_string(static_cast<int64_t>(tile_map->get_instance_id())));
    r["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "tilemap_create completed");
    return r;
}

JV handle_set_cell(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "tilemap_set_cell called");

    auto* p = args.Find("path");
    if (!p || !p->IsString()) return error("missing required parameter: path");

    auto* node = find_node(p->GetString());
    if (!node) return error("node not found: " + p->GetString());

    auto* tile_map = godot::Object::cast_to<godot::TileMap>(node);
    if (!tile_map) return error("node is not a TileMap: " + p->GetString());

    auto* x = args.Find("x");
    auto* y = args.Find("y");
    if (!x || !x->IsInt()) return error("missing required parameter: x");
    if (!y || !y->IsInt()) return error("missing required parameter: y");

    int layer = 0;
    auto* la = args.Find("layer");
    if (la && la->IsInt()) layer = static_cast<int>(la->GetInt());

    int source_id = 0;
    auto* si = args.Find("source_id");
    if (si && si->IsInt()) source_id = static_cast<int>(si->GetInt());

    godot::Vector2i atlas_coords(-1, -1);
    auto* ac = args.Find("atlas_coords");
    if (ac && ac->IsObject()) {
        auto* acx = ac->Find("x");
        auto* acy = ac->Find("y");
        if (acx && acx->IsInt() && acy && acy->IsInt())
            atlas_coords = godot::Vector2i(static_cast<int>(acx->GetInt()), static_cast<int>(acy->GetInt()));
    }

    tile_map->set_cell(layer,
        godot::Vector2i(static_cast<int>(x->GetInt()), static_cast<int>(y->GetInt())),
        source_id, atlas_coords);

    JV r(JV::object_tag);
    r["result"] = JV("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "tilemap_set_cell completed");
    return r;
}

JV handle_set_cells(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "tilemap_set_cells called");

    auto* p = args.Find("node_path");
    if (!p || !p->IsString()) return error("missing required parameter: node_path");

    auto* node = find_node(p->GetString());
    if (!node) return error("node not found: " + p->GetString());

    auto* tile_map = godot::Object::cast_to<godot::TileMap>(node);
    if (!tile_map) return error("node is not a TileMap: " + p->GetString());

    auto* cells = args.Find("cells");
    if (!cells || !cells->IsArray()) return error("missing required parameter: cells");

    int layer = 0;
    auto* la = args.Find("layer");
    if (la && la->IsInt()) layer = static_cast<int>(la->GetInt());

    const auto& arr = cells->GetArray();
    int set_count = 0;
    int failed_count = 0;
    std::string failure_detail;
    for (size_t i = 0; i < arr.size(); ++i) {
        const auto& cell = arr[i];
        auto* cx = cell.Find("x");
        auto* cy = cell.Find("y");
        auto* si = cell.Find("source_id");
        std::string failure;
        if (!cell.IsObject()) {
            failure = "expected an object";
        } else if (!cx || !cx->IsInt()) {
            failure = "missing required parameter: x";
        } else if (!cy || !cy->IsInt()) {
            failure = "missing required parameter: y";
        } else if (!si || !si->IsInt()) {
            failure = "missing required parameter: source_id";
        }
        if (!failure.empty()) {
            if (failed_count > 0) failure_detail += "; ";
            failure_detail += "cells[" + std::to_string(i) + "]: " + failure;
            ++failed_count;
            continue;
        }

        godot::Vector2i atlas_coords(0, 0);
        auto* ac = cell.Find("atlas_coords");
        if (ac && ac->IsObject()) {
            auto* acx = ac->Find("x");
            auto* acy = ac->Find("y");
            if (acx && acx->IsInt() && acy && acy->IsInt())
                atlas_coords = godot::Vector2i(static_cast<int>(acx->GetInt()), static_cast<int>(acy->GetInt()));
        }

        tile_map->set_cell(layer,
            godot::Vector2i(static_cast<int>(cx->GetInt()), static_cast<int>(cy->GetInt())),
            static_cast<int>(si->GetInt()), atlas_coords);
        ++set_count;
    }

    if (failed_count > 0) {
        JV e(JV::object_tag);
        e["error"] = JV("invalid cells entries: " + failure_detail);
        e["warnings"] = JV(std::to_string(failed_count) + " cells skipped");
        return e;
    }

    JV r(JV::object_tag);
    JV info(JV::object_tag);
    info["set_count"] = JV(set_count);
    info["layer"] = JV(layer);
    r["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "tilemap_set_cells completed");
    return r;
}

JV handle_tileset_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "tileset_create called");

    std::string name = "TileSet";
    auto* n = args.Find("name");
    if (n && n->IsString()) name = n->GetString();

    int tile_size = 16;
    auto* ts = args.Find("tile_size");
    if (ts && ts->IsInt()) tile_size = static_cast<int>(ts->GetInt());

    auto* cdbs = godot::ClassDBSingleton::get_singleton();
    if (!cdbs) return error("ClassDB singleton not available");

    godot::Variant ts_var = cdbs->instantiate(godot::StringName("TileSet"));
    if (ts_var.get_type() == godot::Variant::NIL)
        return error("failed to instantiate TileSet");

    godot::Ref<godot::TileSet> tile_set = ts_var;
    if (tile_set.is_null()) return error("instantiated object is not a TileSet");

    tile_set->set("resource_name", godot::String(name.c_str()));
    tile_set->set("tile_size", godot::Vector2i(tile_size, tile_size));

    JV r(JV::object_tag);
    JV info(JV::object_tag);
    info["class"] = JV("TileSet");
    info["name"] = JV(name);
    info["object_id"] = JV(static_cast<int64_t>(tile_set->get_instance_id()));
    info["object_id_str"] = JV(std::to_string(static_cast<int64_t>(tile_set->get_instance_id())));
    r["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "tileset_create completed");
    return r;
}

} // namespace tilemap_ops
} // namespace godot_self_driving
