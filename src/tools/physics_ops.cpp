#include "physics_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <mcp/JsonValue.hpp>
#include <godot_cpp/classes/physics_server2d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state2d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters2d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_point_query_parameters2d.hpp>
#include <godot_cpp/classes/physics_point_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters2d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <unordered_map>
#include <string>

namespace godot_self_driving {
namespace physics_ops {

using JV = mcp::JsonValue;

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

struct RidStore {
    std::unordered_map<int64_t, godot::RID> map;

    int64_t store(const godot::RID& rid) {
        int64_t id = rid.get_id();
        map[id] = rid;
        return id;
    }

    godot::RID get(int64_t id) {
        auto it = map.find(id);
        if (it != map.end()) return it->second;
        return godot::RID();
    }
};

RidStore& rid_store() {
    static RidStore s;
    return s;
}

godot::RID resolve(const JV& args, const char* key) {
    auto* it = args.Find(key);
    if (!it || !it->IsNumber()) return godot::RID();
    return rid_store().get(it->GetInt());
}

godot::Vector2 parse_vec2(const JV& j) {
    auto* x = j.Find("x");
    auto* y = j.Find("y");
    return godot::Vector2(
        static_cast<float>(x ? x->GetDouble() : 0.0),
        static_cast<float>(y ? y->GetDouble() : 0.0));
}

godot::Vector3 parse_vec3(const JV& j) {
    auto* x = j.Find("x");
    auto* y = j.Find("y");
    auto* z = j.Find("z");
    return godot::Vector3(
        static_cast<float>(x ? x->GetDouble() : 0.0),
        static_cast<float>(y ? y->GetDouble() : 0.0),
        static_cast<float>(z ? z->GetDouble() : 0.0));
}

godot::Transform2D parse_t2d(const JV& j) {
    godot::Transform2D t;
    if (j.Contains("origin")) t.set_origin(parse_vec2(j["origin"]));
    if (j.Contains("rotation")) t.set_rotation(static_cast<float>(j["rotation"].GetDouble()));
    if (j.Contains("scale")) t.set_scale(parse_vec2(j["scale"]));
    return t;
}

godot::Basis parse_basis(const JV& j) {
    godot::Basis b;
    if (j.IsArray()) {
        size_t sz = j.Size();
        for (size_t i = 0; i < 3 && i < sz; i++) {
            const JV& r = j[i];
            if (r.IsArray()) {
                size_t rsz = r.Size();
                b.rows[i] = godot::Vector3(
                    static_cast<float>(rsz > 0 ? r[0].GetDouble() : 0.0),
                    static_cast<float>(rsz > 1 ? r[1].GetDouble() : 0.0),
                    static_cast<float>(rsz > 2 ? r[2].GetDouble() : 0.0));
            }
        }
    }
    return b;
}

godot::Transform3D parse_t3d(const JV& j) {
    godot::Basis basis;
    if (j.Contains("basis")) basis = parse_basis(j["basis"]);
    godot::Vector3 origin;
    if (j.Contains("origin")) origin = parse_vec3(j["origin"]);
    return godot::Transform3D(basis, origin);
}

godot::TypedArray<godot::RID> parse_rids(const JV& j) {
    godot::TypedArray<godot::RID> arr;
    if (j.IsArray()) {
        for (size_t i = 0; i < j.Size(); i++) {
            godot::RID r = rid_store().get(j[i].GetInt());
            if (r.is_valid()) arr.append(r);
        }
    }
    return arr;
}

JV rid_result(const godot::RID& rid) {
    if (!rid.is_valid()) return JV(nullptr);
    int64_t id = rid_store().store(rid);
    JV j(JV::object_tag);
    j["id"] = JV(id);
    j["is_valid"] = JV(true);
    return j;
}

JV hit_2d(const godot::Dictionary& d) {
    JV h(JV::object_tag);
    if (d.has("position")) { auto p = godot::Vector2(d["position"]); JV v(JV::object_tag); v["x"] = JV(p.x); v["y"] = JV(p.y); h["position"] = std::move(v); }
    if (d.has("normal")) { auto n = godot::Vector2(d["normal"]); JV v(JV::object_tag); v["x"] = JV(n.x); v["y"] = JV(n.y); h["normal"] = std::move(v); }
    if (d.has("collider_id")) h["collider_id"] = JV(static_cast<int64_t>(d["collider_id"]));
    if (d.has("rid")) { godot::RID r = d["rid"]; h["rid"] = JV(rid_store().store(r)); }
    if (d.has("shape")) h["shape"] = JV(static_cast<int>(d["shape"]));
    if (d.has("face_index")) h["face_index"] = JV(static_cast<int>(d["face_index"]));
    return h;
}

JV hit_3d(const godot::Dictionary& d) {
    JV h(JV::object_tag);
    if (d.has("position")) { auto p = godot::Vector3(d["position"]); JV v(JV::object_tag); v["x"] = JV(p.x); v["y"] = JV(p.y); v["z"] = JV(p.z); h["position"] = std::move(v); }
    if (d.has("normal")) { auto n = godot::Vector3(d["normal"]); JV v(JV::object_tag); v["x"] = JV(n.x); v["y"] = JV(n.y); v["z"] = JV(n.z); h["normal"] = std::move(v); }
    if (d.has("collider_id")) h["collider_id"] = JV(static_cast<int64_t>(d["collider_id"]));
    if (d.has("rid")) { godot::RID r = d["rid"]; h["rid"] = JV(rid_store().store(r)); }
    if (d.has("shape")) h["shape"] = JV(static_cast<int>(d["shape"]));
    if (d.has("face_index")) h["face_index"] = JV(static_cast<int>(d["face_index"]));
    return h;
}

} // namespace

JV handle_2d_space_get_direct_state(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_space_get_direct_state called");
    godot::RID space = resolve(args, "space_rid");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space not active or not in a scene"); return r; }
    JV r(JV::object_tag);
    r["space_rid"] = JV(rid_store().store(space));
    r["valid"] = JV(true);
    JV queries(JV::array_tag);
    queries.PushBack(JV("intersect_ray"));
    queries.PushBack(JV("intersect_point"));
    queries.PushBack(JV("intersect_shape"));
    queries.PushBack(JV("cast_motion"));
    queries.PushBack(JV("collide_shape"));
    queries.PushBack(JV("get_rest_info"));
    r["available_queries"] = std::move(queries);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_space_get_direct_state completed");
    JV ret(JV::object_tag); ret["result"] = std::move(r); return ret;
}

JV handle_2d_ray_cast(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_ray_cast called");
    godot::RID space = resolve(args, "space_rid");
    auto* it_from = args.Find("from");
    auto* it_to = args.Find("to");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!it_from || !it_from->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: from"); return r; }
    if (!it_to || !it_to->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: to"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    auto* cm = args.Find("collision_mask");
    uint32_t cmv = cm ? static_cast<uint32_t>(cm->GetInt()) : 4294967295;
    auto* excl = args.Find("exclude");
    JV empty_arr(JV::array_tag);
    auto params = godot::PhysicsRayQueryParameters2D::create(
        parse_vec2(*it_from), parse_vec2(*it_to), cmv,
        parse_rids(excl ? *excl : empty_arr));
    if (args.Contains("collide_with_bodies")) params->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) params->set_collide_with_areas(args["collide_with_areas"].GetBool());
    if (args.Contains("hit_from_inside")) params->set_hit_from_inside(args["hit_from_inside"].GetBool());
    godot::Dictionary result = state->intersect_ray(params);
    if (result.is_empty()) { JV r(JV::object_tag); r["result"] = JV(nullptr); return r; }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_ray_cast completed");
    JV r(JV::object_tag); r["result"] = hit_2d(result); return r;
}

JV handle_2d_shape_cast(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_shape_cast called");
    godot::RID space = resolve(args, "space_rid");
    godot::RID shape = resolve(args, "shape_rid");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!shape.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: shape_rid"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    godot::Ref<godot::PhysicsShapeQueryParameters2D> p;
    p.instantiate();
    p->set_shape_rid(shape);
    if (args.Contains("transform")) p->set_transform(parse_t2d(args["transform"]));
    if (args.Contains("motion")) p->set_motion(parse_vec2(args["motion"]));
    if (args.Contains("collision_mask")) p->set_collision_mask(static_cast<uint32_t>(args["collision_mask"].GetInt()));
    if (args.Contains("exclude")) p->set_exclude(parse_rids(args["exclude"]));
    if (args.Contains("collide_with_bodies")) p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) p->set_collide_with_areas(args["collide_with_areas"].GetBool());
    auto* mr = args.Find("max_results");
    int64_t mrv = mr ? mr->GetInt() : 32;
    auto hits = state->intersect_shape(p, mrv);
    JV arr(JV::array_tag);
    for (int64_t i = 0; i < hits.size(); i++) arr.PushBack(hit_2d(hits[i]));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_shape_cast completed");
    JV r(JV::object_tag); r["result"] = std::move(arr); return r;
}

