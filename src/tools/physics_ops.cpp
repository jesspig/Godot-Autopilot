#include "physics_ops.hpp"
#include "core/log_system.hpp"
#include "core/resource_registry.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/rid_registry.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape2d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/collision_object2d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/physics_direct_space_state2d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_point_query_parameters2d.hpp>
#include <godot_cpp/classes/physics_point_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters2d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_server2d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters2d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shape2d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world2d.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace physics_ops {

using JV = mcp::JsonValue;

namespace {

struct PhysicsRidDomain {};

godot::Transform2D parse_t2d(const JV &j) {
  godot::Transform2D t;
  if (j.Contains("origin"))
    t.set_origin(util::json_to_vec2(j["origin"]));
  if (j.Contains("rotation"))
    t.set_rotation(
        static_cast<float>(util::json_number(j.Find("rotation"), 0.0)));
  if (j.Contains("scale"))
    t.set_scale(util::json_to_vec2(j["scale"]));
  return t;
}

godot::Basis parse_basis(const JV &j) {
  godot::Basis b;
  if (j.IsArray()) {
    size_t sz = j.Size();
    for (size_t i = 0; i < 3 && i < sz; i++) {
      const JV &r = j[i];
      if (r.IsArray()) {
        size_t rsz = r.Size();
        b.rows[i] = godot::Vector3(
            static_cast<float>(
                rsz > 0
                    ? (r[0].IsNumber()
                           ? (r[0].IsInt() ? static_cast<double>(r[0].GetInt())
                                           : r[0].GetDouble())
                           : 0.0)
                    : 0.0),
            static_cast<float>(
                rsz > 1
                    ? (r[1].IsNumber()
                           ? (r[1].IsInt() ? static_cast<double>(r[1].GetInt())
                                           : r[1].GetDouble())
                           : 0.0)
                    : 0.0),
            static_cast<float>(
                rsz > 2
                    ? (r[2].IsNumber()
                           ? (r[2].IsInt() ? static_cast<double>(r[2].GetInt())
                                           : r[2].GetDouble())
                           : 0.0)
                    : 0.0));
      }
    }
  }
  return b;
}

godot::Transform3D parse_t3d(const JV &j) {
  godot::Basis basis;
  if (j.Contains("basis"))
    basis = parse_basis(j["basis"]);
  godot::Vector3 origin;
  if (j.Contains("origin"))
    origin = util::json_to_vec3(j["origin"]);
  return godot::Transform3D(basis, origin);
}

godot::TypedArray<godot::RID> parse_rids(const JV &j) {
  godot::TypedArray<godot::RID> arr;
  if (j.IsArray()) {
    for (size_t i = 0; i < j.Size(); i++) {
      godot::RID r = util::rid_store<PhysicsRidDomain>().get(j[i].GetInt());
      if (r.is_valid())
        arr.append(r);
    }
  }
  return arr;
}

JV rid_result(const godot::RID &rid) {
  if (!rid.is_valid())
    return JV(nullptr);
  int64_t id = util::rid_store<PhysicsRidDomain>().store(rid);
  JV j(JV::object_tag);
  j["id"] = JV(id);
  j["is_valid"] = JV(true);
  return j;
}

JV hit_2d(const godot::Dictionary &d) {
  JV h(JV::object_tag);
  if (d.has("position")) {
    auto p = godot::Vector2(d["position"]);
    JV v(JV::object_tag);
    v["x"] = JV(p.x);
    v["y"] = JV(p.y);
    h["position"] = std::move(v);
  }
  if (d.has("normal")) {
    auto n = godot::Vector2(d["normal"]);
    JV v(JV::object_tag);
    v["x"] = JV(n.x);
    v["y"] = JV(n.y);
    h["normal"] = std::move(v);
  }
  if (d.has("collider_id"))
    h["collider_id"] = JV(static_cast<int64_t>(d["collider_id"]));
  if (d.has("rid")) {
    godot::RID r = d["rid"];
    h["rid"] = JV(util::rid_store<PhysicsRidDomain>().store(r));
  }
  if (d.has("shape"))
    h["shape"] = JV(static_cast<int>(d["shape"]));
  if (d.has("face_index"))
    h["face_index"] = JV(static_cast<int>(d["face_index"]));
  return h;
}

JV hit_3d(const godot::Dictionary &d) {
  JV h(JV::object_tag);
  if (d.has("position")) {
    auto p = godot::Vector3(d["position"]);
    JV v(JV::object_tag);
    v["x"] = JV(p.x);
    v["y"] = JV(p.y);
    v["z"] = JV(p.z);
    h["position"] = std::move(v);
  }
  if (d.has("normal")) {
    auto n = godot::Vector3(d["normal"]);
    JV v(JV::object_tag);
    v["x"] = JV(n.x);
    v["y"] = JV(n.y);
    v["z"] = JV(n.z);
    h["normal"] = std::move(v);
  }
  if (d.has("collider_id"))
    h["collider_id"] = JV(static_cast<int64_t>(d["collider_id"]));
  if (d.has("rid")) {
    godot::RID r = d["rid"];
    h["rid"] = JV(util::rid_store<PhysicsRidDomain>().store(r));
  }
  if (d.has("shape"))
    h["shape"] = JV(static_cast<int>(d["shape"]));
  if (d.has("face_index"))
    h["face_index"] = JV(static_cast<int>(d["face_index"]));
  return h;
}

} // namespace

JV handle_2d_space_get_direct_state(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_2d_space_direct_state called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  auto *ps = godot::PhysicsServer2D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer2D not available");
    return r;
  }
  auto *state = ps->space_get_direct_state(space);
  if (!state) {
    JV r(JV::object_tag);
    r["error"] = JV("space not active or not in a scene");
    return r;
  }
  JV r(JV::object_tag);
  r["space_rid"] = JV(util::rid_store<PhysicsRidDomain>().store(space));
  r["valid"] = JV(true);
  JV queries(JV::array_tag);
  queries.PushBack(JV("intersect_ray"));
  queries.PushBack(JV("intersect_point"));
  queries.PushBack(JV("intersect_shape"));
  queries.PushBack(JV("cast_motion"));
  queries.PushBack(JV("collide_shape"));
  queries.PushBack(JV("get_rest_info"));
  r["available_queries"] = std::move(queries);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_2d_space_direct_state completed");
  JV ret(JV::object_tag);
  ret["result"] = std::move(r);
  return ret;
}

static godot::RID auto_detect_2d_space() {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return godot::RID();
  auto *root = editor->get_edited_scene_root();
  if (!root)
    return godot::RID();
  auto *viewport = root->get_viewport();
  if (!viewport)
    return godot::RID();
  auto world = viewport->find_world_2d();
  if (!world.is_valid())
    return godot::RID();
  return world->get_space();
}

JV handle_2d_ray_cast(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_2d_ray called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  bool auto_detected = !space.is_valid();
  if (auto_detected) {
    space = auto_detect_2d_space();
  }
  auto *it_from = args.Find("from");
  auto *it_to = args.Find("to");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("could not resolve 2D physics space: provide space_rid or "
                    "ensure a scene with a 2D viewport is open");
    return r;
  }
  if (!it_from || !it_from->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: from");
    return r;
  }
  if (!it_to || !it_to->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: to");
    return r;
  }
  auto *ps = godot::PhysicsServer2D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer2D not available");
    return r;
  }
  auto *state = ps->space_get_direct_state(space);
  if (!state) {
    JV r(JV::object_tag);
    r["error"] = JV("space_get_direct_state returned null");
    r["space_rid"] = JV(static_cast<int64_t>(space.get_id()));
    r["auto_detected"] = JV(auto_detected);
    return r;
  }
  auto *cm = args.Find("collision_mask");
  uint32_t cmv = cm ? static_cast<uint32_t>(cm->GetInt()) : 4294967295;
  auto *excl = args.Find("exclude");
  JV empty_arr(JV::array_tag);
  auto params = godot::PhysicsRayQueryParameters2D::create(
      util::json_to_vec2(*it_from), util::json_to_vec2(*it_to), cmv,
      parse_rids(excl ? *excl : empty_arr));
  if (args.Contains("collide_with_bodies"))
    params->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
  if (args.Contains("collide_with_areas"))
    params->set_collide_with_areas(args["collide_with_areas"].GetBool());
  if (args.Contains("hit_from_inside"))
    params->set_hit_from_inside(args["hit_from_inside"].GetBool());
  godot::Dictionary result = state->intersect_ray(params);
  if (result.is_empty()) {
    JV r(JV::object_tag);
    r["result"] = JV(nullptr);
    r["space_rid"] = JV(static_cast<int64_t>(space.get_id()));
    r["auto_detected"] = JV(auto_detected);
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_2d_ray completed");
  JV r(JV::object_tag);
  r["result"] = hit_2d(result);
  r["space_rid"] = JV(static_cast<int64_t>(space.get_id()));
  r["auto_detected"] = JV(auto_detected);
  return r;
}

