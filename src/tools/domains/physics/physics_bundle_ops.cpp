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