JV handle_2d_point_query(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_point_query called");
    godot::RID space = resolve(args, "space_rid");
    auto* it_pos = args.Find("position");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!it_pos || !it_pos->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: position"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    godot::Ref<godot::PhysicsPointQueryParameters2D> p;
    p.instantiate();
    p->set_position(parse_vec2(*it_pos));
    if (args.Contains("collision_mask")) p->set_collision_mask(static_cast<uint32_t>(args["collision_mask"].GetInt()));
    if (args.Contains("exclude")) p->set_exclude(parse_rids(args["exclude"]));
    if (args.Contains("collide_with_bodies")) p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) p->set_collide_with_areas(args["collide_with_areas"].GetBool());
    auto* mr = args.Find("max_results");
    int64_t mrv = mr ? mr->GetInt() : 32;
    auto hits = state->intersect_point(p, mrv);
    JV arr(JV::array_tag);
    for (int64_t i = 0; i < hits.size(); i++) arr.PushBack(hit_2d(hits[i]));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_point_query completed");
    JV r(JV::object_tag); r["result"] = std::move(arr); return r;
}

JV handle_2d_intersect_shape(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_intersect_shape called");
    godot::RID space = resolve(args, "space_rid");
    godot::RID shape = resolve(args, "shape_rid");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!shape.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: shape_rid"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    godot::Ref<godot::PhysicsShapeQueryParameters2D> p;
    p.instantiate();
    p->set_shape_rid(shape);
    if (args.Contains("transform")) p->set_transform(parse_t2d(args["transform"]));
    if (args.Contains("collision_mask")) p->set_collision_mask(static_cast<uint32_t>(args["collision_mask"].GetInt()));
    if (args.Contains("exclude")) p->set_exclude(parse_rids(args["exclude"]));
    if (args.Contains("collide_with_bodies")) p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) p->set_collide_with_areas(args["collide_with_areas"].GetBool());
    auto* mr = args.Find("max_results");
    int64_t mrv = mr ? mr->GetInt() : 32;
    auto hits = state->intersect_shape(p, mrv);
    JV arr(JV::array_tag);
    for (int64_t i = 0; i < hits.size(); i++) arr.PushBack(hit_2d(hits[i]));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_intersect_shape completed");
    JV r(JV::object_tag); r["result"] = std::move(arr); return r;
}