JV handle_2d_intersect_shape(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_2d_shape called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  godot::RID shape = util::resolve_rid<PhysicsRidDomain>(args, "shape_rid");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  if (!shape.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: shape_rid");
    return r;
  }
  auto *ps = godot::PhysicsServer2D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer2D not available");
    return r;
  }
  auto *state = ps->space_get_direct_state(space);
  if (!state) {
    JV r(JV::object_tag);
    r["error"] = JV("space_get_direct_state returned null");
    return r;
  }
  godot::Ref<godot::PhysicsShapeQueryParameters2D> p;
  p.instantiate();
  p->set_shape_rid(shape);
  if (args.Contains("transform"))
    p->set_transform(parse_t2d(args["transform"]));
  if (args.Contains("motion"))
    p->set_motion(util::json_to_vec2(args["motion"]));
  if (args.Contains("collision_mask"))
    p->set_collision_mask(
        static_cast<uint32_t>(args["collision_mask"].GetInt()));
  if (args.Contains("exclude"))
    p->set_exclude(parse_rids(args["exclude"]));
  if (args.Contains("collide_with_bodies"))
    p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
  if (args.Contains("collide_with_areas"))
    p->set_collide_with_areas(args["collide_with_areas"].GetBool());
  auto *mr = args.Find("max_results");
  int64_t mrv = mr ? mr->GetInt() : 32;
  auto hits = state->intersect_shape(p, mrv);
  JV arr(JV::array_tag);
  for (int64_t i = 0; i < hits.size(); i++)
    arr.PushBack(hit_2d(hits[i]));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_2d_shape completed");
  JV r(JV::object_tag);
  r["result"] = std::move(arr);
  return r;
}

JV handle_2d_intersect_point(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_2d_point called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  auto *it_pos = args.Find("position");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  if (!it_pos || !it_pos->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: position");
    return r;
  }
  auto *ps = godot::PhysicsServer2D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer2D not available");
    return r;
  }
  auto *state = ps->space_get_direct_state(space);
  if (!state) {
    JV r(JV::object_tag);
    r["error"] = JV("space_get_direct_state returned null");
    return r;
  }
  godot::Ref<godot::PhysicsPointQueryParameters2D> p;
  p.instantiate();
  p->set_position(util::json_to_vec2(*it_pos));
  if (args.Contains("collision_mask"))
    p->set_collision_mask(
        static_cast<uint32_t>(args["collision_mask"].GetInt()));
  if (args.Contains("exclude"))
    p->set_exclude(parse_rids(args["exclude"]));
  if (args.Contains("collide_with_bodies"))
    p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
  if (args.Contains("collide_with_areas"))
    p->set_collide_with_areas(args["collide_with_areas"].GetBool());
  auto *mr = args.Find("max_results");
  int64_t mrv = mr ? mr->GetInt() : 32;
  auto hits = state->intersect_point(p, mrv);
  JV arr(JV::array_tag);
  for (int64_t i = 0; i < hits.size(); i++)
    arr.PushBack(hit_2d(hits[i]));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_2d_point completed");
  JV r(JV::object_tag);
  r["result"] = std::move(arr);
  return r;
}

JV handle_2d_body_create(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_2d_body called");
  auto *ps = godot::PhysicsServer2D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer2D not available");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_2d_body completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(ps->body_create());
  return r;
}

JV handle_2d_body_set_mode(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_2d_body_mode called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_mode = args.Find("mode");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_mode || !it_mode->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: mode");
    return r;
  }
  int m = static_cast<int>(it_mode->IsInt()
                               ? it_mode->GetInt()
                               : static_cast<int64_t>(it_mode->GetDouble()));
  if (m < 0 || m > 3) {
    JV r(JV::object_tag);
    r["error"] =
        JV("invalid mode: 0=static,1=kinematic,2=rigid,3=rigid_linear");
    return r;
  }
  godot::PhysicsServer2D::get_singleton()->body_set_mode(
      body, static_cast<godot::PhysicsServer2D::BodyMode>(m));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_2d_body_mode completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_2d_body_apply_force(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_2d_body_force called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_force = args.Find("force");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_force || !it_force->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: force");
    return r;
  }
  auto *ps = godot::PhysicsServer2D::get_singleton();
  godot::Vector2 f = util::json_to_vec2(*it_force);
  if (args.Contains("position") && args["position"].IsObject())
    ps->body_apply_force(body, f, util::json_to_vec2(args["position"]));
  else
    ps->body_apply_central_force(body, f);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_2d_body_force completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_2d_body_apply_impulse(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_2d_body_impulse called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_imp = args.Find("impulse");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_imp || !it_imp->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: impulse");
    return r;
  }
  auto *ps = godot::PhysicsServer2D::get_singleton();
  godot::Vector2 imp = util::json_to_vec2(*it_imp);
  if (args.Contains("position") && args["position"].IsObject())
    ps->body_apply_impulse(body, imp, util::json_to_vec2(args["position"]));
  else
    ps->body_apply_central_impulse(body, imp);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_2d_body_impulse completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_2d_body_set_state(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_2d_body_state called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_state = args.Find("state");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_state || !it_state->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: state");
    return r;
  }
  int s = static_cast<int>(it_state->IsInt()
                               ? it_state->GetInt()
                               : static_cast<int64_t>(it_state->GetDouble()));
  if (s < 0 || s > 4) {
    JV r(JV::object_tag);
    r["error"] =
        JV("invalid state: "
           "0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep");
    return r;
  }
  auto *it_val = args.Find("value");
  if (!it_val) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: value");
    return r;
  }
  godot::PhysicsServer2D::get_singleton()->body_set_state(
      body, static_cast<godot::PhysicsServer2D::BodyState>(s),
      VariantJson::deserialize(*it_val));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_2d_body_state completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_2d_body_get_state(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_2d_body_state called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_state = args.Find("state");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_state || !it_state->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: state");
    return r;
  }
  int s = static_cast<int>(it_state->IsInt()
                               ? it_state->GetInt()
                               : static_cast<int64_t>(it_state->GetDouble()));
  if (s < 0 || s > 4) {
    JV r(JV::object_tag);
    r["error"] =
        JV("invalid state: "
           "0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep");
    return r;
  }
  auto val = godot::PhysicsServer2D::get_singleton()->body_get_state(
      body, static_cast<godot::PhysicsServer2D::BodyState>(s));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_2d_body_state completed");
  JV r(JV::object_tag);
  r["result"] = VariantJson::serialize(val);
  return r;
}

JV handle_2d_joint_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_2d_joint called");
  auto *it_type = args.Find("type");
  if (!it_type || !it_type->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: type");
    return r;
  }
  auto *ps = godot::PhysicsServer2D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer2D not available");
    return r;
  }
  godot::RID j = ps->joint_create();
  std::string t = it_type->GetString();
  auto rid = [&](const std::string &k) -> godot::RID {
    auto *v = args.Find(k);
    return v && v->IsNumber()
               ? util::rid_store<PhysicsRidDomain>().get(v->GetInt())
               : godot::RID();
  };
  if (t == "pin") {
    godot::Vector2 a =
        args.Contains("anchor") ? util::json_to_vec2(args["anchor"]) : godot::Vector2();
    ps->joint_make_pin(j, a, rid("body_a"), rid("body_b"));
  } else if (t == "groove") {
    if (!args.Contains("groove1_a") || !args.Contains("groove2_a") ||
        !args.Contains("anchor_b")) {
      ps->free_rid(j);
      JV r(JV::object_tag);
      r["error"] = JV("groove requires groove1_a, groove2_a, anchor_b");
      return r;
    }
    ps->joint_make_groove(
        j, util::json_to_vec2(args["groove1_a"]), util::json_to_vec2(args["groove2_a"]),
        util::json_to_vec2(args["anchor_b"]), rid("body_a"), rid("body_b"));
  } else if (t == "damped_spring") {
    if (!args.Contains("anchor_a") || !args.Contains("anchor_b") ||
        !args.Contains("body_a")) {
      ps->free_rid(j);
      JV r(JV::object_tag);
      r["error"] = JV("damped_spring requires anchor_a, anchor_b, body_a");
      return r;
    }
    ps->joint_make_damped_spring(j, util::json_to_vec2(args["anchor_a"]),
                                 util::json_to_vec2(args["anchor_b"]), rid("body_a"),
                                 rid("body_b"));
  } else {
    ps->free_rid(j);
    JV r(JV::object_tag);
    r["error"] =
        JV("unknown type: " + t + " (supported: pin, groove, damped_spring)");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_2d_joint completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(j);
  return r;
}

JV handle_2d_area_create(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_2d_area called");
  auto *ps = godot::PhysicsServer2D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer2D not available");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_2d_area completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(ps->area_create());
  return r;
}

