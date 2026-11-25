#include "tools/domains/physics/physics_ops.hpp"
#include "core/observability/log_system.hpp"
#include "core/resource_registry.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "tools/domains/resource/resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/rid_registry.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/area3d.hpp>
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
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/physics_direct_body_state2d.hpp>
#include <godot_cpp/classes/physics_direct_body_state3d.hpp>
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
#include <godot_cpp/classes/physics_test_motion_parameters2d.hpp>
#include <godot_cpp/classes/physics_test_motion_parameters3d.hpp>
#include <godot_cpp/classes/physics_test_motion_result2d.hpp>
#include <godot_cpp/classes/physics_test_motion_result3d.hpp>
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
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "util/json_value.hpp"
#include <string>

namespace godot_autopilot {
namespace physics_ops {

using JV = godot_autopilot::util::JsonValue;

namespace {

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

namespace {

const char *const kPhysicsQueryModeText =
    "intersect_point, intersect_ray, intersect_shape, cast_motion, "
    "collide_shape, rest_info, test_motion, contacts, area_overlaps";

bool require_query_key(const JV &args, const char *mode, const char *key,
                       JV *error) {
  if (args.Find(key) != nullptr)
    return true;
  *error = util::error_json("missing required parameter for mode '" +
                            std::string(mode) + "': " + key);
  return false;
}

bool parse_query_dimension(const JV &args, int *dimension, JV *error) {
  auto *value = args.Find("dimension");
  if (!value || !value->IsNumber()) {
    *error = util::error_json(
        "missing or invalid required parameter: dimension (expected 2 or 3)");
    return false;
  }
  int64_t number = 0;
  if (value->IsInt()) {
    number = value->GetInt();
  } else {
    const double raw = value->GetDouble();
    if (raw != 2.0 && raw != 3.0) {
      *error = util::error_json(
          "invalid dimension: expected 2 (PhysicsServer2D) or 3 "
          "(PhysicsServer3D)");
      return false;
    }
    number = static_cast<int64_t>(raw);
  }
  if (number != 2 && number != 3) {
    *error = util::error_json("invalid dimension: expected 2 (PhysicsServer2D) "
                              "or 3 (PhysicsServer3D)");
    return false;
  }
  *dimension = static_cast<int>(number);
  return true;
}

bool parse_query_mode(const JV &args, std::string *mode, JV *error) {
  auto *value = args.Find("mode");
  if (!value || !value->IsString()) {
    *error = util::error_json(
        "missing or invalid required parameter: mode (supported: " +
        std::string(kPhysicsQueryModeText) + ")");
    return false;
  }
  static const char *const kModes[] = {
      "intersect_point", "intersect_ray", "intersect_shape", "cast_motion",
      "collide_shape",   "rest_info",     "test_motion",     "contacts",
      "area_overlaps"};
  const std::string candidate = value->GetString();
  for (const char *known : kModes) {
    if (candidate == known) {
      *mode = candidate;
      return true;
    }
  }
  *error = util::error_json("invalid mode: " + candidate + " (supported: " +
                            kPhysicsQueryModeText + ")");
  return false;
}

bool parse_query_vec2(const JV &args, const char *key, godot::Vector2 *out,
                      JV *error) {
  auto *value = args.Find(key);
  auto *x = value && value->IsObject() ? value->Find("x") : nullptr;
  auto *y = value && value->IsObject() ? value->Find("y") : nullptr;
  if (!x || !y || !x->IsNumber() || !y->IsNumber()) {
    *error = util::error_json(
        "invalid parameter: " + std::string(key) +
        " expects a Vector2 object {x, y} for dimension 2");
    return false;
  }
  *out = godot::Vector2(static_cast<float>(util::json_number(x, 0.0)),
                        static_cast<float>(util::json_number(y, 0.0)));
  return true;
}

bool parse_query_vec3(const JV &args, const char *key, godot::Vector3 *out,
                      JV *error) {
  auto *value = args.Find(key);
  auto *x = value && value->IsObject() ? value->Find("x") : nullptr;
  auto *y = value && value->IsObject() ? value->Find("y") : nullptr;
  auto *z = value && value->IsObject() ? value->Find("z") : nullptr;
  if (!x || !y || !z || !x->IsNumber() || !y->IsNumber() || !z->IsNumber()) {
    *error = util::error_json(
        "invalid parameter: " + std::string(key) +
        " expects a Vector3 object {x, y, z} for dimension 3");
    return false;
  }
  *out = godot::Vector3(static_cast<float>(util::json_number(x, 0.0)),
                        static_cast<float>(util::json_number(y, 0.0)),
                        static_cast<float>(util::json_number(z, 0.0)));
  return true;
}

bool parse_query_transform2d(const JV &args, const char *key,
                             godot::Transform2D *out, JV *error) {
  auto *value = args.Find(key);
  if (!value || !value->IsObject()) {
    *error = util::error_json(
        "invalid parameter: " + std::string(key) +
        " expects a Transform2D object with origin {x, y}, rotation and scale "
        "{x, y} for dimension 2");
    return false;
  }
  const bool has_transform_fields =
      value->Contains("origin") || value->Contains("rotation") ||
      value->Contains("scale");
  if (!has_transform_fields) {
    if (!value->Contains("x") && !value->Contains("y")) {
      *out = godot::Transform2D();
      return true;
    }
    auto *x = value->Find("x");
    auto *y = value->Find("y");
    if (!x || !y || !x->IsNumber() || !y->IsNumber()) {
      *error = util::error_json(
          "invalid parameter: " + std::string(key) +
          " expects a Vector2 object {x, y} or a Transform2D object for "
          "dimension 2");
      return false;
    }
    godot::Transform2D transform;
    transform.set_origin(
        godot::Vector2(static_cast<float>(util::json_number(x, 0.0)),
                       static_cast<float>(util::json_number(y, 0.0))));
    *out = transform;
    return true;
  }
  auto *origin = value->Find("origin");
  if (origin &&
      (!origin->IsObject() || !origin->Find("x") || !origin->Find("y") ||
       !origin->Find("x")->IsNumber() || !origin->Find("y")->IsNumber())) {
    *error = util::error_json(
        "invalid parameter: " + std::string(key) +
        ".origin expects a Vector2 object {x, y} for dimension 2");
    return false;
  }
  auto *rotation = value->Find("rotation");
  if (rotation && !rotation->IsNumber()) {
    *error = util::error_json("invalid parameter: " + std::string(key) +
                              ".rotation expects a number for dimension 2");
    return false;
  }
  auto *scale = value->Find("scale");
  if (scale &&
      (!scale->IsObject() || !scale->Find("x") || !scale->Find("y") ||
       !scale->Find("x")->IsNumber() || !scale->Find("y")->IsNumber())) {
    *error = util::error_json(
        "invalid parameter: " + std::string(key) +
        ".scale expects a Vector2 object {x, y} for dimension 2");
    return false;
  }
  *out = parse_t2d(*value);
  return true;
}

bool parse_query_transform3d(const JV &args, const char *key,
                             godot::Transform3D *out, JV *error) {
  auto *value = args.Find(key);
  if (!value || !value->IsObject()) {
    *error = util::error_json(
        "invalid parameter: " + std::string(key) +
        " expects a Transform3D object with basis rows and origin {x, y, z} "
        "for dimension 3");
    return false;
  }
  const bool has_transform_fields =
      value->Contains("origin") || value->Contains("basis");
  if (!has_transform_fields) {
    if (!value->Contains("x") && !value->Contains("y") &&
        !value->Contains("z")) {
      *out = godot::Transform3D();
      return true;
    }
    auto *x = value->Find("x");
    auto *y = value->Find("y");
    auto *z = value->Find("z");
    if (!x || !y || !z || !x->IsNumber() || !y->IsNumber() || !z->IsNumber()) {
      *error = util::error_json(
          "invalid parameter: " + std::string(key) +
          " expects a Vector3 object {x, y, z} or a Transform3D object for "
          "dimension 3");
      return false;
    }
    godot::Transform3D transform;
    transform.set_origin(godot::Vector3(
        static_cast<float>(util::json_number(x, 0.0)),
        static_cast<float>(util::json_number(y, 0.0)),
        static_cast<float>(util::json_number(z, 0.0))));
    *out = transform;
    return true;
  }
  auto *origin = value->Find("origin");
  if (origin &&
      (!origin->IsObject() || !origin->Find("x") || !origin->Find("y") ||
       !origin->Find("z") || !origin->Find("x")->IsNumber() ||
       !origin->Find("y")->IsNumber() || !origin->Find("z")->IsNumber())) {
    *error = util::error_json(
        "invalid parameter: " + std::string(key) +
        ".origin expects a Vector3 object {x, y, z} for dimension 3");
    return false;
  }
  auto *basis = value->Find("basis");
  if (basis) {
    if (!basis->IsArray() || basis->Size() != 3) {
      *error = util::error_json(
          "invalid parameter: " + std::string(key) +
          ".basis expects an array of 3 row arrays for dimension 3");
      return false;
    }
    for (size_t i = 0; i < 3; i++) {
      const JV &row = (*basis)[i];
      if (!row.IsArray() || row.Size() != 3 || !row[0].IsNumber() ||
          !row[1].IsNumber() || !row[2].IsNumber()) {
        *error = util::error_json(
            "invalid parameter: " + std::string(key) +
            ".basis expects an array of 3 row arrays for dimension 3");
        return false;
      }
    }
  }
  *out = parse_t3d(*value);
  return true;
}

bool resolve_query_space_2d(const JV &args,
                            godot::PhysicsDirectSpaceState2D **state,
                            godot::RID *space, JV *error) {
  *space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  if (!space->is_valid()) {
    *error = util::error_json(
        "missing or invalid required parameter: space_rid (required for "
        "dimension 2)");
    return false;
  }
  auto *server = godot::PhysicsServer2D::get_singleton();
  if (!server) {
    *error = util::error_json("PhysicsServer2D not available");
    return false;
  }
  *state = server->space_get_direct_state(*space);
  if (!*state) {
    *error = util::error_json("space_get_direct_state returned null: the space "
                              "must be active and attached to a scene");
    return false;
  }
  return true;
}

bool resolve_query_space_3d(const JV &args,
                            godot::PhysicsDirectSpaceState3D **state,
                            godot::RID *space, JV *error) {
  *space = util::resolve_rid<PhysicsRidDomain>(args, "space_rid");
  if (!space->is_valid()) {
    *error = util::error_json(
        "missing or invalid required parameter: space_rid (required for "
        "dimension 3)");
    return false;
  }
  auto *server = godot::PhysicsServer3D::get_singleton();
  if (!server) {
    *error = util::error_json("PhysicsServer3D not available");
    return false;
  }
  *state = server->space_get_direct_state(*space);
  if (!*state) {
    *error = util::error_json("space_get_direct_state returned null: the space "
                              "must be active and attached to a scene");
    return false;
  }
  return true;
}

bool build_query_shape_2d(
    const JV &args, godot::Ref<godot::PhysicsShapeQueryParameters2D> *params,
    JV *error) {
  godot::RID shape = util::resolve_rid<PhysicsRidDomain>(args, "shape_rid");
  if (!shape.is_valid()) {
    *error = util::error_json(
        "missing or invalid required parameter: shape_rid");
    return false;
  }
  params->instantiate();
  (*params)->set_shape_rid(shape);
  if (args.Contains("transform")) {
    godot::Transform2D transform;
    if (!parse_query_transform2d(args, "transform", &transform, error))
      return false;
    (*params)->set_transform(transform);
  }
  if (args.Contains("motion")) {
    godot::Vector2 motion;
    if (!parse_query_vec2(args, "motion", &motion, error))
      return false;
    (*params)->set_motion(motion);
  }
  if (args.Contains("collision_mask"))
    (*params)->set_collision_mask(
        static_cast<uint32_t>(args["collision_mask"].GetInt()));
  if (args.Contains("exclude"))
    (*params)->set_exclude(parse_rids(args["exclude"]));
  if (args.Contains("collide_with_bodies"))
    (*params)->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
  if (args.Contains("collide_with_areas"))
    (*params)->set_collide_with_areas(args["collide_with_areas"].GetBool());
  return true;
}

bool build_query_shape_3d(
    const JV &args, godot::Ref<godot::PhysicsShapeQueryParameters3D> *params,
    JV *error) {
  godot::RID shape = util::resolve_rid<PhysicsRidDomain>(args, "shape_rid");
  if (!shape.is_valid()) {
    *error = util::error_json(
        "missing or invalid required parameter: shape_rid");
    return false;
  }
  params->instantiate();
  (*params)->set_shape_rid(shape);
  if (args.Contains("transform")) {
    godot::Transform3D transform;
    if (!parse_query_transform3d(args, "transform", &transform, error))
      return false;
    (*params)->set_transform(transform);
  }
  if (args.Contains("motion")) {
    godot::Vector3 motion;
    if (!parse_query_vec3(args, "motion", &motion, error))
      return false;
    (*params)->set_motion(motion);
  }
  if (args.Contains("collision_mask"))
    (*params)->set_collision_mask(
        static_cast<uint32_t>(args["collision_mask"].GetInt()));
  if (args.Contains("exclude"))
    (*params)->set_exclude(parse_rids(args["exclude"]));
  if (args.Contains("collide_with_bodies"))
    (*params)->set_collide_with_bodies(args["collide_with_bodies"].GetBool());
  if (args.Contains("collide_with_areas"))
    (*params)->set_collide_with_areas(args["collide_with_areas"].GetBool());
  return true;
}

JV query_success(JV payload, const char *mode, int dimension) {
  JV result(JV::object_tag);
  result["result"] = std::move(payload);
  result["mode"] = JV(mode);
  result["dimension"] = JV(dimension);
  return result;
}

bool validate_query_intersect(const JV &args, int dimension,
                              const std::string &mode, JV *error) {
  if (mode == "intersect_point") {
    if (!args.Contains("position"))
      return true;
    if (dimension == 2) {
      godot::Vector2 position;
      return parse_query_vec2(args, "position", &position, error);
    }
    godot::Vector3 position;
    return parse_query_vec3(args, "position", &position, error);
  }
  if (mode == "intersect_ray") {
    if (args.Contains("from")) {
      if (dimension == 2) {
        godot::Vector2 from;
        if (!parse_query_vec2(args, "from", &from, error))
          return false;
      } else {
        godot::Vector3 from;
        if (!parse_query_vec3(args, "from", &from, error))
          return false;
      }
    }
    if (args.Contains("to")) {
      if (dimension == 2) {
        godot::Vector2 to;
        if (!parse_query_vec2(args, "to", &to, error))
          return false;
      } else {
        godot::Vector3 to;
        if (!parse_query_vec3(args, "to", &to, error))
          return false;
      }
    }
    return true;
  }
  if (args.Contains("transform")) {
    if (dimension == 2) {
      godot::Transform2D transform;
      if (!parse_query_transform2d(args, "transform", &transform, error))
        return false;
    } else {
      godot::Transform3D transform;
      if (!parse_query_transform3d(args, "transform", &transform, error))
        return false;
    }
  }
  if (args.Contains("motion")) {
    if (dimension == 2) {
      godot::Vector2 motion;
      if (!parse_query_vec2(args, "motion", &motion, error))
        return false;
    } else {
      godot::Vector3 motion;
      if (!parse_query_vec3(args, "motion", &motion, error))
        return false;
    }
  }
  return true;
}

JV normalize_query_transform(const JV &args, int dimension) {
  JV out(JV::object_tag);
  if (args.IsObject())
    out = args;
  auto *transform = out.Find("transform");
  if (!transform || !transform->IsObject())
    return out;
  const bool has_fields =
      dimension == 2
          ? (transform->Contains("origin") ||
             transform->Contains("rotation") || transform->Contains("scale"))
          : (transform->Contains("origin") ||
             transform->Contains("basis"));
  if (has_fields)
    return out;
  auto *x = transform->Find("x");
  auto *y = transform->Find("y");
  auto *z = transform->Find("z");
  if (!x || !y || (dimension == 3 && !z))
    return out;
  JV origin(JV::object_tag);
  origin["x"] = *x;
  origin["y"] = *y;
  if (dimension == 3)
    origin["z"] = *z;
  JV normalized(JV::object_tag);
  normalized["origin"] = std::move(origin);
  out["transform"] = std::move(normalized);
  return out;
}

JV query_intersect(const JV &args, int dimension, const std::string &mode) {
  const JV normalized = normalize_query_transform(args, dimension);
  JV result;
  if (dimension == 2) {
    if (mode == "intersect_point")
      result = handle_2d_intersect_point(normalized);
    else if (mode == "intersect_ray")
      result = handle_2d_ray_cast(normalized);
    else
      result = handle_2d_intersect_shape(normalized);
  } else {
    if (mode == "intersect_point")
      result = handle_3d_intersect_point(normalized);
    else if (mode == "intersect_ray")
      result = handle_3d_ray_cast(normalized);
    else
      result = handle_3d_intersect_shape(normalized);
  }
  if (result.IsObject()) {
    result["mode"] = JV(mode);
    result["dimension"] = JV(dimension);
  }
  return result;
}

JV query_rest_info_json(const godot::Dictionary &info, int dimension) {
  auto vec = [dimension](const godot::Variant &value) -> JV {
    if (dimension == 2)
      return util::vec2_to_json(godot::Vector2(value));
    return util::vec3_to_json(godot::Vector3(value));
  };
  JV result(JV::object_tag);
  if (info.has("point"))
    result["point"] = vec(info["point"]);
  if (info.has("normal"))
    result["normal"] = vec(info["normal"]);
  if (info.has("linear_velocity"))
    result["linear_velocity"] = vec(info["linear_velocity"]);
  if (info.has("rid")) {
    godot::RID rid = info["rid"];
    result["rid"] = JV(util::rid_store<PhysicsRidDomain>().store(rid));
  }
  if (info.has("collider_id"))
    result["collider_id"] = JV(static_cast<int64_t>(info["collider_id"]));
  if (info.has("shape"))
    result["shape"] = JV(static_cast<int>(info["shape"]));
  return result;
}

JV handle_query_shape_modes(const JV &args, int dimension,
                            const std::string &mode) {
  JV error;
  if (!require_query_key(args, mode.c_str(), "space_rid", &error))
    return error;
  if (!require_query_key(args, mode.c_str(), "shape_rid", &error))
    return error;
  if (mode == "cast_motion" &&
      !require_query_key(args, mode.c_str(), "motion", &error))
    return error;
  if (dimension == 2) {
    godot::PhysicsDirectSpaceState2D *state = nullptr;
    godot::RID space;
    if (!resolve_query_space_2d(args, &state, &space, &error))
      return error;
    godot::Ref<godot::PhysicsShapeQueryParameters2D> params;
    if (!build_query_shape_2d(args, &params, &error))
      return error;
    JV payload;
    if (mode == "cast_motion") {
      auto fractions = state->cast_motion(params);
      if (fractions.size() != 2) {
        return util::error_json(
            "cast_motion returned no safe/unsafe fraction pair");
      }
      JV value(JV::object_tag);
      value["safe"] = JV(static_cast<double>(fractions[0]));
      value["unsafe"] = JV(static_cast<double>(fractions[1]));
      payload = std::move(value);
    } else if (mode == "collide_shape") {
      auto *max_value = args.Find("max_results");
      const int64_t max_results = max_value ? max_value->GetInt() : 32;
      auto points =
          state->collide_shape(params, static_cast<int32_t>(max_results));
      JV values(JV::array_tag);
      for (int i = 0; i < points.size(); i++)
        values.PushBack(util::vec2_to_json(godot::Vector2(points[i])));
      JV value(JV::object_tag);
      value["points"] = std::move(values);
      payload = std::move(value);
    } else {
      godot::Dictionary info = state->get_rest_info(params);
      payload = info.is_empty() ? JV(nullptr)
                                : query_rest_info_json(info, dimension);
    }
    JV result = query_success(std::move(payload), mode.c_str(), dimension);
    result["space_rid"] = JV(util::rid_store<PhysicsRidDomain>().store(space));
    return result;
  }
  godot::PhysicsDirectSpaceState3D *state = nullptr;
  godot::RID space;
  if (!resolve_query_space_3d(args, &state, &space, &error))
    return error;
  godot::Ref<godot::PhysicsShapeQueryParameters3D> params;
  if (!build_query_shape_3d(args, &params, &error))
    return error;
  JV payload;
  if (mode == "cast_motion") {
    auto fractions = state->cast_motion(params);
    if (fractions.size() != 2) {
      return util::error_json(
          "cast_motion returned no safe/unsafe fraction pair");
    }
    JV value(JV::object_tag);
    value["safe"] = JV(static_cast<double>(fractions[0]));
    value["unsafe"] = JV(static_cast<double>(fractions[1]));
    payload = std::move(value);
  } else if (mode == "collide_shape") {
    auto *max_value = args.Find("max_results");
    const int64_t max_results = max_value ? max_value->GetInt() : 32;
    auto points =
        state->collide_shape(params, static_cast<int32_t>(max_results));
    JV values(JV::array_tag);
    for (int i = 0; i < points.size(); i++)
      values.PushBack(util::vec3_to_json(godot::Vector3(points[i])));
    JV value(JV::object_tag);
    value["points"] = std::move(values);
    payload = std::move(value);
  } else {
    godot::Dictionary info = state->get_rest_info(params);
    payload = info.is_empty() ? JV(nullptr)
                              : query_rest_info_json(info, dimension);
  }
  JV result = query_success(std::move(payload), mode.c_str(), dimension);
  result["space_rid"] = JV(util::rid_store<PhysicsRidDomain>().store(space));
  return result;
}

JV query_test_collision_2d(
    const godot::Ref<godot::PhysicsTestMotionResult2D> &result) {
  JV collision(JV::object_tag);
  collision["point"] = util::vec2_to_json(result->get_collision_point());
  collision["normal"] = util::vec2_to_json(result->get_collision_normal());
  collision["collider_id"] =
      JV(static_cast<int64_t>(result->get_collider_id()));
  collision["collider_rid"] = JV(util::rid_store<PhysicsRidDomain>().store(
      result->get_collider_rid()));
  collision["collider_shape"] = JV(result->get_collider_shape());
  collision["local_shape"] = JV(result->get_collision_local_shape());
  collision["depth"] = JV(static_cast<double>(result->get_collision_depth()));
  collision["safe_fraction"] =
      JV(static_cast<double>(result->get_collision_safe_fraction()));
  collision["unsafe_fraction"] =
      JV(static_cast<double>(result->get_collision_unsafe_fraction()));
  return collision;
}

JV query_test_collision_3d(
    const godot::Ref<godot::PhysicsTestMotionResult3D> &result, int index) {
  JV collision(JV::object_tag);
  collision["point"] =
      util::vec3_to_json(result->get_collision_point(index));
  collision["normal"] =
      util::vec3_to_json(result->get_collision_normal(index));
  collision["collider_id"] =
      JV(static_cast<int64_t>(result->get_collider_id(index)));
  collision["collider_rid"] = JV(util::rid_store<PhysicsRidDomain>().store(
      result->get_collider_rid(index)));
  collision["collider_shape"] = JV(result->get_collider_shape(index));
  collision["local_shape"] = JV(result->get_collision_local_shape(index));
  collision["depth"] =
      JV(static_cast<double>(result->get_collision_depth(index)));
  return collision;
}

JV handle_query_test_motion(const JV &args, int dimension) {
  JV error;
  if (!require_query_key(args, "test_motion", "body_rid", &error))
    return error;
  if (!require_query_key(args, "test_motion", "from", &error))
    return error;
  if (!require_query_key(args, "test_motion", "motion", &error))
    return error;
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "body_rid");
  if (!body.is_valid())
    return util::error_json(
        "missing or invalid required parameter for mode 'test_motion': "
        "body_rid");
  if (dimension == 2) {
    auto *server = godot::PhysicsServer2D::get_singleton();
    if (!server)
      return util::error_json("PhysicsServer2D not available");
    godot::Transform2D from;
    if (!parse_query_transform2d(args, "from", &from, &error))
      return error;
    godot::Vector2 motion;
    if (!parse_query_vec2(args, "motion", &motion, &error))
      return error;
    godot::Ref<godot::PhysicsTestMotionParameters2D> params;
    params.instantiate();
    params->set_from(from);
    params->set_motion(motion);
    if (args.Contains("margin"))
      params->set_margin(
          static_cast<float>(util::json_number(args.Find("margin"), 0.0)));
    if (args.Contains("collide_separation_ray"))
      params->set_collide_separation_ray_enabled(
          args["collide_separation_ray"].GetBool());
    if (args.Contains("recovery_as_collision"))
      params->set_recovery_as_collision_enabled(
          args["recovery_as_collision"].GetBool());
    if (args.Contains("exclude"))
      params->set_exclude_bodies(parse_rids(args["exclude"]));
    godot::Ref<godot::PhysicsTestMotionResult2D> result;
    result.instantiate();
    const bool collided = server->body_test_motion(body, params, result);
    JV payload(JV::object_tag);
    payload["safe"] = JV(!collided);
    payload["unsafe"] = JV(collided);
    payload["travel"] = util::vec2_to_json(result->get_travel());
    payload["remainder"] = util::vec2_to_json(result->get_remainder());
    payload["safe_fraction"] =
        JV(static_cast<double>(result->get_collision_safe_fraction()));
    payload["unsafe_fraction"] =
        JV(static_cast<double>(result->get_collision_unsafe_fraction()));
    JV collisions(JV::array_tag);
    if (collided) {
      JV collision = query_test_collision_2d(result);
      payload["collision"] = collision;
      collisions.PushBack(std::move(collision));
    } else {
      payload["collision"] = JV(nullptr);
    }
    payload["collisions"] = std::move(collisions);
    return query_success(std::move(payload), "test_motion", dimension);
  }
  auto *server = godot::PhysicsServer3D::get_singleton();
  if (!server)
    return util::error_json("PhysicsServer3D not available");
  godot::Transform3D from;
  if (!parse_query_transform3d(args, "from", &from, &error))
    return error;
  godot::Vector3 motion;
  if (!parse_query_vec3(args, "motion", &motion, &error))
    return error;
  godot::Ref<godot::PhysicsTestMotionParameters3D> params;
  params.instantiate();
  params->set_from(from);
  params->set_motion(motion);
  if (args.Contains("margin"))
    params->set_margin(
        static_cast<float>(util::json_number(args.Find("margin"), 0.0)));
  if (args.Contains("collide_separation_ray"))
    params->set_collide_separation_ray_enabled(
        args["collide_separation_ray"].GetBool());
  if (args.Contains("recovery_as_collision"))
    params->set_recovery_as_collision_enabled(
        args["recovery_as_collision"].GetBool());
  if (args.Contains("exclude"))
    params->set_exclude_bodies(parse_rids(args["exclude"]));
  godot::Ref<godot::PhysicsTestMotionResult3D> result;
  result.instantiate();
  const bool collided = server->body_test_motion(body, params, result);
  const int collision_count = result->get_collision_count();
  JV payload(JV::object_tag);
  payload["safe"] = JV(!collided);
  payload["unsafe"] = JV(collided);
  payload["travel"] = util::vec3_to_json(result->get_travel());
  payload["remainder"] = util::vec3_to_json(result->get_remainder());
  payload["safe_fraction"] =
      JV(static_cast<double>(result->get_collision_safe_fraction()));
  payload["unsafe_fraction"] =
      JV(static_cast<double>(result->get_collision_unsafe_fraction()));
  JV collisions(JV::array_tag);
  for (int i = 0; i < collision_count; i++)
    collisions.PushBack(query_test_collision_3d(result, i));
  if (collision_count > 0)
    payload["collision"] = query_test_collision_3d(result, 0);
  else
    payload["collision"] = JV(nullptr);
  payload["collisions"] = std::move(collisions);
  return query_success(std::move(payload), "test_motion", dimension);
}

JV handle_query_contacts(const JV &args, int dimension) {
  JV error;
  if (!require_query_key(args, "contacts", "body_rid", &error))
    return error;
  godot::RID body = util::resolve_rid<PhysicsRidDomain>(args, "body_rid");
  if (!body.is_valid())
    return util::error_json(
        "missing or invalid required parameter for mode 'contacts': body_rid");
  if (dimension == 2) {
    auto *server = godot::PhysicsServer2D::get_singleton();
    if (!server)
      return util::error_json("PhysicsServer2D not available");
    auto *state = server->body_get_direct_state(body);
    if (!state)
      return util::error_json("body_get_direct_state returned null: the body "
                              "must be in an active physics space");
    const int count = state->get_contact_count();
    JV contacts(JV::array_tag);
    for (int i = 0; i < count; i++) {
      JV contact(JV::object_tag);
      contact["local_position"] =
          util::vec2_to_json(state->get_contact_local_position(i));
      contact["normal"] =
          util::vec2_to_json(state->get_contact_local_normal(i));
      contact["collider_position"] =
          util::vec2_to_json(state->get_contact_collider_position(i));
      contact["impulse"] = util::vec2_to_json(state->get_contact_impulse(i));
      contact["collider_id"] =
          JV(static_cast<int64_t>(state->get_contact_collider_id(i)));
      contact["collider_shape"] =
          JV(state->get_contact_collider_shape(i));
      contact["collider_rid"] = JV(util::rid_store<PhysicsRidDomain>().store(
          state->get_contact_collider(i)));
      contacts.PushBack(std::move(contact));
    }
    JV payload(JV::object_tag);
    payload["count"] = JV(count);
    payload["contacts"] = std::move(contacts);
    return query_success(std::move(payload), "contacts", dimension);
  }
  auto *server = godot::PhysicsServer3D::get_singleton();
  if (!server)
    return util::error_json("PhysicsServer3D not available");
  auto *state = server->body_get_direct_state(body);
  if (!state)
    return util::error_json("body_get_direct_state returned null: the body "
                            "must be in an active physics space");
  const int count = state->get_contact_count();
  JV contacts(JV::array_tag);
  for (int i = 0; i < count; i++) {
    JV contact(JV::object_tag);
    contact["local_position"] =
        util::vec3_to_json(state->get_contact_local_position(i));
    contact["normal"] =
        util::vec3_to_json(state->get_contact_local_normal(i));
    contact["collider_position"] =
        util::vec3_to_json(state->get_contact_collider_position(i));
    contact["impulse"] = util::vec3_to_json(state->get_contact_impulse(i));
    contact["collider_id"] =
        JV(static_cast<int64_t>(state->get_contact_collider_id(i)));
    contact["collider_shape"] = JV(state->get_contact_collider_shape(i));
    contact["collider_rid"] = JV(util::rid_store<PhysicsRidDomain>().store(
        state->get_contact_collider(i)));
    contacts.PushBack(std::move(contact));
  }
  JV payload(JV::object_tag);
  payload["count"] = JV(count);
  payload["contacts"] = std::move(contacts);
  return query_success(std::move(payload), "contacts", dimension);
}

JV query_node_ref_json(godot::Node *node, const godot::RID &rid) {
  JV entry(JV::object_tag);
  entry["rid"] = JV(util::rid_store<PhysicsRidDomain>().store(rid));
  entry["collider_id"] = JV(static_cast<int64_t>(node->get_instance_id()));
  entry["name"] = JV(util::to_std(node->get_name()));
  entry["path"] = JV(util::to_std(node->get_path()));
  return entry;
}

godot::Area3D *resolve_query_area_3d(const JV &args, JV *error) {
  auto *node_path = args.Find("node_path");
  auto *area_value = args.Find("area_rid");
  if (node_path && area_value) {
    *error = util::error_json(
        "invalid parameter for mode 'area_overlaps': pass either area_rid or "
        "node_path, not both");
    return nullptr;
  }
  if (node_path) {
    if (!node_path->IsString()) {
      *error = util::error_json(
          "invalid parameter for mode 'area_overlaps': node_path must be a "
          "string");
      return nullptr;
    }
    auto *editor = godot::EditorInterface::get_singleton();
    if (!editor) {
      *error = util::error_json("EditorInterface not available");
      return nullptr;
    }
    auto *root = editor->get_edited_scene_root();
    if (!root) {
      *error = util::error_json("no edited scene root");
      return nullptr;
    }
    const std::string path = node_path->GetString();
    godot::Node *node = util::resolve_scene_node(path, root);
    if (!node) {
      *error = util::error_json("node not found: " + path);
      return nullptr;
    }
    auto *area = godot::Object::cast_to<godot::Area3D>(node);
    if (!area) {
      *error = util::error_json("node is not an Area3D: " + path);
      return nullptr;
    }
    return area;
  }
  if (area_value) {
    godot::RID area_rid = util::resolve_rid<PhysicsRidDomain>(args, "area_rid");
    if (!area_rid.is_valid()) {
      *error = util::error_json(
          "missing or invalid required parameter for mode 'area_overlaps': "
          "area_rid");
      return nullptr;
    }
    auto *server = godot::PhysicsServer3D::get_singleton();
    if (!server) {
      *error = util::error_json("PhysicsServer3D not available");
      return nullptr;
    }
    const uint64_t instance_id = server->area_get_object_instance_id(area_rid);
    auto *area = godot::Object::cast_to<godot::Area3D>(
        godot::ObjectDB::get_instance(instance_id));
    if (!area) {
      *error = util::error_json(
          "area_rid is not attached to an Area3D node; pass node_path of an "
          "Area3D instead");
      return nullptr;
    }
    return area;
  }
  *error = util::error_json(
      "missing required parameter for mode 'area_overlaps': area_rid or "
      "node_path");
  return nullptr;
}

godot::Area2D *resolve_query_area_2d(const JV &args, JV *error) {
  auto *node_path = args.Find("node_path");
  auto *area_value = args.Find("area_rid");
  if (node_path && area_value) {
    *error = util::error_json(
        "invalid parameter for mode 'area_overlaps': pass either area_rid or "
        "node_path, not both");
    return nullptr;
  }
  if (node_path) {
    if (!node_path->IsString()) {
      *error = util::error_json(
          "invalid parameter for mode 'area_overlaps': node_path must be a "
          "string");
      return nullptr;
    }
    auto *editor = godot::EditorInterface::get_singleton();
    if (!editor) {
      *error = util::error_json("EditorInterface not available");
      return nullptr;
    }
    auto *root = editor->get_edited_scene_root();
    if (!root) {
      *error = util::error_json("no edited scene root");
      return nullptr;
    }
    const std::string path = node_path->GetString();
    godot::Node *node = util::resolve_scene_node(path, root);
    if (!node) {
      *error = util::error_json("node not found: " + path);
      return nullptr;
    }
    auto *area = godot::Object::cast_to<godot::Area2D>(node);
    if (!area) {
      *error = util::error_json("node is not an Area2D: " + path);
      return nullptr;
    }
    return area;
  }
  if (area_value) {
    godot::RID area_rid = util::resolve_rid<PhysicsRidDomain>(args, "area_rid");
    if (!area_rid.is_valid()) {
      *error = util::error_json(
          "missing or invalid required parameter for mode 'area_overlaps': "
          "area_rid");
      return nullptr;
    }
    auto *server = godot::PhysicsServer2D::get_singleton();
    if (!server) {
      *error = util::error_json("PhysicsServer2D not available");
      return nullptr;
    }
    const uint64_t instance_id = server->area_get_object_instance_id(area_rid);
    auto *area = godot::Object::cast_to<godot::Area2D>(
        godot::ObjectDB::get_instance(instance_id));
    if (!area) {
      *error = util::error_json(
          "area_rid is not attached to an Area2D node; pass node_path of an "
          "Area2D instead");
      return nullptr;
    }
    return area;
  }
  *error = util::error_json(
      "missing required parameter for mode 'area_overlaps': area_rid or "
      "node_path");
  return nullptr;
}

JV handle_query_area_overlaps(const JV &args, int dimension) {
  JV error;
  if (dimension == 2) {
    godot::Area2D *area = resolve_query_area_2d(args, &error);
    if (!area)
      return error;
    JV bodies(JV::array_tag);
    auto overlapping_bodies = area->get_overlapping_bodies();
    for (int i = 0; i < overlapping_bodies.size(); i++) {
      auto *collider = godot::Object::cast_to<godot::CollisionObject2D>(
          overlapping_bodies[i]);
      if (collider)
        bodies.PushBack(query_node_ref_json(collider, collider->get_rid()));
    }
    JV areas(JV::array_tag);
    auto overlapping_areas = area->get_overlapping_areas();
    for (int i = 0; i < overlapping_areas.size(); i++) {
      auto *collider = godot::Object::cast_to<godot::CollisionObject2D>(
          overlapping_areas[i]);
      if (collider)
        areas.PushBack(query_node_ref_json(collider, collider->get_rid()));
    }
    JV payload(JV::object_tag);
    payload["bodies"] = std::move(bodies);
    payload["areas"] = std::move(areas);
    return query_success(std::move(payload), "area_overlaps", dimension);
  }
  godot::Area3D *area = resolve_query_area_3d(args, &error);
  if (!area)
    return error;
  JV bodies(JV::array_tag);
  auto overlapping_bodies = area->get_overlapping_bodies();
  for (int i = 0; i < overlapping_bodies.size(); i++) {
    auto *collider =
        godot::Object::cast_to<godot::CollisionObject3D>(overlapping_bodies[i]);
    if (collider)
      bodies.PushBack(query_node_ref_json(collider, collider->get_rid()));
  }
  JV areas(JV::array_tag);
  auto overlapping_areas = area->get_overlapping_areas();
  for (int i = 0; i < overlapping_areas.size(); i++) {
    auto *collider =
        godot::Object::cast_to<godot::CollisionObject3D>(overlapping_areas[i]);
    if (collider)
      areas.PushBack(query_node_ref_json(collider, collider->get_rid()));
  }
  JV payload(JV::object_tag);
  payload["bodies"] = std::move(bodies);
  payload["areas"] = std::move(areas);
  return query_success(std::move(payload), "area_overlaps", dimension);
}

} // namespace

JV handle_physics_query(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "physics_query called");
  JV error;
  int dimension = 0;
  if (!parse_query_dimension(args, &dimension, &error))
    return error;
  std::string mode;
  if (!parse_query_mode(args, &mode, &error))
    return error;
  JV result;
  if (mode == "intersect_point" || mode == "intersect_ray" ||
      mode == "intersect_shape") {
    if (!validate_query_intersect(args, dimension, mode, &error))
      return error;
    result = query_intersect(args, dimension, mode);
  } else if (mode == "cast_motion" || mode == "collide_shape" ||
             mode == "rest_info") {
    result = handle_query_shape_modes(args, dimension, mode);
  } else if (mode == "test_motion") {
    result = handle_query_test_motion(args, dimension);
  } else if (mode == "contacts") {
    result = handle_query_contacts(args, dimension);
  } else {
    result = handle_query_area_overlaps(args, dimension);
  }
  if (result.IsObject() && result.Find("error") == nullptr)
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "physics_query completed");
  return result;
}

} // namespace physics_ops
} // namespace godot_autopilot