JV handle_2d_intersect_point(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_intersect_point called");
    godot::RID space = resolve(args, "space_rid");
    auto* it_pos = args.Find("position");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!it_pos || !it_pos->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: position"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    godot::Ref<godot::PhysicsPointQueryParameters2D> p;
    p.instantiate();
    p->set_position(parse_vec2(*it_pos));
    if (args.Contains("collision_mask")) p->set_collision_mask(static_cast<uint32_t>(args["collision_mask"].GetInt()));
    if (args.Contains("exclude")) p->set_exclude(parse_rids(args["exclude"]));
    if (args.Contains("collide_with_bodies")) p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) p->set_collide_with_areas(args["collide_with_areas"].GetBool());
    auto* mr = args.Find("max_results");
    int64_t mrv = mr ? mr->GetInt() : 32;
    auto hits = state->intersect_point(p, mrv);
    JV arr(JV::array_tag);
    for (int64_t i = 0; i < hits.size(); i++) arr.PushBack(hit_2d(hits[i]));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_intersect_point completed");
    JV r(JV::object_tag); r["result"] = std::move(arr); return r;
}

JV handle_2d_body_create(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_create called");
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_create completed");
    JV r(JV::object_tag); r["result"] = rid_result(ps->body_create()); return r;
}

JV handle_2d_body_set_mode(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_set_mode called");
    godot::RID body = resolve(args, "rid");
    auto* it_mode = args.Find("mode");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_mode || !it_mode->IsNumber()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: mode"); return r; }
    int m = static_cast<int>(it_mode->IsInt() ? it_mode->GetInt() : static_cast<int64_t>(it_mode->GetDouble()));
    if (m < 0 || m > 3) { JV r(JV::object_tag); r["error"] = JV("invalid mode: 0=static,1=kinematic,2=rigid,3=rigid_linear"); return r; }
    godot::PhysicsServer2D::get_singleton()->body_set_mode(body, static_cast<godot::PhysicsServer2D::BodyMode>(m));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_set_mode completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_2d_body_apply_force(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_apply_force called");
    godot::RID body = resolve(args, "rid");
    auto* it_force = args.Find("force");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_force || !it_force->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: force"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    godot::Vector2 f = parse_vec2(*it_force);
    if (args.Contains("position") && args["position"].IsObject())
        ps->body_apply_force(body, f, parse_vec2(args["position"]));
    else
        ps->body_apply_central_force(body, f);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_apply_force completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_2d_body_apply_impulse(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_apply_impulse called");
    godot::RID body = resolve(args, "rid");
    auto* it_imp = args.Find("impulse");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_imp || !it_imp->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: impulse"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    godot::Vector2 imp = parse_vec2(*it_imp);
    if (args.Contains("position") && args["position"].IsObject())
        ps->body_apply_impulse(body, imp, parse_vec2(args["position"]));
    else
        ps->body_apply_central_impulse(body, imp);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_apply_impulse completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_2d_body_set_state(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_set_state called");
    godot::RID body = resolve(args, "rid");
    auto* it_state = args.Find("state");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_state || !it_state->IsNumber()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: state"); return r; }
    int s = static_cast<int>(it_state->IsInt() ? it_state->GetInt() : static_cast<int64_t>(it_state->GetDouble()));
    if (s < 0 || s > 4) { JV r(JV::object_tag); r["error"] = JV("invalid state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep"); return r; }
    auto* it_val = args.Find("value");
    if (!it_val) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: value"); return r; }
    godot::PhysicsServer2D::get_singleton()->body_set_state(body, static_cast<godot::PhysicsServer2D::BodyState>(s), VariantJson::deserialize(*it_val));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_set_state completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_2d_body_get_state(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_get_state called");
    godot::RID body = resolve(args, "rid");
    auto* it_state = args.Find("state");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_state || !it_state->IsNumber()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: state"); return r; }
    int s = static_cast<int>(it_state->IsInt() ? it_state->GetInt() : static_cast<int64_t>(it_state->GetDouble()));
    if (s < 0 || s > 4) { JV r(JV::object_tag); r["error"] = JV("invalid state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep"); return r; }
    auto val = godot::PhysicsServer2D::get_singleton()->body_get_state(body, static_cast<godot::PhysicsServer2D::BodyState>(s));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_body_get_state completed");
    JV r(JV::object_tag); r["result"] = VariantJson::serialize(val); return r;
}

JV handle_2d_joint_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_joint_create called");
    auto* it_type = args.Find("type");
    if (!it_type || !it_type->IsString()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: type"); return r; }
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    godot::RID j = ps->joint_create();
    std::string t = it_type->GetString();
    auto rid = [&](const std::string& k) -> godot::RID {
        auto* v = args.Find(k);
        return v && v->IsNumber() ? rid_store().get(v->GetInt()) : godot::RID();
    };
    if (t == "pin") {
        godot::Vector2 a = args.Contains("anchor") ? parse_vec2(args["anchor"]) : godot::Vector2();
        ps->joint_make_pin(j, a, rid("body_a"), rid("body_b"));
    } else if (t == "groove") {
        if (!args.Contains("groove1_a") || !args.Contains("groove2_a") || !args.Contains("anchor_b"))
            { ps->free_rid(j); JV r(JV::object_tag); r["error"] = JV("groove requires groove1_a, groove2_a, anchor_b"); return r; }
        ps->joint_make_groove(j, parse_vec2(args["groove1_a"]), parse_vec2(args["groove2_a"]),
            parse_vec2(args["anchor_b"]), rid("body_a"), rid("body_b"));
    } else if (t == "damped_spring") {
        if (!args.Contains("anchor_a") || !args.Contains("anchor_b") || !args.Contains("body_a"))
            { ps->free_rid(j); JV r(JV::object_tag); r["error"] = JV("damped_spring requires anchor_a, anchor_b, body_a"); return r; }
        ps->joint_make_damped_spring(j, parse_vec2(args["anchor_a"]), parse_vec2(args["anchor_b"]), rid("body_a"), rid("body_b"));
    } else {
        ps->free_rid(j);
        JV r(JV::object_tag); r["error"] = JV("unknown type: " + t + " (supported: pin, groove, damped_spring)"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_joint_create completed");
    JV r(JV::object_tag); r["result"] = rid_result(j); return r;
}

JV handle_2d_area_create(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_area_create called");
    auto* ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer2D not available"); return r; }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_area_create completed");
    JV r(JV::object_tag); r["result"] = rid_result(ps->area_create()); return r;
}

JV handle_2d_area_set_monitorable(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_area_set_monitorable called");
    godot::RID area = resolve(args, "rid");
    auto* it_mon = args.Find("monitorable");
    if (!area.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_mon || !it_mon->IsBool()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: monitorable"); return r; }
    godot::PhysicsServer2D::get_singleton()->area_set_monitorable(area, it_mon->GetBool());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_2d_area_set_monitorable completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_space_get_direct_state(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_space_get_direct_state called");
    godot::RID space = resolve(args, "space_rid");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space not active or not in a scene"); return r; }
    JV r(JV::object_tag);
    r["space_rid"] = JV(rid_store().store(space));
    r["valid"] = JV(true);
    JV queries(JV::array_tag);
    queries.PushBack(JV("intersect_ray"));
    queries.PushBack(JV("intersect_point"));
    queries.PushBack(JV("intersect_shape"));
    queries.PushBack(JV("cast_motion"));
    queries.PushBack(JV("collide_shape"));
    queries.PushBack(JV("get_rest_info"));
    r["available_queries"] = std::move(queries);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_space_get_direct_state completed");
    JV ret(JV::object_tag); ret["result"] = std::move(r); return ret;
}

JV handle_3d_ray_cast(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_ray_cast called");
    godot::RID space = resolve(args, "space_rid");
    auto* it_from = args.Find("from");
    auto* it_to = args.Find("to");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!it_from || !it_from->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: from"); return r; }
    if (!it_to || !it_to->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: to"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    auto* cm = args.Find("collision_mask");
    uint32_t cmv = cm ? static_cast<uint32_t>(cm->GetInt()) : 4294967295;
    auto* excl = args.Find("exclude");
    JV empty_arr(JV::array_tag);
    auto params = godot::PhysicsRayQueryParameters3D::create(
        parse_vec3(*it_from), parse_vec3(*it_to), cmv,
        parse_rids(excl ? *excl : empty_arr));
    if (args.Contains("collide_with_bodies")) params->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) params->set_collide_with_areas(args["collide_with_areas"].GetBool());
    if (args.Contains("hit_from_inside")) params->set_hit_from_inside(args["hit_from_inside"].GetBool());
    if (args.Contains("hit_back_faces")) params->set_hit_back_faces(args["hit_back_faces"].GetBool());
    godot::Dictionary result = state->intersect_ray(params);
    if (result.is_empty()) { JV r(JV::object_tag); r["result"] = JV(nullptr); return r; }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_ray_cast completed");
    JV r(JV::object_tag); r["result"] = hit_3d(result); return r;
}

JV handle_3d_shape_cast(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_shape_cast called");
    godot::RID space = resolve(args, "space_rid");
    godot::RID shape = resolve(args, "shape_rid");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!shape.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: shape_rid"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    godot::Ref<godot::PhysicsShapeQueryParameters3D> p;
    p.instantiate();
    p->set_shape_rid(shape);
    if (args.Contains("transform")) p->set_transform(parse_t3d(args["transform"]));
    if (args.Contains("motion")) p->set_motion(parse_vec3(args["motion"]));
    if (args.Contains("collision_mask")) p->set_collision_mask(static_cast<uint32_t>(args["collision_mask"].GetInt()));
    if (args.Contains("exclude")) p->set_exclude(parse_rids(args["exclude"]));
    if (args.Contains("collide_with_bodies")) p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) p->set_collide_with_areas(args["collide_with_areas"].GetBool());
    auto* mr = args.Find("max_results");
    int64_t mrv = mr ? mr->GetInt() : 32;
    auto hits = state->intersect_shape(p, mrv);
    JV arr(JV::array_tag);
    for (int64_t i = 0; i < hits.size(); i++) arr.PushBack(hit_3d(hits[i]));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_shape_cast completed");
    JV r(JV::object_tag); r["result"] = std::move(arr); return r;
}

JV handle_3d_point_query(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_point_query called");
    godot::RID space = resolve(args, "space_rid");
    auto* it_pos = args.Find("position");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!it_pos || !it_pos->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: position"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    godot::Ref<godot::PhysicsPointQueryParameters3D> p;
    p.instantiate();
    p->set_position(parse_vec3(*it_pos));
    if (args.Contains("collision_mask")) p->set_collision_mask(static_cast<uint32_t>(args["collision_mask"].GetInt()));
    if (args.Contains("exclude")) p->set_exclude(parse_rids(args["exclude"]));
    if (args.Contains("collide_with_bodies")) p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) p->set_collide_with_areas(args["collide_with_areas"].GetBool());
    auto* mr = args.Find("max_results");
    int64_t mrv = mr ? mr->GetInt() : 32;
    auto hits = state->intersect_point(p, mrv);
    JV arr(JV::array_tag);
    for (int64_t i = 0; i < hits.size(); i++) arr.PushBack(hit_3d(hits[i]));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_point_query completed");
    JV r(JV::object_tag); r["result"] = std::move(arr); return r;
}

JV handle_3d_intersect_shape(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_intersect_shape called");
    godot::RID space = resolve(args, "space_rid");
    godot::RID shape = resolve(args, "shape_rid");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!shape.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: shape_rid"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    godot::Ref<godot::PhysicsShapeQueryParameters3D> p;
    p.instantiate();
    p->set_shape_rid(shape);
    if (args.Contains("transform")) p->set_transform(parse_t3d(args["transform"]));
    if (args.Contains("collision_mask")) p->set_collision_mask(static_cast<uint32_t>(args["collision_mask"].GetInt()));
    if (args.Contains("exclude")) p->set_exclude(parse_rids(args["exclude"]));
    if (args.Contains("collide_with_bodies")) p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) p->set_collide_with_areas(args["collide_with_areas"].GetBool());
    auto* mr = args.Find("max_results");
    int64_t mrv = mr ? mr->GetInt() : 32;
    auto hits = state->intersect_shape(p, mrv);
    JV arr(JV::array_tag);
    for (int64_t i = 0; i < hits.size(); i++) arr.PushBack(hit_3d(hits[i]));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_intersect_shape completed");
    JV r(JV::object_tag); r["result"] = std::move(arr); return r;
}

JV handle_3d_intersect_point(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_intersect_point called");
    godot::RID space = resolve(args, "space_rid");
    auto* it_pos = args.Find("position");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    if (!it_pos || !it_pos->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: position"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    auto* state = ps->space_get_direct_state(space);
    if (!state) { JV r(JV::object_tag); r["error"] = JV("space_get_direct_state returned null"); return r; }
    godot::Ref<godot::PhysicsPointQueryParameters3D> p;
    p.instantiate();
    p->set_position(parse_vec3(*it_pos));
    if (args.Contains("collision_mask")) p->set_collision_mask(static_cast<uint32_t>(args["collision_mask"].GetInt()));
    if (args.Contains("exclude")) p->set_exclude(parse_rids(args["exclude"]));
    if (args.Contains("collide_with_bodies")) p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
    if (args.Contains("collide_with_areas")) p->set_collide_with_areas(args["collide_with_areas"].GetBool());
    auto* mr = args.Find("max_results");
    int64_t mrv = mr ? mr->GetInt() : 32;
    auto hits = state->intersect_point(p, mrv);
    JV arr(JV::array_tag);
    for (int64_t i = 0; i < hits.size(); i++) arr.PushBack(hit_3d(hits[i]));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_intersect_point completed");
    JV r(JV::object_tag); r["result"] = std::move(arr); return r;
}

JV handle_3d_body_create(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_create called");
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_create completed");
    JV r(JV::object_tag); r["result"] = rid_result(ps->body_create()); return r;
}

JV handle_3d_body_set_mode(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_set_mode called");
    godot::RID body = resolve(args, "rid");
    auto* it_mode = args.Find("mode");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_mode || !it_mode->IsNumber()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: mode"); return r; }
    int m = static_cast<int>(it_mode->IsInt() ? it_mode->GetInt() : static_cast<int64_t>(it_mode->GetDouble()));
    if (m < 0 || m > 3) { JV r(JV::object_tag); r["error"] = JV("invalid mode: 0=static,1=kinematic,2=rigid,3=rigid_linear"); return r; }
    godot::PhysicsServer3D::get_singleton()->body_set_mode(body, static_cast<godot::PhysicsServer3D::BodyMode>(m));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_set_mode completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_body_apply_force(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_apply_force called");
    godot::RID body = resolve(args, "rid");
    auto* it_force = args.Find("force");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_force || !it_force->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: force"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    godot::Vector3 f = parse_vec3(*it_force);
    if (args.Contains("position") && args["position"].IsObject())
        ps->body_apply_force(body, f, parse_vec3(args["position"]));
    else
        ps->body_apply_central_force(body, f);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_apply_force completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_body_apply_impulse(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_apply_impulse called");
    godot::RID body = resolve(args, "rid");
    auto* it_imp = args.Find("impulse");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_imp || !it_imp->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: impulse"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    godot::Vector3 imp = parse_vec3(*it_imp);
    if (args.Contains("position") && args["position"].IsObject())
        ps->body_apply_impulse(body, imp, parse_vec3(args["position"]));
    else
        ps->body_apply_central_impulse(body, imp);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_apply_impulse completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_body_set_state(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_set_state called");
    godot::RID body = resolve(args, "rid");
    auto* it_state = args.Find("state");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_state || !it_state->IsNumber()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: state"); return r; }
    int s = static_cast<int>(it_state->IsInt() ? it_state->GetInt() : static_cast<int64_t>(it_state->GetDouble()));
    if (s < 0 || s > 4) { JV r(JV::object_tag); r["error"] = JV("invalid state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep"); return r; }
    auto* it_val = args.Find("value");
    if (!it_val) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: value"); return r; }
    godot::PhysicsServer3D::get_singleton()->body_set_state(body, static_cast<godot::PhysicsServer3D::BodyState>(s), VariantJson::deserialize(*it_val));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_set_state completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_body_get_state(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_get_state called");
    godot::RID body = resolve(args, "rid");
    auto* it_state = args.Find("state");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_state || !it_state->IsNumber()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: state"); return r; }
    int s = static_cast<int>(it_state->IsInt() ? it_state->GetInt() : static_cast<int64_t>(it_state->GetDouble()));
    if (s < 0 || s > 4) { JV r(JV::object_tag); r["error"] = JV("invalid state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep"); return r; }
    auto val = godot::PhysicsServer3D::get_singleton()->body_get_state(body, static_cast<godot::PhysicsServer3D::BodyState>(s));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_get_state completed");
    JV r(JV::object_tag); r["result"] = VariantJson::serialize(val); return r;
}

JV handle_3d_joint_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_joint_create called");
    auto* it_type = args.Find("type");
    if (!it_type || !it_type->IsString()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: type"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    godot::RID j = ps->joint_create();
    std::string t = it_type->GetString();
    auto rid = [&](const std::string& k) -> godot::RID {
        auto* v = args.Find(k);
        return v && v->IsNumber() ? rid_store().get(v->GetInt()) : godot::RID();
    };
    if (t == "pin") {
        godot::RID ba = rid("body_a_rid");
        godot::RID bb = rid("body_b_rid");
        godot::Vector3 la = args.Contains("local_a") ? parse_vec3(args["local_a"]) : godot::Vector3();
        godot::Vector3 lb = args.Contains("local_b") ? parse_vec3(args["local_b"]) : godot::Vector3();
        if (!ba.is_valid()) { ps->free_rid(j); JV r(JV::object_tag); r["error"] = JV("pin requires body_a_rid"); return r; }
        ps->joint_make_pin(j, ba, la, bb.is_valid() ? bb : godot::RID(), lb);
    } else if (t == "hinge") {
        godot::RID ba = rid("body_a_rid");
        godot::RID bb = rid("body_b_rid");
        if (!ba.is_valid()) { ps->free_rid(j); JV r(JV::object_tag); r["error"] = JV("hinge requires body_a_rid"); return r; }
        godot::Transform3D ha = args.Contains("hinge_a") ? parse_t3d(args["hinge_a"]) : godot::Transform3D();
        godot::Transform3D hb = args.Contains("hinge_b") ? parse_t3d(args["hinge_b"]) : godot::Transform3D();
        ps->joint_make_hinge(j, ba, ha, bb.is_valid() ? bb : godot::RID(), hb);
    } else if (t == "slider") {
        godot::RID ba = rid("body_a_rid");
        godot::RID bb = rid("body_b_rid");
        if (!ba.is_valid()) { ps->free_rid(j); JV r(JV::object_tag); r["error"] = JV("slider requires body_a_rid"); return r; }
        godot::Transform3D ra = args.Contains("ref_a") ? parse_t3d(args["ref_a"]) : godot::Transform3D();
        godot::Transform3D rb = args.Contains("ref_b") ? parse_t3d(args["ref_b"]) : godot::Transform3D();
        ps->joint_make_slider(j, ba, ra, bb.is_valid() ? bb : godot::RID(), rb);
    } else if (t == "cone_twist") {
        godot::RID ba = rid("body_a_rid");
        godot::RID bb = rid("body_b_rid");
        if (!ba.is_valid()) { ps->free_rid(j); JV r(JV::object_tag); r["error"] = JV("cone_twist requires body_a_rid"); return r; }
        godot::Transform3D ra = args.Contains("ref_a") ? parse_t3d(args["ref_a"]) : godot::Transform3D();
        godot::Transform3D rb = args.Contains("ref_b") ? parse_t3d(args["ref_b"]) : godot::Transform3D();
        ps->joint_make_cone_twist(j, ba, ra, bb.is_valid() ? bb : godot::RID(), rb);
    } else if (t == "generic_6dof") {
        godot::RID ba = rid("body_a_rid");
        godot::RID bb = rid("body_b_rid");
        if (!ba.is_valid()) { ps->free_rid(j); JV r(JV::object_tag); r["error"] = JV("generic_6dof requires body_a_rid"); return r; }
        godot::Transform3D ra = args.Contains("ref_a") ? parse_t3d(args["ref_a"]) : godot::Transform3D();
        godot::Transform3D rb = args.Contains("ref_b") ? parse_t3d(args["ref_b"]) : godot::Transform3D();
        ps->joint_make_generic_6dof(j, ba, ra, bb.is_valid() ? bb : godot::RID(), rb);
    } else {
        ps->free_rid(j);
        JV r(JV::object_tag); r["error"] = JV("unknown type: " + t + " (supported: pin, hinge, slider, cone_twist, generic_6dof)"); return r;
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_joint_create completed");
    JV r(JV::object_tag); r["result"] = rid_result(j); return r;
}

JV handle_3d_area_create(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_area_create called");
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_area_create completed");
    JV r(JV::object_tag); r["result"] = rid_result(ps->area_create()); return r;
}

JV handle_3d_area_set_monitorable(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_area_set_monitorable called");
    godot::RID area = resolve(args, "rid");
    auto* it_mon = args.Find("monitorable");
    if (!area.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_mon || !it_mon->IsBool()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: monitorable"); return r; }
    godot::PhysicsServer3D::get_singleton()->area_set_monitorable(area, it_mon->GetBool());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_area_set_monitorable completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_body_apply_torque(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_apply_torque called");
    godot::RID body = resolve(args, "rid");
    auto* it_t = args.Find("torque");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_t || !it_t->IsObject()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: torque"); return r; }
    godot::PhysicsServer3D::get_singleton()->body_apply_torque(body, parse_vec3(*it_t));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_apply_torque completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_body_set_axis_lock(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_set_axis_lock called");
    godot::RID body = resolve(args, "rid");
    auto* it_axis = args.Find("axis");
    auto* it_lock = args.Find("lock");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!it_axis || !it_axis->IsNumber()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: axis"); return r; }
    if (!it_lock || !it_lock->IsBool()) { JV r(JV::object_tag); r["error"] = JV("missing required parameter: lock"); return r; }
    int axis = static_cast<int>(it_axis->IsInt() ? it_axis->GetInt() : static_cast<int64_t>(it_axis->GetDouble()));
    static const int valid[] = {1, 2, 4, 8, 16, 32};
    bool ok = false;
    for (int v : valid) { if (axis == v) { ok = true; break; } }
    if (!ok) { JV r(JV::object_tag); r["error"] = JV("invalid axis: 1=linear_x,2=linear_y,4=linear_z,8=angular_x,16=angular_y,32=angular_z"); return r; }
    godot::PhysicsServer3D::get_singleton()->body_set_axis_lock(body, static_cast<godot::PhysicsServer3D::BodyAxis>(axis), it_lock->GetBool());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_set_axis_lock completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_body_add_collision_exception(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_add_collision_exception called");
    godot::RID body = resolve(args, "rid");
    godot::RID ex = resolve(args, "excepted_body_rid");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!ex.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: excepted_body_rid"); return r; }
    godot::PhysicsServer3D::get_singleton()->body_add_collision_exception(body, ex);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_add_collision_exception completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_body_remove_collision_exception(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_remove_collision_exception called");
    godot::RID body = resolve(args, "rid");
    godot::RID ex = resolve(args, "excepted_body_rid");
    if (!body.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!ex.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: excepted_body_rid"); return r; }
    godot::PhysicsServer3D::get_singleton()->body_remove_collision_exception(body, ex);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_body_remove_collision_exception completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_joint_set_param(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_joint_set_param called");
    godot::RID joint = resolve(args, "rid");
    if (!joint.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (args.Contains("solver_priority"))
        ps->joint_set_solver_priority(joint, static_cast<int32_t>(args["solver_priority"].GetInt()));
    if (args.Contains("disable_collision"))
        ps->joint_disable_collisions_between_bodies(joint, args["disable_collision"].GetBool());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_joint_set_param completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_area_set_space_override(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_area_set_space_override called");
    godot::RID area = resolve(args, "rid");
    godot::RID space = resolve(args, "space_rid");
    if (!area.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: space_rid"); return r; }
    godot::PhysicsServer3D::get_singleton()->area_set_space(area, space);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_area_set_space_override completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_space_set_gravity(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_space_set_gravity called");
    godot::RID space = resolve(args, "rid");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (args.Contains("solver_iterations"))
        ps->space_set_param(space, godot::PhysicsServer3D::SPACE_PARAM_SOLVER_ITERATIONS, static_cast<float>(args["solver_iterations"].GetInt()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_space_set_gravity completed");
    JV r(JV::object_tag); r["result"] = JV("gravity-related space params updated (actual gravity set via AreaParameter or ProjectSettings)"); return r;
}

JV handle_3d_space_set_debug(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_space_set_debug called");
    godot::RID space = resolve(args, "rid");
    if (!space.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (args.Contains("solver_iterations"))
        ps->space_set_param(space, godot::PhysicsServer3D::SPACE_PARAM_SOLVER_ITERATIONS, static_cast<float>(args["solver_iterations"].GetInt()));
    if (args.Contains("contact_max_allowed_penetration"))
        ps->space_set_param(space, godot::PhysicsServer3D::SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION, static_cast<float>(args["contact_max_allowed_penetration"].GetDouble()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_space_set_debug completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

JV handle_3d_soft_body_create(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_soft_body_create called");
    auto* ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) { JV r(JV::object_tag); r["error"] = JV("PhysicsServer3D not available"); return r; }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_soft_body_create completed");
    JV r(JV::object_tag); r["result"] = rid_result(ps->soft_body_create()); return r;
}

JV handle_3d_soft_body_set_mesh(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_soft_body_set_mesh called");
    godot::RID soft = resolve(args, "rid");
    godot::RID mesh = resolve(args, "mesh_rid");
    if (!soft.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: rid"); return r; }
    if (!mesh.is_valid()) { JV r(JV::object_tag); r["error"] = JV("missing or invalid required parameter: mesh_rid"); return r; }
    godot::PhysicsServer3D::get_singleton()->soft_body_set_mesh(soft, mesh);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "physics_3d_soft_body_set_mesh completed");
    JV r(JV::object_tag); r["result"] = JV("ok"); return r;
}

} // namespace physics_ops
} // namespace godot_self_driving