JV handle_2d_area_set_monitorable(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_2d_area_monitorable called");
  godot::RID area = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_mon = args.Find("monitorable");
  if (!area.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_mon || !it_mon->IsBool()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: monitorable");
    return r;
  }
  godot::PhysicsServer2D::get_singleton()->area_set_monitorable(
      area, it_mon->GetBool());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_2d_area_monitorable completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_space_get_direct_state(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_3d_space_direct_state called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  auto *state = ps->space_get_direct_state(space);
  if (!state) {
    JV r(JV::object_tag);
    r["error"] = JV("space not active or not in a scene");
    return r;
  }
  JV r(JV::object_tag);
  r["space_rid"] = JV(util::rid_store<PhysicsRidDomain>().store(space));
  r["valid"] = JV(true);
  JV queries(JV::array_tag);
  queries.PushBack(JV("intersect_ray"));
  queries.PushBack(JV("intersect_point"));
  queries.PushBack(JV("intersect_shape"));
  queries.PushBack(JV("cast_motion"));
  queries.PushBack(JV("collide_shape"));
  queries.PushBack(JV("get_rest_info"));
  r["available_queries"] = std::move(queries);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_3d_space_direct_state completed");
  JV ret(JV::object_tag);
  ret["result"] = std::move(r);
  return ret;
}

JV handle_3d_ray_cast(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_3d_ray called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  auto *it_from = args.Find("from");
  auto *it_to = args.Find("to");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  if (!it_from || !it_from->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: from");
    return r;
  }
  if (!it_to || !it_to->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: to");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  auto *state = ps->space_get_direct_state(space);
  if (!state) {
    JV r(JV::object_tag);
    r["error"] = JV("space_get_direct_state returned null");
    return r;
  }
  auto *cm = args.Find("collision_mask");
  uint32_t cmv = cm ? static_cast<uint32_t>(cm->GetInt()) : 4294967295;
  auto *excl = args.Find("exclude");
  JV empty_arr(JV::array_tag);
  auto params = godot::PhysicsRayQueryParameters3D::create(
      util::json_to_vec3(*it_from), util::json_to_vec3(*it_to), cmv,
      parse_rids(excl ? *excl : empty_arr));
  if (args.Contains("collide_with_bodies"))
    params->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
  if (args.Contains("collide_with_areas"))
    params->set_collide_with_areas(args["collide_with_areas"].GetBool());
  if (args.Contains("hit_from_inside"))
    params->set_hit_from_inside(args["hit_from_inside"].GetBool());
  if (args.Contains("hit_back_faces"))
    params->set_hit_back_faces(args["hit_back_faces"].GetBool());
  godot::Dictionary result = state->intersect_ray(params);
  if (result.is_empty()) {
    JV r(JV::object_tag);
    r["result"] = JV(nullptr);
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_3d_ray completed");
  JV r(JV::object_tag);
  r["result"] = hit_3d(result);
  return r;
}

JV handle_3d_intersect_shape(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_3d_shape called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  godot::RID shape = util::resolve_rid<PhysicsRidDomain>(args, "shape_rid");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  if (!shape.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: shape_rid");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  auto *state = ps->space_get_direct_state(space);
  if (!state) {
    JV r(JV::object_tag);
    r["error"] = JV("space_get_direct_state returned null");
    return r;
  }
  godot::Ref<godot::PhysicsShapeQueryParameters3D> p;
  p.instantiate();
  p->set_shape_rid(shape);
  if (args.Contains("transform"))
    p->set_transform(parse_t3d(args["transform"]));
  if (args.Contains("motion"))
    p->set_motion(util::json_to_vec3(args["motion"]));
  if (args.Contains("collision_mask"))
    p->set_collision_mask(
        static_cast<uint32_t>(args["collision_mask"].GetInt()));
  if (args.Contains("exclude"))
    p->set_exclude(parse_rids(args["exclude"]));
  if (args.Contains("collide_with_bodies"))
    p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
  if (args.Contains("collide_with_areas"))
    p->set_collide_with_areas(args["collide_with_areas"].GetBool());
  auto *mr = args.Find("max_results");
  int64_t mrv = mr ? mr->GetInt() : 32;
  auto hits = state->intersect_shape(p, mrv);
  JV arr(JV::array_tag);
  for (int64_t i = 0; i < hits.size(); i++)
    arr.PushBack(hit_3d(hits[i]));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_3d_shape completed");
  JV r(JV::object_tag);
  r["result"] = std::move(arr);
  return r;
}

JV handle_3d_intersect_point(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_3d_point called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  auto *it_pos = args.Find("position");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  if (!it_pos || !it_pos->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: position");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  auto *state = ps->space_get_direct_state(space);
  if (!state) {
    JV r(JV::object_tag);
    r["error"] = JV("space_get_direct_state returned null");
    return r;
  }
  godot::Ref<godot::PhysicsPointQueryParameters3D> p;
  p.instantiate();
  p->set_position(util::json_to_vec3(*it_pos));
  if (args.Contains("collision_mask"))
    p->set_collision_mask(
        static_cast<uint32_t>(args["collision_mask"].GetInt()));
  if (args.Contains("exclude"))
    p->set_exclude(parse_rids(args["exclude"]));
  if (args.Contains("collide_with_bodies"))
    p->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
  if (args.Contains("collide_with_areas"))
    p->set_collide_with_areas(args["collide_with_areas"].GetBool());
  auto *mr = args.Find("max_results");
  int64_t mrv = mr ? mr->GetInt() : 32;
  auto hits = state->intersect_point(p, mrv);
  JV arr(JV::array_tag);
  for (int64_t i = 0; i < hits.size(); i++)
    arr.PushBack(hit_3d(hits[i]));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "intersect_physics_3d_point completed");
  JV r(JV::object_tag);
  r["result"] = std::move(arr);
  return r;
}

JV handle_3d_body_create(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_body called");
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_body completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(ps->body_create());
  return r;
}

JV handle_3d_body_set_mode(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_mode called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_mode = args.Find("mode");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_mode || !it_mode->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: mode");
    return r;
  }
  int m = static_cast<int>(it_mode->IsInt()
                               ? it_mode->GetInt()
                               : static_cast<int64_t>(it_mode->GetDouble()));
  if (m < 0 || m > 3) {
    JV r(JV::object_tag);
    r["error"] =
        JV("invalid mode: 0=static,1=kinematic,2=rigid,3=rigid_linear");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->body_set_mode(
      body, static_cast<godot::PhysicsServer3D::BodyMode>(m));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_mode completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_apply_force(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_3d_body_force called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_force = args.Find("force");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_force || !it_force->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: force");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  godot::Vector3 f = util::json_to_vec3(*it_force);
  if (args.Contains("position") && args["position"].IsObject())
    ps->body_apply_force(body, f, util::json_to_vec3(args["position"]));
  else
    ps->body_apply_central_force(body, f);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_3d_body_force completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_apply_impulse(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_3d_body_impulse called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_imp = args.Find("impulse");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_imp || !it_imp->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: impulse");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  godot::Vector3 imp = util::json_to_vec3(*it_imp);
  if (args.Contains("position") && args["position"].IsObject())
    ps->body_apply_impulse(body, imp, util::json_to_vec3(args["position"]));
  else
    ps->body_apply_central_impulse(body, imp);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_3d_body_impulse completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_set_state(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_state called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_state = args.Find("state");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_state || !it_state->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: state");
    return r;
  }
  int s = static_cast<int>(it_state->IsInt()
                               ? it_state->GetInt()
                               : static_cast<int64_t>(it_state->GetDouble()));
  if (s < 0 || s > 4) {
    JV r(JV::object_tag);
    r["error"] =
        JV("invalid state: "
           "0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep");
    return r;
  }
  auto *it_val = args.Find("value");
  if (!it_val) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: value");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->body_set_state(
      body, static_cast<godot::PhysicsServer3D::BodyState>(s),
      VariantJson::deserialize(*it_val));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_state completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_get_state(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_3d_body_state called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_state = args.Find("state");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_state || !it_state->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: state");
    return r;
  }
  int s = static_cast<int>(it_state->IsInt()
                               ? it_state->GetInt()
                               : static_cast<int64_t>(it_state->GetDouble()));
  if (s < 0 || s > 4) {
    JV r(JV::object_tag);
    r["error"] =
        JV("invalid state: "
           "0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep");
    return r;
  }
  auto val = godot::PhysicsServer3D::get_singleton()->body_get_state(
      body, static_cast<godot::PhysicsServer3D::BodyState>(s));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_3d_body_state completed");
  JV r(JV::object_tag);
  r["result"] = VariantJson::serialize(val);
  return r;
}

JV handle_3d_joint_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_joint called");
  auto *it_type = args.Find("type");
  if (!it_type || !it_type->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: type");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  godot::RID j = ps->joint_create();
  std::string t = it_type->GetString();
  auto rid = [&](const std::string &k) -> godot::RID {
    auto *v = args.Find(k);
    return v && v->IsNumber()
               ? util::rid_store<PhysicsRidDomain>().get(v->GetInt())
               : godot::RID();
  };
  if (t == "pin") {
    godot::RID ba = rid("body_a_rid");
    godot::RID bb = rid("body_b_rid");
    godot::Vector3 la = args.Contains("local_a") ? util::json_to_vec3(args["local_a"])
                                                 : godot::Vector3();
    godot::Vector3 lb = args.Contains("local_b") ? util::json_to_vec3(args["local_b"])
                                                 : godot::Vector3();
    if (!ba.is_valid()) {
      ps->free_rid(j);
      JV r(JV::object_tag);
      r["error"] = JV("pin requires body_a_rid");
      return r;
    }
    ps->joint_make_pin(j, ba, la, bb.is_valid() ? bb : godot::RID(), lb);
  } else if (t == "hinge") {
    godot::RID ba = rid("body_a_rid");
    godot::RID bb = rid("body_b_rid");
    if (!ba.is_valid()) {
      ps->free_rid(j);
      JV r(JV::object_tag);
      r["error"] = JV("hinge requires body_a_rid");
      return r;
    }
    godot::Transform3D ha = args.Contains("hinge_a")
                                ? parse_t3d(args["hinge_a"])
                                : godot::Transform3D();
    godot::Transform3D hb = args.Contains("hinge_b")
                                ? parse_t3d(args["hinge_b"])
                                : godot::Transform3D();
    ps->joint_make_hinge(j, ba, ha, bb.is_valid() ? bb : godot::RID(), hb);
  } else if (t == "slider") {
    godot::RID ba = rid("body_a_rid");
    godot::RID bb = rid("body_b_rid");
    if (!ba.is_valid()) {
      ps->free_rid(j);
      JV r(JV::object_tag);
      r["error"] = JV("slider requires body_a_rid");
      return r;
    }
    godot::Transform3D ra = args.Contains("ref_a") ? parse_t3d(args["ref_a"])
                                                   : godot::Transform3D();
    godot::Transform3D rb = args.Contains("ref_b") ? parse_t3d(args["ref_b"])
                                                   : godot::Transform3D();
    ps->joint_make_slider(j, ba, ra, bb.is_valid() ? bb : godot::RID(), rb);
  } else if (t == "cone_twist") {
    godot::RID ba = rid("body_a_rid");
    godot::RID bb = rid("body_b_rid");
    if (!ba.is_valid()) {
      ps->free_rid(j);
      JV r(JV::object_tag);
      r["error"] = JV("cone_twist requires body_a_rid");
      return r;
    }
    godot::Transform3D ra = args.Contains("ref_a") ? parse_t3d(args["ref_a"])
                                                   : godot::Transform3D();
    godot::Transform3D rb = args.Contains("ref_b") ? parse_t3d(args["ref_b"])
                                                   : godot::Transform3D();
    ps->joint_make_cone_twist(j, ba, ra, bb.is_valid() ? bb : godot::RID(), rb);
  } else if (t == "generic_6dof") {
    godot::RID ba = rid("body_a_rid");
    godot::RID bb = rid("body_b_rid");
    if (!ba.is_valid()) {
      ps->free_rid(j);
      JV r(JV::object_tag);
      r["error"] = JV("generic_6dof requires body_a_rid");
      return r;
    }
    godot::Transform3D ra = args.Contains("ref_a") ? parse_t3d(args["ref_a"])
                                                   : godot::Transform3D();
    godot::Transform3D rb = args.Contains("ref_b") ? parse_t3d(args["ref_b"])
                                                   : godot::Transform3D();
    ps->joint_make_generic_6dof(j, ba, ra, bb.is_valid() ? bb : godot::RID(),
                                rb);
  } else {
    ps->free_rid(j);
    JV r(JV::object_tag);
    r["error"] =
        JV("unknown type: " + t +
           " (supported: pin, hinge, slider, cone_twist, generic_6dof)");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_joint completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(j);
  return r;
}

JV handle_3d_area_create(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_area called");
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_area completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(ps->area_create());
  return r;
}

JV handle_3d_area_set_monitorable(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_area_monitorable called");
  godot::RID area = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_mon = args.Find("monitorable");
  if (!area.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_mon || !it_mon->IsBool()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: monitorable");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->area_set_monitorable(
      area, it_mon->GetBool());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_area_monitorable completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_apply_torque(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_3d_body_torque called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_t = args.Find("torque");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_t || !it_t->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: torque");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->body_apply_torque(body,
                                                             util::json_to_vec3(*it_t));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "apply_physics_3d_body_torque completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_set_axis_lock(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_axis_lock called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *it_axis = args.Find("axis");
  auto *it_lock = args.Find("lock");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!it_axis || !it_axis->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: axis");
    return r;
  }
  if (!it_lock || !it_lock->IsBool()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: lock");
    return r;
  }
  int axis = static_cast<int>(it_axis->IsInt()
                                  ? it_axis->GetInt()
                                  : static_cast<int64_t>(it_axis->GetDouble()));
  static const int valid[] = {1, 2, 4, 8, 16, 32};
  bool ok = false;
  for (int v : valid) {
    if (axis == v) {
      ok = true;
      break;
    }
  }
  if (!ok) {
    JV r(JV::object_tag);
    r["error"] = JV("invalid axis: "
                    "1=linear_x,2=linear_y,4=linear_z,8=angular_x,16=angular_y,"
                    "32=angular_z");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->body_set_axis_lock(
      body, static_cast<godot::PhysicsServer3D::BodyAxis>(axis),
      it_lock->GetBool());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_axis_lock completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_add_collision_exception(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_physics_3d_body_collision_exception called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  godot::RID ex = util::resolve_rid<PhysicsRidDomain>(args, "excepted_body_rid");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!ex.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: excepted_body_rid");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->body_add_collision_exception(body,
                                                                        ex);
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "add_physics_3d_body_collision_exception completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_remove_collision_exception(const JV &args) {
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "remove_physics_3d_body_collision_exception called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  godot::RID ex = util::resolve_rid<PhysicsRidDomain>(args, "excepted_body_rid");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!ex.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: excepted_body_rid");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->body_remove_collision_exception(body,
                                                                           ex);
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "remove_physics_3d_body_collision_exception completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_joint_set_param(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_joint_param called");
  godot::RID joint = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  if (!joint.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (args.Contains("solver_priority"))
    ps->joint_set_solver_priority(
        joint, static_cast<int32_t>(args["solver_priority"].GetInt()));
  if (args.Contains("disable_collision"))
    ps->joint_disable_collisions_between_bodies(
        joint, args["disable_collision"].GetBool());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_joint_param completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_area_set_space_override(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_area_space called");
  godot::RID area = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  if (!area.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->area_set_space(area, space);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_area_space completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_space_set_gravity(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_space_solver_iterations called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (args.Contains("solver_iterations"))
    ps->space_set_param(space,
                        godot::PhysicsServer3D::SPACE_PARAM_SOLVER_ITERATIONS,
                        static_cast<float>(args["solver_iterations"].GetInt()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_space_solver_iterations completed");
  JV r(JV::object_tag);
  r["result"] = JV("gravity-related space params updated (actual gravity set "
                   "via AreaParameter or ProjectSettings)");
  return r;
}

JV handle_3d_space_set_debug(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_space_solver_params called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (args.Contains("solver_iterations"))
    ps->space_set_param(space,
                        godot::PhysicsServer3D::SPACE_PARAM_SOLVER_ITERATIONS,
                        static_cast<float>(args["solver_iterations"].GetInt()));
  if (args.Contains("contact_max_allowed_penetration"))
    ps->space_set_param(
        space,
        godot::PhysicsServer3D::SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION,
        static_cast<float>(util::json_number(
            args.Find("contact_max_allowed_penetration"), 0.0)));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_space_solver_params completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_soft_body_create(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_soft_body called");
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_soft_body completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(ps->soft_body_create());
  return r;
}

JV handle_3d_soft_body_set_mesh(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_soft_body_mesh called");
  godot::RID soft = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  godot::RID mesh = util::resolve_rid<PhysicsRidDomain>(args, "mesh_rid");
  if (!soft.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!mesh.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: mesh_rid");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->soft_body_set_mesh(soft, mesh);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_soft_body_mesh completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_shape_create(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_sphere_shape called");
  auto *ps = godot::PhysicsServer3D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer3D not available");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_3d_sphere_shape completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(ps->sphere_shape_create());
  return r;
}

JV handle_3d_shape_set_data(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_shape_data called");
  godot::RID shape = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  if (!shape.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  auto *dp = args.Find("data");
  if (!dp) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: data");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->shape_set_data(
      shape, VariantJson::deserialize(*dp));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_shape_data completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_add_shape(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_physics_3d_body_shape called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  godot::RID shape = util::resolve_rid<PhysicsRidDomain>(args, "shape_rid");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!shape.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: shape_rid");
    return r;
  }
  godot::Transform3D t;
  if (args.Contains("transform"))
    t = parse_t3d(args["transform"]);
  bool disabled = false;
  if (args.Contains("disabled"))
    disabled = args["disabled"].GetBool();
  godot::PhysicsServer3D::get_singleton()->body_add_shape(body, shape, t,
                                                          disabled);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_physics_3d_body_shape completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_set_param(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_param called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  auto *pp = args.Find("param");
  auto *vp = args.Find("value");
  if (!pp || !pp->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: param");
    return r;
  }
  if (!vp || !vp->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: value");
    return r;
  }
  int param = static_cast<int>(
      pp->IsInt() ? pp->GetInt() : static_cast<int64_t>(pp->GetDouble()));
  float val = static_cast<float>(
      vp->IsDouble() ? vp->GetDouble() : static_cast<double>(vp->GetInt()));
  godot::PhysicsServer3D::get_singleton()->body_set_param(
      body, static_cast<godot::PhysicsServer3D::BodyParameter>(param), val);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_param completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_area_set_param(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_area_param called");
  godot::RID area = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  if (!area.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  auto *pp = args.Find("param");
  auto *vp = args.Find("value");
  if (!pp || !pp->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: param");
    return r;
  }
  if (!vp) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: value");
    return r;
  }
  int param = static_cast<int>(
      pp->IsInt() ? pp->GetInt() : static_cast<int64_t>(pp->GetDouble()));
  godot::PhysicsServer3D::get_singleton()->area_set_param(
      area, static_cast<godot::PhysicsServer3D::AreaParameter>(param),
      VariantJson::deserialize(*vp));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_area_param completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_space_set_param(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_space_param called");
  godot::RID space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  if (!space.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: space_rid");
    return r;
  }
  auto *pp = args.Find("param");
  auto *vp = args.Find("value");
  if (!pp || !pp->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: param");
    return r;
  }
  if (!vp || !vp->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: value");
    return r;
  }
  int param = static_cast<int>(
      pp->IsInt() ? pp->GetInt() : static_cast<int64_t>(pp->GetDouble()));
  float val = static_cast<float>(
      vp->IsDouble() ? vp->GetDouble() : static_cast<double>(vp->GetInt()));
  godot::PhysicsServer3D::get_singleton()->space_set_param(
      space, static_cast<godot::PhysicsServer3D::SpaceParameter>(param), val);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_space_param completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_area_set_transform(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_area_transform called");
  godot::RID area = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *tp = args.Find("transform");
  if (!area.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!tp || !tp->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: transform");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->area_set_transform(area,
                                                              parse_t3d(*tp));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_area_transform completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_3d_body_set_transform(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_transform called");
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  auto *tp = args.Find("transform");
  if (!body.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  if (!tp || !tp->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: transform");
    return r;
  }
  godot::PhysicsServer3D::get_singleton()->body_set_state(
      body, godot::PhysicsServer3D::BODY_STATE_TRANSFORM, parse_t3d(*tp));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_3d_body_transform completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_2d_shape_create(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_2d_circle_shape called");
  auto *ps = godot::PhysicsServer2D::get_singleton();
  if (!ps) {
    JV r(JV::object_tag);
    r["error"] = JV("PhysicsServer2D not available");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_physics_2d_circle_shape completed");
  JV r(JV::object_tag);
  r["result"] = rid_result(ps->circle_shape_create());
  return r;
}

JV handle_2d_shape_set_data(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_2d_shape_data called");
  godot::RID shape = util::resolve_rid<PhysicsRidDomain>(args, "rid");
  if (!shape.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: rid");
    return r;
  }
  auto *dp = args.Find("data");
  if (!dp) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: data");
    return r;
  }
  godot::PhysicsServer2D::get_singleton()->shape_set_data(
      shape, VariantJson::deserialize(*dp));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_2d_shape_data completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_physics_node_get_rid(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_physics_node_rid called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: path");
    return r;
  }
  std::string path = pp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    JV r(JV::object_tag);
    r["error"] = JV("EditorInterface not available");
    return r;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    JV r(JV::object_tag);
    r["error"] = JV("no edited scene root");
    return r;
  }
  godot::Node *node = util::resolve_scene_node(path, root);
  if (!node) {
    JV r(JV::object_tag);
    r["error"] = JV("node not found: " + path);
    return r;
  }
  auto *co2d = godot::Object::cast_to<godot::CollisionObject2D>(node);
  if (co2d) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_physics_node_rid completed");
    JV r(JV::object_tag);
    r["result"] = rid_result(co2d->get_rid());
    return r;
  }
  auto *co3d = godot::Object::cast_to<godot::CollisionObject3D>(node);
  if (co3d) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_physics_node_rid completed");
    JV r(JV::object_tag);
    r["result"] = rid_result(co3d->get_rid());
    return r;
  }
  JV r(JV::object_tag);
  r["error"] =
      JV("node is not a CollisionObject2D or CollisionObject3D: " + path);
  return r;
}

JV handle_resolve_object(const JV &args) {
  auto *idp = args.Find("object_id");
  if (!idp || !idp->IsInt()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: object_id (integer)");
    return r;
  }
  uint64_t object_id = static_cast<uint64_t>(idp->GetInt());
  godot::Object *obj = godot::ObjectDB::get_instance(object_id);
  if (!obj) {
    JV r(JV::object_tag);
    r["error"] =
        JV("no object found for object_id: " + std::to_string(object_id));
    return r;
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["class"] = JV(util::to_std(obj->get_class()));
  godot::Node *node = godot::Object::cast_to<godot::Node>(obj);
  if (node) {
    if (node->is_inside_tree()) {
      r["node_path"] = JV(util::to_std(node->get_path()));
    }
    r["name"] = JV(util::to_std(node->get_name()));
  }
  auto res = godot::Ref<godot::Resource>(obj);
  if (res.is_valid()) {
    std::string res_path = util::to_std(res->get_path());
    if (!res_path.empty()) {
      r["resource_path"] = JV(res_path);
    }
  }
  return r;
}

JV handle_body_bundle(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_body_bundle called");
  auto *dim_v = args.Find("dimension");
  bool is_2d = dim_v && dim_v->IsString() && dim_v->GetString() == "2d";
  bool is_3d = dim_v && dim_v->IsString() && dim_v->GetString() == "3d";
  if (!is_2d && !is_3d) {
    JV r(JV::object_tag);
    r["error"] =
        JV("missing or invalid required parameter: dimension (expected '2d' or '3d')");
    return r;
  }
  auto *type_v = args.Find("body_type");
  int mode_from_type = -1;
  if (type_v && type_v->IsString()) {
    std::string bt = type_v->GetString();
    if (bt == "static") {
      mode_from_type = 0;
    } else if (bt == "kinematic") {
      mode_from_type = 1;
    } else if (bt == "rigid") {
      mode_from_type = 2;
    } else if (bt == "rigid_linear") {
      mode_from_type = 3;
    }
  }
  if (mode_from_type < 0) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: body_type "
                    "(expected 'static', 'kinematic', 'rigid' or 'rigid_linear')");
    return r;
  }
  int final_mode = mode_from_type;
  if (args.Contains("mode")) {
    auto *mode_v = args.Find("mode");
    if (!mode_v || !mode_v->IsNumber()) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: mode (expected number "
                      "0=static,1=kinematic,2=rigid,3=rigid_linear)");
      return r;
    }
    int m = static_cast<int>(mode_v->IsInt()
                                 ? mode_v->GetInt()
                                 : static_cast<int64_t>(mode_v->GetDouble()));
    if (m < 0 || m > 3) {
      JV r(JV::object_tag);
      r["error"] =
          JV("invalid mode: 0=static,1=kinematic,2=rigid,3=rigid_linear");
      return r;
    }
    final_mode = m;
  }
  auto *shape_v = args.Find("shape");
  if (!shape_v || !shape_v->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: shape");
    return r;
  }
  auto *stype_v = shape_v->Find("type");
  std::string stype =
      (stype_v && stype_v->IsString()) ? stype_v->GetString() : "";
  int shape_kind = -1;
  double radius = 0.0;
  double height = 0.0;
  godot::Vector2 size2;
  godot::Vector3 size3;
  auto num_field = [](const JV &obj, const char *key, double *out) -> bool {
    auto *v = obj.Find(key);
    if (!v || !v->IsNumber()) {
      return false;
    }
    *out = v->IsInt() ? static_cast<double>(v->GetInt()) : v->GetDouble();
    return true;
  };
  if (is_2d) {
    if (stype == "circle") {
      if (!num_field(*shape_v, "radius", &radius) || radius <= 0.0) {
        JV r(JV::object_tag);
        r["error"] =
            JV("shape of type 'circle' requires numeric field 'radius' (> 0)");
        return r;
      }
      shape_kind = 0;
    } else if (stype == "rectangle") {
      auto *sz = shape_v->Find("size");
      if (!sz || !sz->IsObject()) {
        JV r(JV::object_tag);
        r["error"] = JV("shape of type 'rectangle' requires object field "
                        "'size' {x, y}");
        return r;
      }
      size2 = util::json_to_vec2(*sz);
      if (size2.x <= 0.0f || size2.y <= 0.0f) {
        JV r(JV::object_tag);
        r["error"] = JV("shape of type 'rectangle' requires 'size' {x, y} "
                        "with positive values");
        return r;
      }
      shape_kind = 1;
    } else if (stype == "capsule") {
      if (!num_field(*shape_v, "radius", &radius) || radius <= 0.0 ||
          !num_field(*shape_v, "height", &height) || height <= 0.0) {
        JV r(JV::object_tag);
        r["error"] = JV("shape of type 'capsule' requires numeric fields "
                        "'radius' and 'height' (> 0)");
        return r;
      }
      shape_kind = 2;
    } else {
      JV r(JV::object_tag);
      r["error"] = JV("unsupported shape type '" + stype +
                      "' for dimension '2d' (supported: circle, rectangle, "
                      "capsule)");
      return r;
    }
  } else {
    if (stype == "sphere") {
      if (!num_field(*shape_v, "radius", &radius) || radius <= 0.0) {
        JV r(JV::object_tag);
        r["error"] =
            JV("shape of type 'sphere' requires numeric field 'radius' (> 0)");
        return r;
      }
      shape_kind = 0;
    } else if (stype == "box") {
      auto *sz = shape_v->Find("size");
      if (!sz || !sz->IsObject()) {
        JV r(JV::object_tag);
        r["error"] = JV("shape of type 'box' requires object field 'size' "
                        "{x, y, z}");
        return r;
      }
      size3 = util::json_to_vec3(*sz);
      if (size3.x <= 0.0f || size3.y <= 0.0f || size3.z <= 0.0f) {
        JV r(JV::object_tag);
        r["error"] = JV("shape of type 'box' requires 'size' {x, y, z} with "
                        "positive values");
        return r;
      }
      shape_kind = 1;
    } else if (stype == "capsule") {
      if (!num_field(*shape_v, "radius", &radius) || radius <= 0.0 ||
          !num_field(*shape_v, "height", &height) || height <= 0.0) {
        JV r(JV::object_tag);
        r["error"] = JV("shape of type 'capsule' requires numeric fields "
                        "'radius' and 'height' (> 0)");
        return r;
      }
      shape_kind = 2;
    } else {
      JV r(JV::object_tag);
      r["error"] = JV("unsupported shape type '" + stype +
                      "' for dimension '3d' (supported: sphere, box, capsule)");
      return r;
    }
  }
  bool want_rid = args.Contains("body_rid");
  bool want_node = args.Contains("node_path");
  if (want_rid && want_node) {
    JV r(JV::object_tag);
    r["error"] =
        JV("body_rid and node_path are mutually exclusive, pass only one");
    return r;
  }
  std::string node_path;
  if (want_node) {
    auto *np = args.Find("node_path");
    if (!np || !np->IsString() || np->GetString().empty()) {
      JV r(JV::object_tag);
      r["error"] = JV("missing or invalid parameter: node_path");
      return r;
    }
    node_path = np->GetString();
  }
  godot::RID body;
  if (want_rid) {
    auto *br = args.Find("body_rid");
    if (!br || !br->IsNumber()) {
      JV r(JV::object_tag);
      r["error"] = JV("missing or invalid parameter: body_rid");
      return r;
    }
    body = util::rid_store<PhysicsRidDomain>().get(br->GetInt());
    if (!body.is_valid()) {
      JV r(JV::object_tag);
      r["error"] = JV("unknown body_rid, no physics body stored for that handle");
      return r;
    }
  }
  bool has_layer = false;
  uint32_t layer = 0;
  bool has_mask = false;
  uint32_t mask = 0;
  bool has_mass = false;
  double mass = 0.0;
  bool has_friction = false;
  double friction = 0.0;
  bool has_bounce = false;
  double bounce = 0.0;
  auto *layer_v = args.Find("layer");
  if (layer_v) {
    if (!layer_v->IsNumber()) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: layer (expected non-negative integer)");
      return r;
    }
    double d = layer_v->IsInt() ? static_cast<double>(layer_v->GetInt())
                                : layer_v->GetDouble();
    if (d < 0.0) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: layer (expected non-negative integer)");
      return r;
    }
    has_layer = true;
    layer = static_cast<uint32_t>(d);
  }
  auto *mask_v = args.Find("mask");
  if (mask_v) {
    if (!mask_v->IsNumber()) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: mask (expected non-negative integer)");
      return r;
    }
    double d = mask_v->IsInt() ? static_cast<double>(mask_v->GetInt())
                               : mask_v->GetDouble();
    if (d < 0.0) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: mask (expected non-negative integer)");
      return r;
    }
    has_mask = true;
    mask = static_cast<uint32_t>(d);
  }
  auto *mass_v = args.Find("mass");
  if (mass_v) {
    if (!mass_v->IsNumber()) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: mass (expected number > 0)");
      return r;
    }
    mass = mass_v->IsInt() ? static_cast<double>(mass_v->GetInt())
                           : mass_v->GetDouble();
    if (mass <= 0.0) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: mass (expected number > 0)");
      return r;
    }
    has_mass = true;
  }
  auto *friction_v = args.Find("friction");
  if (friction_v) {
    if (!friction_v->IsNumber()) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: friction (expected number >= 0)");
      return r;
    }
    friction = friction_v->IsInt() ? static_cast<double>(friction_v->GetInt())
                                   : friction_v->GetDouble();
    if (friction < 0.0) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: friction (expected number >= 0)");
      return r;
    }
    has_friction = true;
  }
  auto *bounce_v = args.Find("bounce");
  if (bounce_v) {
    if (!bounce_v->IsNumber()) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: bounce (expected number >= 0)");
      return r;
    }
    bounce = bounce_v->IsInt() ? static_cast<double>(bounce_v->GetInt())
                               : bounce_v->GetDouble();
    if (bounce < 0.0) {
      JV r(JV::object_tag);
      r["error"] = JV("invalid parameter: bounce (expected number >= 0)");
      return r;
    }
    has_bounce = true;
  }
  godot::RID shape;
  if (is_2d) {
    auto *ps = godot::PhysicsServer2D::get_singleton();
    if (!ps) {
      JV r(JV::object_tag);
      r["error"] = JV("PhysicsServer2D not available");
      return r;
    }
    if (want_node) {
      auto *editor = godot::EditorInterface::get_singleton();
      if (!editor) {
        JV r(JV::object_tag);
        r["error"] = JV("EditorInterface not available");
        return r;
      }
      auto *root = editor->get_edited_scene_root();
      if (!root) {
        JV r(JV::object_tag);
        r["error"] = JV("no edited scene root");
        return r;
      }
      godot::Node *node = util::resolve_scene_node(node_path, root);
      if (!node) {
        JV r(JV::object_tag);
        r["error"] = JV("node not found: " + node_path);
        return r;
      }
      auto *co = godot::Object::cast_to<godot::CollisionObject2D>(node);
      if (!co) {
        JV r(JV::object_tag);
        r["error"] = JV("node is not a CollisionObject2D: " + node_path);
        return r;
      }
      body = co->get_rid();
    } else if (!want_rid) {
      body = ps->body_create();
    }
    ps->body_set_mode(body,
                      static_cast<godot::PhysicsServer2D::BodyMode>(final_mode));
    if (has_layer) {
      ps->body_set_collision_layer(body, layer);
    }
    if (has_mask) {
      ps->body_set_collision_mask(body, mask);
    }
    if (has_mass) {
      ps->body_set_param(body, godot::PhysicsServer2D::BODY_PARAM_MASS,
                         godot::Variant(mass));
    }
    if (has_friction) {
      ps->body_set_param(body, godot::PhysicsServer2D::BODY_PARAM_FRICTION,
                         godot::Variant(friction));
    }
    if (has_bounce) {
      ps->body_set_param(body, godot::PhysicsServer2D::BODY_PARAM_BOUNCE,
                         godot::Variant(bounce));
    }
    if (shape_kind == 0) {
      shape = ps->circle_shape_create();
      ps->shape_set_data(shape, godot::Variant(radius));
    } else if (shape_kind == 1) {
      shape = ps->rectangle_shape_create();
      ps->shape_set_data(
          shape, godot::Variant(godot::Vector2(size2.x * 0.5f, size2.y * 0.5f)));
    } else {
      shape = ps->capsule_shape_create();
      godot::Array data;
      data.append(godot::Variant(height));
      data.append(godot::Variant(radius));
      ps->shape_set_data(shape, godot::Variant(data));
    }
    ps->body_add_shape(body, shape, godot::Transform2D(), false);
  } else {
    auto *ps = godot::PhysicsServer3D::get_singleton();
    if (!ps) {
      JV r(JV::object_tag);
      r["error"] = JV("PhysicsServer3D not available");
      return r;
    }
    if (want_node) {
      auto *editor = godot::EditorInterface::get_singleton();
      if (!editor) {
        JV r(JV::object_tag);
        r["error"] = JV("EditorInterface not available");
        return r;
      }
      auto *root = editor->get_edited_scene_root();
      if (!root) {
        JV r(JV::object_tag);
        r["error"] = JV("no edited scene root");
        return r;
      }
      godot::Node *node = util::resolve_scene_node(node_path, root);
      if (!node) {
        JV r(JV::object_tag);
        r["error"] = JV("node not found: " + node_path);
        return r;
      }
      auto *co = godot::Object::cast_to<godot::CollisionObject3D>(node);
      if (!co) {
        JV r(JV::object_tag);
        r["error"] = JV("node is not a CollisionObject3D: " + node_path);
        return r;
      }
      body = co->get_rid();
    } else if (!want_rid) {
      body = ps->body_create();
    }
    ps->body_set_mode(body,
                      static_cast<godot::PhysicsServer3D::BodyMode>(final_mode));
    if (has_layer) {
      ps->body_set_collision_layer(body, layer);
    }
    if (has_mask) {
      ps->body_set_collision_mask(body, mask);
    }
    if (has_mass) {
      ps->body_set_param(body, godot::PhysicsServer3D::BODY_PARAM_MASS,
                         godot::Variant(mass));
    }
    if (has_friction) {
      ps->body_set_param(body, godot::PhysicsServer3D::BODY_PARAM_FRICTION,
                         godot::Variant(friction));
    }
    if (has_bounce) {
      ps->body_set_param(body, godot::PhysicsServer3D::BODY_PARAM_BOUNCE,
                         godot::Variant(bounce));
    }
    if (shape_kind == 0) {
      shape = ps->sphere_shape_create();
      ps->shape_set_data(shape, godot::Variant(radius));
    } else if (shape_kind == 1) {
      shape = ps->box_shape_create();
      ps->shape_set_data(shape, godot::Variant(godot::Vector3(
                                    size3.x * 0.5f, size3.y * 0.5f,
                                    size3.z * 0.5f)));
    } else {
      shape = ps->capsule_shape_create();
      godot::Dictionary data;
      data[godot::StringName("radius")] = godot::Variant(radius);
      data[godot::StringName("height")] = godot::Variant(height);
      ps->shape_set_data(shape, godot::Variant(data));
    }
    ps->body_add_shape(body, shape, godot::Transform3D(), false);
  }
  int64_t body_id = util::rid_store<PhysicsRidDomain>().store(body);
  int64_t shape_id = util::rid_store<PhysicsRidDomain>().store(shape);
  JV applied(JV::object_tag);
  applied["mode"] = JV(static_cast<int64_t>(final_mode));
  if (has_layer) {
    applied["layer"] = JV(static_cast<int64_t>(layer));
  }
  if (has_mask) {
    applied["mask"] = JV(static_cast<int64_t>(mask));
  }
  if (has_mass) {
    applied["mass"] = JV(mass);
  }
  if (has_friction) {
    applied["friction"] = JV(friction);
  }
  if (has_bounce) {
    applied["bounce"] = JV(bounce);
  }
  JV inner(JV::object_tag);
  inner["body_rid"] = JV(body_id);
  inner["shape_rid"] = JV(shape_id);
  inner["params_applied"] = std::move(applied);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_physics_body_bundle completed");
  JV r(JV::object_tag);
  r["result"] = std::move(inner);
  return r;
}

JV handle_collision_shape_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_collision_shape called");
  auto *dim_v = args.Find("dim");
  bool is_2d = dim_v && dim_v->IsString() && dim_v->GetString() == "2d";
  bool is_3d = dim_v && dim_v->IsString() && dim_v->GetString() == "3d";
  if (!is_2d && !is_3d) {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: dim (expected '2d' or '3d')");
    return r;
  }
  auto *shape_v = args.Find("shape");
  if (!shape_v || !shape_v->IsString() || shape_v->GetString().empty()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: shape");
    return r;
  }
  std::string stype = shape_v->GetString();
  auto *params_v = args.Find("params");
  if (!params_v || !params_v->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: params");
    return r;
  }
  double radius = 0.0;
  double height = 0.0;
  godot::Vector2 size2;
  godot::Vector3 size3;
  auto num_field = [](const JV &obj, const char *key, double *out) -> bool {
    auto *v = obj.Find(key);
    if (!v || !v->IsNumber()) {
      return false;
    }
    *out = v->IsInt() ? static_cast<double>(v->GetInt()) : v->GetDouble();
    return true;
  };
  auto unknown_field = [](const JV &obj, const char *first, const char *second) -> std::string {
    for (const auto &entry : obj.GetObject()) {
      if (entry.first != first && (second == nullptr || entry.first != second)) {
        return entry.first;
      }
    }
    return std::string();
  };
  if (is_2d) {
    if (stype == "circle") {
      if (!num_field(*params_v, "radius", &radius) || radius <= 0.0) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'circle' requires numeric field 'radius' (> 0)");
        return r;
      }
      std::string unknown = unknown_field(*params_v, "radius", nullptr);
      if (!unknown.empty()) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'circle' rejects unknown field '" + unknown + "' (allowed: radius)");
        return r;
      }
    } else if (stype == "rectangle") {
      auto *sz = params_v->Find("size");
      if (!sz || !sz->IsObject()) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'rectangle' requires object field 'size' {x, y}");
        return r;
      }
      size2 = util::json_to_vec2(*sz);
      if (size2.x <= 0.0f || size2.y <= 0.0f) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'rectangle' requires 'size' {x, y} with positive values");
        return r;
      }
      std::string unknown = unknown_field(*params_v, "size", nullptr);
      if (!unknown.empty()) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'rectangle' rejects unknown field '" + unknown + "' (allowed: size)");
        return r;
      }
    } else if (stype == "capsule") {
      if (!num_field(*params_v, "radius", &radius) || radius <= 0.0 ||
          !num_field(*params_v, "height", &height) || height <= 0.0) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'capsule' requires numeric fields 'radius' and 'height' (> 0)");
        return r;
      }
      std::string unknown = unknown_field(*params_v, "radius", "height");
      if (!unknown.empty()) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'capsule' rejects unknown field '" + unknown + "' (allowed: radius, height)");
        return r;
      }
    } else {
      JV r(JV::object_tag);
      r["error"] = JV("unsupported shape type '" + stype + "' for dim '2d' (supported: circle, rectangle, capsule)");
      return r;
    }
  } else {
    if (stype == "sphere") {
      if (!num_field(*params_v, "radius", &radius) || radius <= 0.0) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'sphere' requires numeric field 'radius' (> 0)");
        return r;
      }
      std::string unknown = unknown_field(*params_v, "radius", nullptr);
      if (!unknown.empty()) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'sphere' rejects unknown field '" + unknown + "' (allowed: radius)");
        return r;
      }
    } else if (stype == "box") {
      auto *sz = params_v->Find("size");
      if (!sz || !sz->IsObject()) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'box' requires object field 'size' {x, y, z}");
        return r;
      }
      size3 = util::json_to_vec3(*sz);
      if (size3.x <= 0.0f || size3.y <= 0.0f || size3.z <= 0.0f) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'box' requires 'size' {x, y, z} with positive values");
        return r;
      }
      std::string unknown = unknown_field(*params_v, "size", nullptr);
      if (!unknown.empty()) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'box' rejects unknown field '" + unknown + "' (allowed: size)");
        return r;
      }
    } else if (stype == "capsule") {
      if (!num_field(*params_v, "radius", &radius) || radius <= 0.0 ||
          !num_field(*params_v, "height", &height) || height <= 0.0) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'capsule' requires numeric fields 'radius' and 'height' (> 0)");
        return r;
      }
      std::string unknown = unknown_field(*params_v, "radius", "height");
      if (!unknown.empty()) {
        JV r(JV::object_tag);
        r["error"] = JV("params for shape 'capsule' rejects unknown field '" + unknown + "' (allowed: radius, height)");
        return r;
      }
    } else {
      JV r(JV::object_tag);
      r["error"] = JV("unsupported shape type '" + stype + "' for dim '3d' (supported: sphere, box, capsule)");
      return r;
    }
  }
  godot::Ref<godot::Resource> res;
  std::string cls;
  if (is_2d) {
    if (stype == "circle") {
      godot::Ref<godot::CircleShape2D> s;
      s.instantiate();
      s->set_radius(static_cast<float>(radius));
      res = s;
      cls = "CircleShape2D";
    } else if (stype == "rectangle") {
      godot::Ref<godot::RectangleShape2D> s;
      s.instantiate();
      s->set_size(size2);
      res = s;
      cls = "RectangleShape2D";
    } else {
      godot::Ref<godot::CapsuleShape2D> s;
      s.instantiate();
      s->set_radius(static_cast<float>(radius));
      s->set_height(static_cast<float>(height));
      res = s;
      cls = "CapsuleShape2D";
    }
  } else {
    if (stype == "sphere") {
      godot::Ref<godot::SphereShape3D> s;
      s.instantiate();
      s->set_radius(static_cast<float>(radius));
      res = s;
      cls = "SphereShape3D";
    } else if (stype == "box") {
      godot::Ref<godot::BoxShape3D> s;
      s.instantiate();
      s->set_size(size3);
      res = s;
      cls = "BoxShape3D";
    } else {
      godot::Ref<godot::CapsuleShape3D> s;
      s.instantiate();
      s->set_radius(static_cast<float>(radius));
      s->set_height(static_cast<float>(height));
      res = s;
      cls = "CapsuleShape3D";
    }
  }
  if (res.is_null()) {
    JV r(JV::object_tag);
    r["error"] = JV("failed to instantiate shape resource");
    return r;
  }
  resource_registry::register_resource(res, std::string());
  int64_t oid = static_cast<int64_t>(res->get_instance_id());
  JV inner2(JV::object_tag);
  inner2["class"] = JV(cls);
  inner2["object_id"] = JV(oid);
  inner2["object_id_str"] = JV(std::to_string(oid));
  inner2["path"] = JV(util::to_std(res->get_path()));
  inner2["dim"] = JV(is_2d ? "2d" : "3d");
  inner2["shape"] = JV(stype);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_collision_shape completed");
  JV rc(JV::object_tag);
  rc["result"] = std::move(inner2);
  return rc;
}

