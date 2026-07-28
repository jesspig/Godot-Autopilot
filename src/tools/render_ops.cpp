#include "render_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <mcp/JsonValue.hpp>
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
        static_cast<float>(r ? r->GetDouble() : 0.0),
        static_cast<float>(g ? g->GetDouble() : 0.0),
        static_cast<float>(b ? b->GetDouble() : 0.0),
        static_cast<float>(a ? a->GetDouble() : 1.0));
}

godot::Vector2 parse_vector2(const JV& j) {
    auto* x = j.Find("x"); auto* y = j.Find("y");
    return godot::Vector2(
        x ? x->GetDouble() : 0.0,
        y ? y->GetDouble() : 0.0);
}

godot::Vector3 parse_vector3(const JV& j) {
    auto* x = j.Find("x"); auto* y = j.Find("y"); auto* z = j.Find("z");
    return godot::Vector3(
        x ? x->GetDouble() : 0.0,
        y ? y->GetDouble() : 0.0,
        z ? z->GetDouble() : 0.0);
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
    float radius = static_cast<float>(rad ? rad->GetDouble() : 1.0);
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
    float width = static_cast<float>(w ? w->GetDouble() : -1.0);
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
    xform.columns[0] = godot::Vector2(xa ? xa->GetDouble() : 1.0, 0.0);
    xform.columns[1] = godot::Vector2(0.0, ya ? ya->GetDouble() : 1.0);
    xform.columns[2] = godot::Vector2(ox ? ox->GetDouble() : 0.0, oy ? oy->GetDouble() : 0.0);

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
        ox ? ox->GetDouble() : 0.0,
        oy ? oy->GetDouble() : 0.0,
        oz ? oz->GetDouble() : 0.0);

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
    float fovy = static_cast<float>(fovy_a ? fovy_a->GetDouble() : 75.0);
    float znear = static_cast<float>(znear_a ? znear_a->GetDouble() : 0.01);
    float zfar = static_cast<float>(zfar_a ? zfar_a->GetDouble() : 4000.0);

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
    float size = static_cast<float>(size_a ? size_a->GetDouble() : 10.0);
    float znear = static_cast<float>(znear_a ? znear_a->GetDouble() : 0.01);
    float zfar = static_cast<float>(zfar_a ? zfar_a->GetDouble() : 4000.0);

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
    float energy = static_cast<float>(en ? en->GetDouble() : 1.0);

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

} // namespace render_ops
} // namespace godot_self_driving
