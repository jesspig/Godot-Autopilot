#include "render_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <mcp/JsonValue.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <string>

namespace godot_self_driving {
namespace render_ops {

using JV = mcp::JsonValue;

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

godot::RID rid_from_json(const JV& j) {
    return godot::UtilityFunctions::rid_from_int64(j.GetInt());
}

godot::Color parse_color(const JV& j) {
    auto* r = j.Find("r"); auto* g = j.Find("g"); auto* b = j.Find("b"); auto* a = j.Find("a");
    return godot::Color(
        static_cast<float>(r && r->IsNumber() ? (r->IsInt() ? static_cast<double>(r->GetInt()) : r->GetDouble()) : 0.0),
        static_cast<float>(g && g->IsNumber() ? (g->IsInt() ? static_cast<double>(g->GetInt()) : g->GetDouble()) : 0.0),
        static_cast<float>(b && b->IsNumber() ? (b->IsInt() ? static_cast<double>(b->GetInt()) : b->GetDouble()) : 0.0),
        static_cast<float>(a && a->IsNumber() ? (a->IsInt() ? static_cast<double>(a->GetInt()) : a->GetDouble()) : 1.0));
}

godot::Vector2 parse_vector2(const JV& j) {
    auto* x = j.Find("x"); auto* y = j.Find("y");
    return godot::Vector2(
        x && x->IsNumber() ? (x->IsInt() ? static_cast<double>(x->GetInt()) : x->GetDouble()) : 0.0,
        y && y->IsNumber() ? (y->IsInt() ? static_cast<double>(y->GetInt()) : y->GetDouble()) : 0.0);
}

godot::Vector3 parse_vector3(const JV& j) {
    auto* x = j.Find("x"); auto* y = j.Find("y"); auto* z = j.Find("z");
    return godot::Vector3(
        x && x->IsNumber() ? (x->IsInt() ? static_cast<double>(x->GetInt()) : x->GetDouble()) : 0.0,
        y && y->IsNumber() ? (y->IsInt() ? static_cast<double>(y->GetInt()) : y->GetDouble()) : 0.0,
        z && z->IsNumber() ? (z->IsInt() ? static_cast<double>(z->GetInt()) : z->GetDouble()) : 0.0);
}

godot::Rect2 parse_rect2(const JV& j) {
    return godot::Rect2(
        parse_vector2(j.At("position")),
        parse_vector2(j.At("size")));
}

JV rid_to_json(const godot::RID& rid) {
    return JV::FromObject({{"rid", JV(static_cast<int64_t>(rid.get_id()))}});
}

} // namespace

JV handle_canvas_item_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_canvas_item_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID rid = rs->canvas_item_create();
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_canvas_item_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_canvas_item_draw_rect(const JV& args) {
    auto* it_ci = args.Find("canvas_item_rid");
    auto* it_rect = args.Find("rect");
    auto* it_color = args.Find("color");
    if (!it_ci || !it_ci->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: canvas_item_rid"); return r;
    }
    if (!it_rect || !it_rect->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: rect"); return r;
    }
    if (!it_color || !it_color->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: color"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_canvas_item_draw_rect called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID ci = rid_from_json(*it_ci);
    godot::Rect2 r = parse_rect2(*it_rect);
    godot::Color c = parse_color(*it_color);
    auto* aa = args.Find("antialiased");
    bool antialiased = aa ? aa->GetBool() : false;

    rs->canvas_item_add_rect(ci, r, c, antialiased);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_canvas_item_draw_rect completed");
    JV ret(JV::object_tag); ret["result"] = JV("ok"); return ret;
}

JV handle_canvas_item_draw_circle(const JV& args) {
    auto* it_ci = args.Find("canvas_item_rid");
    auto* it_pos = args.Find("position");
    auto* it_color = args.Find("color");
    if (!it_ci || !it_ci->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: canvas_item_rid"); return r;
    }
    if (!it_pos || !it_pos->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: position"); return r;
    }
    if (!it_color || !it_color->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: color"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_canvas_item_draw_circle called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID ci = rid_from_json(*it_ci);
    godot::Vector2 pos = parse_vector2(*it_pos);
    auto* rad = args.Find("radius");
    float radius = static_cast<float>(rad && rad->IsNumber() ? (rad->IsInt() ? static_cast<double>(rad->GetInt()) : rad->GetDouble()) : 1.0);
    godot::Color c = parse_color(*it_color);
    auto* aa = args.Find("antialiased");
    bool antialiased = aa ? aa->GetBool() : false;

    rs->canvas_item_add_circle(ci, pos, radius, c, antialiased);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_canvas_item_draw_circle completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_canvas_item_draw_texture(const JV& args) {
    auto* it_ci = args.Find("canvas_item_rid");
    auto* it_tex = args.Find("texture_rid");
    auto* it_rect = args.Find("rect");
    if (!it_ci || !it_ci->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: canvas_item_rid"); return r;
    }
    if (!it_tex || !it_tex->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: texture_rid"); return r;
    }
    if (!it_rect || !it_rect->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: rect"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_canvas_item_draw_texture called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID ci = rid_from_json(*it_ci);
    godot::RID tex = rid_from_json(*it_tex);
    godot::Rect2 r = parse_rect2(*it_rect);
    auto* tile_a = args.Find("tile");
    bool tile = tile_a ? tile_a->GetBool() : false;
    godot::Color modulate(1, 1, 1, 1);
    auto* it_mod = args.Find("modulate");
    if (it_mod && it_mod->IsObject()) {
        modulate = parse_color(*it_mod);
    }
    auto* transp = args.Find("transpose");
    bool transpose = transp ? transp->GetBool() : false;

    rs->canvas_item_add_texture_rect(ci, r, tex, tile, modulate, transpose);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_canvas_item_draw_texture completed");
    JV ret(JV::object_tag); ret["result"] = JV("ok"); return ret;
}

JV handle_canvas_item_draw_line(const JV& args) {
    auto* it_ci = args.Find("canvas_item_rid");
    auto* it_from = args.Find("from");
    auto* it_to = args.Find("to");
    auto* it_color = args.Find("color");
    if (!it_ci || !it_ci->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: canvas_item_rid"); return r;
    }
    if (!it_from || !it_from->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: from"); return r;
    }
    if (!it_to || !it_to->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: to"); return r;
    }
    if (!it_color || !it_color->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: color"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_canvas_item_draw_line called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID ci = rid_from_json(*it_ci);
    godot::Vector2 from = parse_vector2(*it_from);
    godot::Vector2 to = parse_vector2(*it_to);
    godot::Color c = parse_color(*it_color);
    auto* w = args.Find("width");
    float width = static_cast<float>(w && w->IsNumber() ? (w->IsInt() ? static_cast<double>(w->GetInt()) : w->GetDouble()) : -1.0);
    auto* aa = args.Find("antialiased");
    bool antialiased = aa ? aa->GetBool() : false;

    rs->canvas_item_add_line(ci, from, to, c, width, antialiased);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_canvas_item_draw_line completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_canvas_item_set_transform(const JV& args) {
    auto* it_ci = args.Find("canvas_item_rid");
    if (!it_ci || !it_ci->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: canvas_item_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_canvas_item_set_transform called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID ci = rid_from_json(*it_ci);
    godot::Transform2D xform;
    auto* xa = args.Find("x");
    auto* ya = args.Find("y");
    auto* ox = args.Find("origin_x");
    auto* oy = args.Find("origin_y");
    xform.columns[0] = godot::Vector2(xa && xa->IsNumber() ? (xa->IsInt() ? static_cast<double>(xa->GetInt()) : xa->GetDouble()) : 1.0, 0.0);
    xform.columns[1] = godot::Vector2(0.0, ya && ya->IsNumber() ? (ya->IsInt() ? static_cast<double>(ya->GetInt()) : ya->GetDouble()) : 1.0);
    xform.columns[2] = godot::Vector2(ox && ox->IsNumber() ? (ox->IsInt() ? static_cast<double>(ox->GetInt()) : ox->GetDouble()) : 0.0, oy && oy->IsNumber() ? (oy->IsInt() ? static_cast<double>(oy->GetInt()) : oy->GetDouble()) : 0.0);

    rs->canvas_item_set_transform(ci, xform);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_canvas_item_set_transform completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_canvas_item_set_visible(const JV& args) {
    auto* it_ci = args.Find("canvas_item_rid");
    if (!it_ci || !it_ci->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: canvas_item_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_canvas_item_set_visible called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID ci = rid_from_json(*it_ci);
    auto* vis = args.Find("visible");
    bool visible = vis ? vis->GetBool() : true;

    rs->canvas_item_set_visible(ci, visible);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_canvas_item_set_visible completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_scenario_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_scenario_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID rid = rs->scenario_create();
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_scenario_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_scenario_set_environment(const JV& args) {
    auto* it_sc = args.Find("scenario_rid");
    auto* it_env = args.Find("environment_rid");
    if (!it_sc || !it_sc->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: scenario_rid"); return r;
    }
    if (!it_env || !it_env->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: environment_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_scenario_set_environment called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID sc = rid_from_json(*it_sc);
    godot::RID env = rid_from_json(*it_env);

    rs->scenario_set_environment(sc, env);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_scenario_set_environment completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_camera_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_camera_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID rid = rs->camera_create();
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_camera_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_camera_set_transform(const JV& args) {
    auto* it_cam = args.Find("camera_rid");
    if (!it_cam || !it_cam->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: camera_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_camera_set_transform called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID cam = rid_from_json(*it_cam);
    godot::Transform3D xform;
    auto* ox = args.Find("origin_x");
    auto* oy = args.Find("origin_y");
    auto* oz = args.Find("origin_z");
    xform.origin = godot::Vector3(
        ox && ox->IsNumber() ? (ox->IsInt() ? static_cast<double>(ox->GetInt()) : ox->GetDouble()) : 0.0,
        oy && oy->IsNumber() ? (oy->IsInt() ? static_cast<double>(oy->GetInt()) : oy->GetDouble()) : 0.0,
        oz && oz->IsNumber() ? (oz->IsInt() ? static_cast<double>(oz->GetInt()) : oz->GetDouble()) : 0.0);

    rs->camera_set_transform(cam, xform);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_camera_set_transform completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_camera_set_perspective(const JV& args) {
    auto* it_cam = args.Find("camera_rid");
    if (!it_cam || !it_cam->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: camera_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_camera_set_perspective called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID cam = rid_from_json(*it_cam);
    auto* fovy_a = args.Find("fovy_degrees");
    auto* znear_a = args.Find("z_near");
    auto* zfar_a = args.Find("z_far");
    float fovy = static_cast<float>(fovy_a && fovy_a->IsNumber() ? (fovy_a->IsInt() ? static_cast<double>(fovy_a->GetInt()) : fovy_a->GetDouble()) : 75.0);
    float znear = static_cast<float>(znear_a && znear_a->IsNumber() ? (znear_a->IsInt() ? static_cast<double>(znear_a->GetInt()) : znear_a->GetDouble()) : 0.01);
    float zfar = static_cast<float>(zfar_a && zfar_a->IsNumber() ? (zfar_a->IsInt() ? static_cast<double>(zfar_a->GetInt()) : zfar_a->GetDouble()) : 4000.0);

    rs->camera_set_perspective(cam, fovy, znear, zfar);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_camera_set_perspective completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_camera_set_orthogonal(const JV& args) {
    auto* it_cam = args.Find("camera_rid");
    if (!it_cam || !it_cam->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: camera_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_camera_set_orthogonal called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID cam = rid_from_json(*it_cam);
    auto* size_a = args.Find("size");
    auto* znear_a = args.Find("z_near");
    auto* zfar_a = args.Find("z_far");
    float size = static_cast<float>(size_a && size_a->IsNumber() ? (size_a->IsInt() ? static_cast<double>(size_a->GetInt()) : size_a->GetDouble()) : 10.0);
    float znear = static_cast<float>(znear_a && znear_a->IsNumber() ? (znear_a->IsInt() ? static_cast<double>(znear_a->GetInt()) : znear_a->GetDouble()) : 0.01);
    float zfar = static_cast<float>(zfar_a && zfar_a->IsNumber() ? (zfar_a->IsInt() ? static_cast<double>(zfar_a->GetInt()) : zfar_a->GetDouble()) : 4000.0);

    rs->camera_set_orthogonal(cam, size, znear, zfar);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_camera_set_orthogonal completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_light_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_light_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    auto* type_a = args.Find("type");
    std::string type = type_a ? type_a->GetString() : "directional";
    godot::RID rid;
    if (type == "directional") {
        rid = rs->directional_light_create();
    } else if (type == "omni") {
        rid = rs->omni_light_create();
    } else if (type == "spot") {
        rid = rs->spot_light_create();
    } else {
        JV r(JV::object_tag); r["error"] = JV("unknown light type: " + type + " (use directional, omni, or spot)"); return r;
    }

    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_light_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_light_set_param(const JV& args) {
    auto* it_light = args.Find("light_rid");
    auto* it_param = args.Find("param");
    auto* it_val = args.Find("value");
    if (!it_light || !it_light->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: light_rid"); return r;
    }
    if (!it_param || !it_param->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: param"); return r;
    }
    if (!it_val || !it_val->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: value"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_light_set_param called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID light = rid_from_json(*it_light);
    auto param = static_cast<godot::RenderingServer::LightParam>(static_cast<int>(it_param->GetInt()));
    float val = static_cast<float>(it_val->IsInt() ? static_cast<double>(it_val->GetInt()) : it_val->GetDouble());

    rs->light_set_param(light, param, val);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_light_set_param completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_light_set_color(const JV& args) {
    auto* it_light = args.Find("light_rid");
    auto* it_color = args.Find("color");
    if (!it_light || !it_light->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: light_rid"); return r;
    }
    if (!it_color || !it_color->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: color"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_light_set_color called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID light = rid_from_json(*it_light);
    godot::Color c = parse_color(*it_color);

    rs->light_set_color(light, c);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_light_set_color completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_mesh_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_mesh_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID rid = rs->mesh_create();
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_mesh_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_mesh_add_surface(const JV& args) {
    auto* it_mesh = args.Find("mesh_rid");
    if (!it_mesh || !it_mesh->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: mesh_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_mesh_add_surface called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID mesh = rid_from_json(*it_mesh);
    auto* prim_a = args.Find("primitive");
    auto primitive = static_cast<godot::RenderingServer::PrimitiveType>(
        prim_a ? static_cast<int>(prim_a->GetInt()) : static_cast<int>(godot::RenderingServer::PRIMITIVE_TRIANGLES));

    godot::Array arrays;
    arrays.resize(godot::RenderingServer::ARRAY_MAX);

    auto* it_arrays = args.Find("arrays");
    if (it_arrays && it_arrays->IsObject()) {
        const JV& arr = *it_arrays;

        auto* it_verts = arr.Find("vertices");
        if (it_verts && it_verts->IsArray()) {
            godot::PackedVector3Array verts;
            const JV::Array& vert_list = it_verts->GetArray();
            for (const auto& v : vert_list) {
                verts.append(parse_vector3(v));
            }
            arrays[godot::RenderingServer::ARRAY_VERTEX] = verts;
        }

        auto* it_norms = arr.Find("normals");
        if (it_norms && it_norms->IsArray()) {
            godot::PackedVector3Array norms;
            const JV::Array& norm_list = it_norms->GetArray();
            for (const auto& n : norm_list) {
                norms.append(parse_vector3(n));
            }
            arrays[godot::RenderingServer::ARRAY_NORMAL] = norms;
        }

        auto* it_tang = arr.Find("tangents");
        if (it_tang && it_tang->IsArray()) {
            godot::PackedFloat32Array tangs;
            const JV::Array& tang_list = it_tang->GetArray();
            for (const auto& t : tang_list) {
                tangs.append(static_cast<float>(t.IsInt() ? static_cast<double>(t.GetInt()) : t.GetDouble()));
            }
            arrays[godot::RenderingServer::ARRAY_TANGENT] = tangs;
        }

        auto* it_colors = arr.Find("colors");
        if (it_colors && it_colors->IsArray()) {
            godot::PackedColorArray cols;
            const JV::Array& color_list = it_colors->GetArray();
            for (const auto& c : color_list) {
                cols.append(parse_color(c));
            }
            arrays[godot::RenderingServer::ARRAY_COLOR] = cols;
        }

        auto* it_uvs = arr.Find("uvs");
        if (it_uvs && it_uvs->IsArray()) {
            godot::PackedVector2Array uvs;
            const JV::Array& uv_list = it_uvs->GetArray();
            for (const auto& uv : uv_list) {
                uvs.append(parse_vector2(uv));
            }
            arrays[godot::RenderingServer::ARRAY_TEX_UV] = uvs;
        }

        auto* it_idx = arr.Find("indices");
        if (it_idx && it_idx->IsArray()) {
            godot::PackedInt32Array idx;
            const JV::Array& idx_list = it_idx->GetArray();
            for (const auto& i : idx_list) {
                idx.append(static_cast<int32_t>(i.GetInt()));
            }
            arrays[godot::RenderingServer::ARRAY_INDEX] = idx;
        }
    }

    rs->mesh_add_surface_from_arrays(mesh, primitive, arrays);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_mesh_add_surface completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_mesh_set_material(const JV& args) {
    auto* it_mesh = args.Find("mesh_rid");
    auto* it_surf = args.Find("surface");
    auto* it_mat = args.Find("material_rid");
    if (!it_mesh || !it_mesh->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: mesh_rid"); return r;
    }
    if (!it_surf || !it_surf->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: surface"); return r;
    }
    if (!it_mat || !it_mat->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: material_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_mesh_set_material called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID mesh = rid_from_json(*it_mesh);
    int surface = static_cast<int>(it_surf->GetInt());
    godot::RID material = rid_from_json(*it_mat);

    rs->mesh_surface_set_material(mesh, surface, material);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_mesh_set_material completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_material_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_material_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID rid = rs->material_create();
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_material_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_material_set_param(const JV& args) {
    auto* it_mat = args.Find("material_rid");
    auto* it_param = args.Find("parameter");
    auto* it_val = args.Find("value");
    if (!it_mat || !it_mat->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: material_rid"); return r;
    }
    if (!it_param || !it_param->IsString()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: parameter"); return r;
    }
    if (!it_val) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: value"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_material_set_param called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID material = rid_from_json(*it_mat);
    godot::StringName param_name(it_param->GetString().c_str());
    auto* th = args.Find("type_hint");
    std::string type_hint = th ? th->GetString() : "";
    godot::Variant val = VariantJson::deserialize(*it_val, type_hint);

    rs->material_set_param(material, param_name, val);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_material_set_param completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_viewport_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_viewport_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID rid = rs->viewport_create();
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_viewport_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_viewport_set_size(const JV& args) {
    auto* it_vp = args.Find("viewport_rid");
    if (!it_vp || !it_vp->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: viewport_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_viewport_set_size called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID vp = rid_from_json(*it_vp);
    auto* w = args.Find("width");
    auto* h = args.Find("height");
    int32_t width = static_cast<int32_t>(w ? w->GetInt() : 640);
    int32_t height = static_cast<int32_t>(h ? h->GetInt() : 480);

    rs->viewport_set_size(vp, width, height);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_viewport_set_size completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_viewport_set_clear_mode(const JV& args) {
    auto* it_vp = args.Find("viewport_rid");
    if (!it_vp || !it_vp->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: viewport_rid"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_viewport_set_clear_mode called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID vp = rid_from_json(*it_vp);
    auto* cm = args.Find("clear_mode");
    auto clear_mode = static_cast<godot::RenderingServer::ViewportClearMode>(
        cm ? static_cast<int>(cm->GetInt()) : 0);

    rs->viewport_set_clear_mode(vp, clear_mode);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_viewport_set_clear_mode completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_particle_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_particle_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID rid = rs->particles_create();
    auto* mode_a = args.Find("mode");
    int mode = mode_a ? static_cast<int>(mode_a->GetInt()) : 1;
    rs->particles_set_mode(rid, static_cast<godot::RenderingServer::ParticlesMode>(mode));

    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_particle_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_environment_set_bg_color(const JV& args) {
    auto* it_env = args.Find("environment_rid");
    auto* it_color = args.Find("color");
    if (!it_env || !it_env->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: environment_rid"); return r;
    }
    if (!it_color || !it_color->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: color"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_environment_set_bg_color called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID env = rid_from_json(*it_env);
    godot::Color c = parse_color(*it_color);

    rs->environment_set_bg_color(env, c);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_environment_set_bg_color completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_environment_set_ambient(const JV& args) {
    auto* it_env = args.Find("environment_rid");
    auto* it_color = args.Find("color");
    if (!it_env || !it_env->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: environment_rid"); return r;
    }
    if (!it_color || !it_color->IsObject()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: color"); return r;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_environment_set_ambient called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID env = rid_from_json(*it_env);
    godot::Color c = parse_color(*it_color);
    auto* src = args.Find("source");
    auto source = static_cast<godot::RenderingServer::EnvironmentAmbientSource>(
        src ? static_cast<int>(src->GetInt()) : 0);
    auto* en = args.Find("energy");
    float energy = static_cast<float>(en && en->IsNumber() ? (en->IsInt() ? static_cast<double>(en->GetInt()) : en->GetDouble()) : 1.0);

    rs->environment_set_ambient_light(env, c, source, energy);
    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_environment_set_ambient completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_fog_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_fog_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID rid = rs->fog_volume_create();

    auto* it_shape = args.Find("shape");
    if (it_shape && it_shape->IsNumber()) {
        rs->fog_volume_set_shape(rid,
            static_cast<godot::RenderingServer::FogVolumeShape>(static_cast<int>(it_shape->GetInt())));
    }

    auto* it_size = args.Find("size");
    if (it_size && it_size->IsObject()) {
        rs->fog_volume_set_size(rid, parse_vector3(*it_size));
    }

    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_fog_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_shader_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_shader_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }

    godot::RID rid = rs->shader_create();

    auto* it_code = args.Find("code");
    if (it_code && it_code->IsString()) {
        rs->shader_set_code(rid, godot::String(it_code->GetString().c_str()));
    }

    LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools, "render_shader_create completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_texture_create_2d(const JV& args) {
    auto* it_path = args.Find("image_path");
    if (!it_path || !it_path->IsString()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: image_path"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_texture_create_2d called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    auto img = godot::Image::load_from_file(godot::String(it_path->GetString().c_str()));
    if (img.is_null()) { JV r(JV::object_tag); r["error"] = JV("failed to load image"); return r; }
    godot::RID rid = rs->texture_2d_create(img);
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_shader_set_code(const JV& args) {
    auto* it_sh = args.Find("shader_rid");
    auto* it_code = args.Find("code");
    if (!it_sh || !it_sh->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: shader_rid"); return r;
    }
    if (!it_code || !it_code->IsString()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: code"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_shader_set_code called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID sh = rid_from_json(*it_sh);
    rs->shader_set_code(sh, godot::String(it_code->GetString().c_str()));
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_shader_get_parameter_list(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_shader_get_parameter_list called");
    JV r(JV::object_tag);
    r["error"] = JV("shader_get_parameter_list not available via godot-cpp; use property_get on the Shader resource");
    return r;
}

JV handle_environment_set_glow(const JV& args) {
    auto* it_env = args.Find("environment_rid");
    if (!it_env || !it_env->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: environment_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_environment_set_glow called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID env = rid_from_json(*it_env);
    bool enabled = true;
    auto* en = args.Find("enabled");
    if (en && en->IsBool()) enabled = en->GetBool();
    godot::PackedFloat32Array levels;
    levels.append(0.85f);
    auto* lv = args.Find("level");
    if (lv && lv->IsDouble()) levels[0] = static_cast<float>(lv->GetDouble());
    float intensity = 0.8f;
    auto* it = args.Find("intensity");
    if (it && it->IsNumber()) intensity = static_cast<float>(it->IsInt() ? static_cast<double>(it->GetInt()) : it->GetDouble());
    float strength = 1.0f;
    auto* st = args.Find("strength");
    if (st && st->IsNumber()) strength = static_cast<float>(st->IsInt() ? static_cast<double>(st->GetInt()) : st->GetDouble());
    float mix = 0.05f;
    auto* mx = args.Find("mix");
    if (mx && mx->IsNumber()) mix = static_cast<float>(mx->IsInt() ? static_cast<double>(mx->GetInt()) : mx->GetDouble());
    float bloom_threshold = 0.0f;
    auto* bt = args.Find("bloom_threshold");
    if (bt && bt->IsNumber()) bloom_threshold = static_cast<float>(bt->IsInt() ? static_cast<double>(bt->GetInt()) : bt->GetDouble());
    int blend_mode = 0;
    auto* bm = args.Find("blend_mode");
    if (bm && bm->IsInt()) blend_mode = static_cast<int>(bm->GetInt());
    float hdr_bleed_threshold = 0.5f;
    auto* hbt = args.Find("hdr_bleed_threshold");
    if (hbt && hbt->IsNumber()) hdr_bleed_threshold = static_cast<float>(hbt->IsInt() ? static_cast<double>(hbt->GetInt()) : hbt->GetDouble());
    float hdr_bleed_scale = 2.0f;
    auto* hbs = args.Find("hdr_bleed_scale");
    if (hbs && hbs->IsNumber()) hdr_bleed_scale = static_cast<float>(hbs->IsInt() ? static_cast<double>(hbs->GetInt()) : hbs->GetDouble());
    float hdr_luminance_cap = 2.0f;
    auto* hlc = args.Find("hdr_luminance_cap");
    if (hlc && hlc->IsNumber()) hdr_luminance_cap = static_cast<float>(hlc->IsInt() ? static_cast<double>(hlc->GetInt()) : hlc->GetDouble());
    float glow_map_strength = 1.0f;
    auto* gms = args.Find("glow_map_strength");
    if (gms && gms->IsNumber()) glow_map_strength = static_cast<float>(gms->IsInt() ? static_cast<double>(gms->GetInt()) : gms->GetDouble());
    rs->environment_set_glow(env, enabled, levels, intensity, strength, mix,
        bloom_threshold, static_cast<godot::RenderingServer::EnvironmentGlowBlendMode>(blend_mode),
        hdr_bleed_threshold, hdr_bleed_scale, hdr_luminance_cap, glow_map_strength, godot::RID());
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_environment_set_ssr(const JV& args) {
    auto* it_env = args.Find("environment_rid");
    if (!it_env || !it_env->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: environment_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_environment_set_ssr called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID env = rid_from_json(*it_env);
    bool enabled = true;
    auto* en = args.Find("enabled");
    if (en && en->IsBool()) enabled = en->GetBool();
    int max_steps = 64;
    auto* ms = args.Find("max_steps");
    if (ms && ms->IsInt()) max_steps = static_cast<int>(ms->GetInt());
    float fade_in = 0.1f;
    auto* fi = args.Find("fade_in");
    if (fi && fi->IsNumber()) fade_in = static_cast<float>(fi->IsInt() ? static_cast<double>(fi->GetInt()) : fi->GetDouble());
    float fade_out = 0.1f;
    auto* fo = args.Find("fade_out");
    if (fo && fo->IsNumber()) fade_out = static_cast<float>(fo->IsInt() ? static_cast<double>(fo->GetInt()) : fo->GetDouble());
    float depth_tolerance = 0.1f;
    auto* dt = args.Find("depth_tolerance");
    if (dt && dt->IsNumber()) depth_tolerance = static_cast<float>(dt->IsInt() ? static_cast<double>(dt->GetInt()) : dt->GetDouble());
    rs->environment_set_ssr(env, enabled, max_steps, fade_in, fade_out, depth_tolerance);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_environment_set_tonemap(const JV& args) {
    auto* it_env = args.Find("environment_rid");
    if (!it_env || !it_env->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: environment_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_environment_set_tonemap called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID env = rid_from_json(*it_env);
    int tone_mapper = 0;
    auto* tm = args.Find("tone_mapper");
    if (tm && tm->IsInt()) tone_mapper = static_cast<int>(tm->GetInt());
    float exposure = 1.0f;
    auto* ex = args.Find("exposure");
    if (ex && ex->IsNumber()) exposure = static_cast<float>(ex->IsInt() ? static_cast<double>(ex->GetInt()) : ex->GetDouble());
    float white = 1.0f;
    auto* wh = args.Find("white");
    if (wh && wh->IsNumber()) white = static_cast<float>(wh->IsInt() ? static_cast<double>(wh->GetInt()) : wh->GetDouble());
    rs->environment_set_tonemap(env, static_cast<godot::RenderingServer::EnvironmentToneMapper>(tone_mapper), exposure, white);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_environment_set_sdfgi(const JV& args) {
    auto* it_env = args.Find("environment_rid");
    if (!it_env || !it_env->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: environment_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_environment_set_sdfgi called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID env = rid_from_json(*it_env);
    bool enabled = true;
    auto* en = args.Find("enabled");
    if (en && en->IsBool()) enabled = en->GetBool();
    int cascades = 4;
    auto* ca = args.Find("cascades");
    if (ca && ca->IsInt()) cascades = static_cast<int>(ca->GetInt());
    float min_cell_size = 0.1f;
    auto* mcs = args.Find("min_cell_size");
    if (mcs && mcs->IsNumber()) min_cell_size = static_cast<float>(mcs->IsInt() ? static_cast<double>(mcs->GetInt()) : mcs->GetDouble());
    int y_scale = 0;
    auto* ys = args.Find("y_scale");
    if (ys && ys->IsInt()) y_scale = static_cast<int>(ys->GetInt());
    bool use_occlusion = true;
    auto* uo = args.Find("use_occlusion");
    if (uo && uo->IsBool()) use_occlusion = uo->GetBool();
    float bounce_feedback = 0.5f;
    auto* bf = args.Find("bounce_feedback");
    if (bf && bf->IsNumber()) bounce_feedback = static_cast<float>(bf->IsInt() ? static_cast<double>(bf->GetInt()) : bf->GetDouble());
    bool read_sky = true;
    auto* rs_ = args.Find("read_sky");
    if (rs_ && rs_->IsBool()) read_sky = rs_->GetBool();
    float energy = 1.0f;
    auto* eg = args.Find("energy");
    if (eg && eg->IsNumber()) energy = static_cast<float>(eg->IsInt() ? static_cast<double>(eg->GetInt()) : eg->GetDouble());
    float normal_bias = 1.0f;
    auto* nb = args.Find("normal_bias");
    if (nb && nb->IsNumber()) normal_bias = static_cast<float>(nb->IsInt() ? static_cast<double>(nb->GetInt()) : nb->GetDouble());
    float probe_bias = 1.0f;
    auto* pb = args.Find("probe_bias");
    if (pb && pb->IsNumber()) probe_bias = static_cast<float>(pb->IsInt() ? static_cast<double>(pb->GetInt()) : pb->GetDouble());
    rs->environment_set_sdfgi(env, enabled, cascades, min_cell_size,
        static_cast<godot::RenderingServer::EnvironmentSDFGIYScale>(y_scale),
        use_occlusion, bounce_feedback, read_sky, energy, normal_bias, probe_bias);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_environment_set_volumetric_fog(const JV& args) {
    auto* it_env = args.Find("environment_rid");
    if (!it_env || !it_env->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: environment_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_environment_set_volumetric_fog called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID env = rid_from_json(*it_env);
    bool enabled = true;
    auto* en = args.Find("enabled");
    if (en && en->IsBool()) enabled = en->GetBool();
    float density = 0.05f;
    auto* de = args.Find("density");
    if (de && de->IsNumber()) density = static_cast<float>(de->IsInt() ? static_cast<double>(de->GetInt()) : de->GetDouble());
    JV albedo_col = JV::FromObject({{"r", JV(1.0)}, {"g", JV(1.0)}, {"b", JV(1.0)}});
    auto* al = args.Find("albedo");
    if (al && al->IsObject()) albedo_col = *al;
    godot::Color albedo = parse_color(albedo_col);
    JV emission_col = JV::FromObject({{"r", JV(0.0)}, {"g", JV(0.0)}, {"b", JV(0.0)}});
    auto* em = args.Find("emission");
    if (em && em->IsObject()) emission_col = *em;
    godot::Color emission = parse_color(emission_col);
    float emission_energy = 1.0f;
    auto* ee = args.Find("emission_energy");
    if (ee && ee->IsNumber()) emission_energy = static_cast<float>(ee->IsInt() ? static_cast<double>(ee->GetInt()) : ee->GetDouble());
    float anisotropy = 0.0f;
    auto* an = args.Find("anisotropy");
    if (an && an->IsNumber()) anisotropy = static_cast<float>(an->IsInt() ? static_cast<double>(an->GetInt()) : an->GetDouble());
    float length = 0.0f;
    auto* le = args.Find("length");
    if (le && le->IsNumber()) length = static_cast<float>(le->IsInt() ? static_cast<double>(le->GetInt()) : le->GetDouble());
    float detail_spread = 0.0f;
    auto* ds = args.Find("detail_spread");
    if (ds && ds->IsNumber()) detail_spread = static_cast<float>(ds->IsInt() ? static_cast<double>(ds->GetInt()) : ds->GetDouble());
    float gi_inject = 0.0f;
    auto* gi = args.Find("gi_inject");
    if (gi && gi->IsNumber()) gi_inject = static_cast<float>(gi->IsInt() ? static_cast<double>(gi->GetInt()) : gi->GetDouble());
    bool temporal_reprojection = false;
    auto* tr = args.Find("temporal_reprojection");
    if (tr && tr->IsBool()) temporal_reprojection = tr->GetBool();
    float temporal_reprojection_amount = 0.5f;
    auto* tra = args.Find("temporal_reprojection_amount");
    if (tra && tra->IsNumber()) temporal_reprojection_amount = static_cast<float>(tra->IsInt() ? static_cast<double>(tra->GetInt()) : tra->GetDouble());
    float ambient_inject = 0.0f;
    auto* ai = args.Find("ambient_inject");
    if (ai && ai->IsNumber()) ambient_inject = static_cast<float>(ai->IsInt() ? static_cast<double>(ai->GetInt()) : ai->GetDouble());
    float sky_affect = 0.0f;
    auto* sa = args.Find("sky_affect");
    if (sa && sa->IsNumber()) sky_affect = static_cast<float>(sa->IsInt() ? static_cast<double>(sa->GetInt()) : sa->GetDouble());
    rs->environment_set_volumetric_fog(env, enabled, density, albedo, emission, emission_energy,
        anisotropy, length, detail_spread, gi_inject, temporal_reprojection,
        temporal_reprojection_amount, ambient_inject, sky_affect);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_sky_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_sky_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID rid = rs->sky_create();
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_sky_set_material(const JV& args) {
    auto* it_sky = args.Find("sky_rid");
    auto* it_mat = args.Find("material_rid");
    if (!it_sky || !it_sky->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: sky_rid"); return r;
    }
    if (!it_mat || !it_mat->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: material_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_sky_set_material called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID sky = rid_from_json(*it_sky);
    godot::RID mat = rid_from_json(*it_mat);
    rs->sky_set_material(sky, mat);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_particles_set_emitting(const JV& args) {
    auto* it_p = args.Find("particles_rid");
    if (!it_p || !it_p->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: particles_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_particles_set_emitting called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID p = rid_from_json(*it_p);
    bool emitting = true;
    auto* em = args.Find("emitting");
    if (em && em->IsBool()) emitting = em->GetBool();
    rs->particles_set_emitting(p, emitting);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_particles_restart(const JV& args) {
    auto* it_p = args.Find("particles_rid");
    if (!it_p || !it_p->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: particles_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_particles_restart called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID p = rid_from_json(*it_p);
    rs->particles_restart(p);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_particles_set_lifetime(const JV& args) {
    auto* it_p = args.Find("particles_rid");
    if (!it_p || !it_p->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: particles_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_particles_set_lifetime called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID p = rid_from_json(*it_p);
    float lifetime = 1.0f;
    auto* lt = args.Find("lifetime");
    if (lt && lt->IsNumber()) lifetime = static_cast<float>(lt->IsInt() ? static_cast<double>(lt->GetInt()) : lt->GetDouble());
    rs->particles_set_lifetime(p, lifetime);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_reflection_probe_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_reflection_probe_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID rid = rs->reflection_probe_create();
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_decal_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_decal_create called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID rid = rs->decal_create();
    JV r(JV::object_tag); r["result"] = rid_to_json(rid); return r;
}

JV handle_fog_volume_set_shape(const JV& args) {
    auto* it_fog = args.Find("fog_rid");
    auto* it_shape = args.Find("shape");
    if (!it_fog || !it_fog->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: fog_rid"); return r;
    }
    if (!it_shape || !it_shape->IsInt()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: shape"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_fog_volume_set_shape called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID fog = rid_from_json(*it_fog);
    int shape = static_cast<int>(it_shape->GetInt());
    rs->fog_volume_set_shape(fog, static_cast<godot::RenderingServer::FogVolumeShape>(shape));
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_instance_set_visible(const JV& args) {
    auto* it_inst = args.Find("instance_rid");
    if (!it_inst || !it_inst->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: instance_rid"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_instance_set_visible called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID inst = rid_from_json(*it_inst);
    bool visible = true;
    auto* vs = args.Find("visible");
    if (vs && vs->IsBool()) visible = vs->GetBool();
    rs->instance_set_visible(inst, visible);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_instance_set_layer_mask(const JV& args) {
    auto* it_inst = args.Find("instance_rid");
    auto* it_mask = args.Find("mask");
    if (!it_inst || !it_inst->IsNumber()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: instance_rid"); return r;
    }
    if (!it_mask || !it_mask->IsInt()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: mask"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_instance_set_layer_mask called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::RID inst = rid_from_json(*it_inst);
    uint32_t mask = static_cast<uint32_t>(it_mask->GetInt());
    rs->instance_set_layer_mask(inst, mask);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_global_shader_parameter_set(const JV& args) {
    auto* it_name = args.Find("name");
    auto* it_val = args.Find("value");
    if (!it_name || !it_name->IsString()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: name"); return r;
    }
    if (!it_val) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: value"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "render_global_shader_parameter_set called");
    auto* rs = godot::RenderingServer::get_singleton();
    if (!rs) { JV r(JV::object_tag); r["error"] = JV("RenderingServer not available"); return r; }
    godot::StringName name(it_name->GetString().c_str());
    auto* th = args.Find("type_hint");
    std::string type_hint = th ? th->GetString() : "";
    godot::Variant val = VariantJson::deserialize(*it_val, type_hint);
    rs->global_shader_parameter_set(name, val);
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_canvas_item_get_rid(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "canvas_item_get_rid called");
    auto* pp = args.Find("path");
    if (!pp || !pp->IsString()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: path"); return r; }
    std::string path = pp->GetString();
    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) { JV r(JV::object_tag); r["error"] = JV("EditorInterface not available"); return r; }
    auto* root = editor->get_edited_scene_root();
    if (!root) { JV r(JV::object_tag); r["error"] = JV("no edited scene root"); return r; }
    std::string clean = path;
    if (!clean.empty() && clean[0] == '/') clean = clean.substr(1);
    if (clean.size() > 5 && clean.compare(0, 5, "root/") == 0) clean = clean.substr(5);
    godot::Node* node = nullptr;
    if (clean.empty() || clean == to_std(root->get_name())) {
        node = root;
    } else {
        node = root->get_node_or_null(godot::NodePath(godot::String(clean.c_str())));
        if (!node) {
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
    }
    if (!node) { JV r(JV::object_tag); r["error"] = JV("node not found: " + path); return r; }
    auto* ci = godot::Object::cast_to<godot::CanvasItem>(node);
    if (!ci) { JV r(JV::object_tag); r["error"] = JV("node is not a CanvasItem: " + path); return r; }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "canvas_item_get_rid completed");
    JV r(JV::object_tag); r["result"] = rid_to_json(ci->get_canvas_item()); return r;
}

JV handle_resolve_rid(const JV& args) {
    auto* idp = args.Find("rid");
    if (!idp || !idp->IsInt()) {
        JV r(JV::object_tag); r["error"] = JV("missing required parameter: rid (integer)"); return r;
    }
    godot::RID rid = rid_from_json(*idp);
    JV r(JV::object_tag);
    r["rid"] = JV(static_cast<int64_t>(rid.get_id()));
    r["is_valid"] = JV(rid.is_valid());
    if (!rid.is_valid()) {
        r["result"] = JV("ok");
        return r;
    }
    auto* editor = godot::EditorInterface::get_singleton();
    if (editor) {
        auto* root = editor->get_edited_scene_root();
        if (root) {
            JV nodes_arr(JV::array_tag);
            godot::TypedArray<godot::Node> all_nodes = root->get_children(true);
            for (int64_t i = 0; i < all_nodes.size(); i++) {
                auto* node = godot::Object::cast_to<godot::Node>(all_nodes[i]);
                if (!node) continue;
                auto* ci = godot::Object::cast_to<godot::CanvasItem>(node);
                if (ci && ci->get_canvas_item() == rid) {
                    JV nj(JV::object_tag);
                    nj["node_path"] = JV(to_std(node->get_path()));
                    nj["type"] = JV("canvas_item");
                    nodes_arr.PushBack(std::move(nj));
                }
            }
            if (nodes_arr.Size() > 0) {
                r["nodes"] = std::move(nodes_arr);
            }
        }
    }
    r["result"] = JV("ok");
    return r;
}

} // namespace render_ops
} // namespace godot_self_driving