JV handle_collision_shape_assign(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "assign_collision_shape called");
  auto *np = args.Find("node_path");
  if (!np || !np->IsString() || np->GetString().empty()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: node_path");
    return r;
  }
  std::string path = np->GetString();
  auto *ref_v = args.Find("shape_ref");
  if (!ref_v || ref_v->IsNull()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: shape_ref");
    return r;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    JV r(JV::object_tag);
    r["error"] = JV("EditorInterface not available");
    return r;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    JV r(JV::object_tag);
    r["error"] = JV("no edited scene root");
    return r;
  }
  godot::Node *node = util::resolve_scene_node(path, root);
  if (!node) {
    JV r(JV::object_tag);
    r["error"] = JV("node not found: " + path);
    return r;
  }
  auto *cs2d = godot::Object::cast_to<godot::CollisionShape2D>(node);
  auto *cs3d = godot::Object::cast_to<godot::CollisionShape3D>(node);
  if (!cs2d && !cs3d) {
    JV r(JV::object_tag);
    r["error"] = JV("node is not a CollisionShape2D or CollisionShape3D: " + path + " (actual class: " + util::to_std(node->get_class()) + ")");
    return r;
  }
  godot::Ref<godot::Resource> res;
  if (ref_v->IsInt()) {
    int64_t oid = ref_v->GetInt();
    godot::Object *obj = godot::ObjectDB::get_instance(static_cast<uint64_t>(oid));
    if (obj) {
      auto *ro = godot::Object::cast_to<godot::Resource>(obj);
      if (ro) {
        res = godot::Ref<godot::Resource>(ro);
      }
    }
    if (res.is_null()) {
      res = resource_registry::lookup_memory(std::to_string(oid));
    }
    if (res.is_null()) {
      JV r(JV::object_tag);
      r["error"] = JV("shape resource not found for object_id: " + std::to_string(oid) + " (create it with create_collision_shape first)");
      return r;
    }
  } else if (ref_v->IsString() || ref_v->IsObject()) {
    bool oid_handled = false;
    if (ref_v->IsObject()) {
      auto *oid_v = ref_v->Find("object_id");
      auto *oids_v = ref_v->Find("object_id_str");
      auto *name_v = ref_v->Find("name");
      auto *res_v = ref_v->Find("resource");
      auto *path_v = ref_v->Find("path");
      if (oid_v && oid_v->IsInt() && !res_v && !path_v) {
        oid_handled = true;
        int64_t oid = oid_v->GetInt();
        godot::Object *obj = godot::ObjectDB::get_instance(static_cast<uint64_t>(oid));
        if (obj) {
          auto *ro = godot::Object::cast_to<godot::Resource>(obj);
          if (ro) {
            res = godot::Ref<godot::Resource>(ro);
          }
        }
        if (res.is_null()) {
          res = resource_registry::lookup_memory(std::to_string(oid));
        }
        if (res.is_null()) {
          JV r(JV::object_tag);
          r["error"] = JV("shape resource not found for object_id: " + std::to_string(oid) + " (create it with create_collision_shape first)");
          return r;
        }
      } else if (oids_v && oids_v->IsString() && !res_v && !path_v) {
        oid_handled = true;
        int64_t oid = 0;
        try {
          oid = std::stoll(oids_v->GetString());
        } catch (...) {
          JV r(JV::object_tag);
          r["error"] = JV("invalid object_id_str: not a valid 64-bit integer");
          return r;
        }
        godot::Object *obj = godot::ObjectDB::get_instance(static_cast<uint64_t>(oid));
        if (obj) {
          auto *ro = godot::Object::cast_to<godot::Resource>(obj);
          if (ro) {
            res = godot::Ref<godot::Resource>(ro);
          }
        }
        if (res.is_null()) {
          res = resource_registry::lookup_memory(std::to_string(oid));
        }
        if (res.is_null()) {
          JV r(JV::object_tag);
          r["error"] = JV("shape resource not found for object_id: " + std::to_string(oid) + " (create it with create_collision_shape first)");
          return r;
        }
      } else if (name_v && name_v->IsString() && !res_v && !path_v && !oid_v && !oids_v) {
        oid_handled = true;
        std::string nm = name_v->GetString();
        res = resource_registry::lookup_memory(nm);
        if (res.is_null()) {
          JV r(JV::object_tag);
          r["error"] = JV("memory resource not found: " + nm + " (create it with create_collision_shape first)");
          return r;
        }
      }
    }
    if (!oid_handled) {
      godot::Variant var;
      std::string rerr;
      if (!resource_ops::try_resolve_resource_value(*ref_v, var, rerr)) {
        JV r(JV::object_tag);
        r["error"] = JV("invalid shape_ref: expected an object_id integer, a \"res://...\" or \"memory://...\" string, or {\"path\": \"res://...\"} / {\"resource\": \"memory://...\"} / {\"object_id\": ...}");
        return r;
      }
      if (!rerr.empty()) {
        JV r(JV::object_tag);
        r["error"] = JV(rerr);
        return r;
      }
      godot::Object *o = var;
      auto *ro = o ? godot::Object::cast_to<godot::Resource>(o) : nullptr;
      if (!ro) {
        std::string actual = o ? util::to_std(o->get_class()) : "null";
        JV r(JV::object_tag);
        r["error"] = JV("shape_ref did not resolve to a Resource (actual class: " + actual + ")");
        return r;
      }
      res = godot::Ref<godot::Resource>(ro);
    }
  } else {
    JV r(JV::object_tag);
    r["error"] = JV("missing or invalid required parameter: shape_ref");
    return r;
  }
  if (res.is_null()) {
    JV r(JV::object_tag);
    r["error"] = JV("shape_ref did not resolve to a Resource");
    return r;
  }
  std::string rpath = util::to_std(res->get_path());
  const std::string mem_prefix = "memory://";
  if (rpath.compare(0, mem_prefix.size(), mem_prefix) == 0) {
    std::string mname = rpath.substr(mem_prefix.size());
    JV r(JV::object_tag);
    r["error"] = JV("cannot assign memory resource '" + mname + "' to " + path + ".shape — memory:// resources are not persistent; save it first with save_resource to a res:// path and pass {\"path\": \"res://...\"} as shape_ref");
    return r;
  }
  std::string sclass = util::to_std(res->get_class());
  if (cs2d) {
    godot::Ref<godot::Shape2D> s = res;
    if (s.is_null()) {
      JV r(JV::object_tag);
      r["error"] = JV("shape resource is not a Shape2D for CollisionShape2D node '" + path + "' (actual class: " + sclass + ") — pass a Shape2D resource from create_collision_shape with dim '2d'");
      return r;
    }
  } else {
    godot::Ref<godot::Shape3D> s = res;
    if (s.is_null()) {
      JV r(JV::object_tag);
      r["error"] = JV("shape resource is not a Shape3D for CollisionShape3D node '" + path + "' (actual class: " + sclass + ") — pass a Shape3D resource from create_collision_shape with dim '3d'");
      return r;
    }
  }
  godot::StringName prop_name("shape");
  godot::Variant old_val = node->get(prop_name);
  node->set(prop_name, godot::Variant(res.ptr()));
  godot::Object *applied_obj = node->get(prop_name);
  if (applied_obj != static_cast<godot::Object *>(res.ptr())) {
    node->set(prop_name, old_val);
    JV r(JV::object_tag);
    r["error"] = JV("value not applied: 'shape' on " + path + " (property rejected the assigned value); old value restored");
    return r;
  }
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "assign_collision_shape completed");
  JV rout(JV::object_tag);
  rout["result"] = JV("ok");
  rout["assigned"] = JV(true);
  rout["node_path"] = JV(path);
  rout["shape_class"] = JV(sclass);
  rout["object_id"] = JV(static_cast<int64_t>(res->get_instance_id()));
  JV undo_info(JV::object_tag);
  undo_info["path"] = JV(path);
  undo_info["property"] = JV("shape");
  undo_info["old_value"] = VariantJson::serialize(old_val);
  rout["undo"] = std::move(undo_info);
  rout["undoable"] = JV(false);
  rout["undo_skip_reason"] = JV("object-typed value: the editor undo stack cannot restore object references reliably");
  return rout;
}

} // namespace physics_ops
} // namespace godot_autopilot
