#include "tools/domains/mesh/mesh_ops.hpp"
#include "core/config.hpp"
#include "core/resource_registry.hpp"
#include "tools/domains/resource/resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/input_guards.hpp"
#include "util/json_godot.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/classes/csg_shape3d.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/plane.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#ifdef GetObject
#undef GetObject
#endif
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace mesh_ops {

namespace {

constexpr int64_t kMeshMaxVertices = 65536;
constexpr int64_t kMeshMaxSessions = 32;
constexpr int64_t kMeshMaxInstances = 100000;
constexpr int64_t kMeshMaxBufferValues = 2000000;
constexpr double kMeshDegToRad = 3.14159265358979323846 / 180.0;

struct MeshArraySlot {
  const char *key;
  godot::Mesh::ArrayType type;
};

const MeshArraySlot kMeshArraySlots[] = {
    {"vertex", godot::Mesh::ARRAY_VERTEX},
    {"normal", godot::Mesh::ARRAY_NORMAL},
    {"tangent", godot::Mesh::ARRAY_TANGENT},
    {"color", godot::Mesh::ARRAY_COLOR},
    {"tex_uv", godot::Mesh::ARRAY_TEX_UV},
    {"tex_uv2", godot::Mesh::ARRAY_TEX_UV2},
    {"index", godot::Mesh::ARRAY_INDEX},
};

struct MeshSessionState {
  godot::Ref<godot::SurfaceTool> tool;
  int64_t vertex_count = 0;
  bool has_uv = false;
  bool has_normal = false;
  bool has_color = false;
};

std::unordered_map<std::string, MeshSessionState> g_mesh_sessions;
int64_t g_mesh_session_sequence = 0;

std::string mesh_action_of(const mcp::JsonValue &args) {
  const mcp::JsonValue *value = args.Find("action");
  if (value == nullptr || !value->IsString()) {
    return std::string();
  }
  return value->GetString();
}

std::string mesh_missing_param(const std::string &action, const char *name) {
  return "missing required parameter for action '" + action + "': " + name;
}

std::string mesh_invalid_param(const char *name, const std::string &expected) {
  return std::string("invalid parameter: ") + name + " must be " + expected;
}

bool mesh_read_required_string(const mcp::JsonValue &args, const char *key,
                               const std::string &action, std::string &out,
                               std::string &error) {
  const util::StringArgStatus status =
      util::string_arg_status(args, key, out, true);
  if (status == util::StringArgStatus::Missing) {
    error = mesh_missing_param(action, key);
    return false;
  }
  if (status != util::StringArgStatus::Ok) {
    error = mesh_invalid_param(key, "a non-empty string");
    return false;
  }
  return true;
}

bool mesh_read_optional_string(const mcp::JsonValue &args, const char *key,
                               std::string &out, std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    return true;
  }
  if (!value->IsString()) {
    error = mesh_invalid_param(key, "a string");
    return false;
  }
  out = value->GetString();
  return true;
}

bool mesh_read_required_int(const mcp::JsonValue &args, const char *key,
                            const std::string &action, int64_t &out,
                            std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    error = mesh_missing_param(action, key);
    return false;
  }
  if (!value->IsNumber()) {
    error = mesh_invalid_param(key, "a number");
    return false;
  }
  out = value->IsInt() ? value->GetInt()
                       : static_cast<int64_t>(value->GetDouble());
  return true;
}

bool mesh_read_int_or(const mcp::JsonValue &args, const char *key,
                      int64_t fallback, int64_t &out, std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    out = fallback;
    return true;
  }
  if (!value->IsNumber()) {
    error = mesh_invalid_param(key, "a number");
    return false;
  }
  out = value->IsInt() ? value->GetInt()
                       : static_cast<int64_t>(value->GetDouble());
  return true;
}

bool mesh_read_bool_or(const mcp::JsonValue &args, const char *key,
                       bool fallback, bool &out, std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    out = fallback;
    return true;
  }
  if (!value->IsBool()) {
    error = mesh_invalid_param(key, "a boolean");
    return false;
  }
  out = value->GetBool();
  return true;
}

bool mesh_read_number(const mcp::JsonValue &value, const char *name, double &out,
                      std::string &error) {
  if (!value.IsNumber()) {
    error = mesh_invalid_param(name, "a number");
    return false;
  }
  out = value.IsInt() ? static_cast<double>(value.GetInt()) : value.GetDouble();
  return true;
}

bool mesh_read_vec2(const mcp::JsonValue &value, const char *name,
                    godot::Vector2 &out, std::string &error) {
  if (!value.IsObject()) {
    error = mesh_invalid_param(name, "an object {x, y}");
    return false;
  }
  for (const auto &entry : value.GetObject()) {
    if (entry.first != "x" && entry.first != "y") {
      error = std::string("invalid parameter: ") + name +
              " has unknown field '" + entry.first + "' (allowed: x, y)";
      return false;
    }
  }
  const mcp::JsonValue *px = value.Find("x");
  const mcp::JsonValue *py = value.Find("y");
  if (px == nullptr || py == nullptr) {
    error = mesh_invalid_param(name, "an object {x, y}");
    return false;
  }
  double x = 0.0;
  double y = 0.0;
  if (!mesh_read_number(*px, "x", x, error) ||
      !mesh_read_number(*py, "y", y, error)) {
    return false;
  }
  out = godot::Vector2(static_cast<float>(x), static_cast<float>(y));
  return true;
}

bool mesh_read_vec3(const mcp::JsonValue &value, const char *name,
                    godot::Vector3 &out, std::string &error) {
  if (!value.IsObject()) {
    error = mesh_invalid_param(name, "an object {x, y, z}");
    return false;
  }
  for (const auto &entry : value.GetObject()) {
    if (entry.first != "x" && entry.first != "y" && entry.first != "z") {
      error = std::string("invalid parameter: ") + name +
              " has unknown field '" + entry.first +
              "' (allowed: x, y, z)";
      return false;
    }
  }
  const mcp::JsonValue *px = value.Find("x");
  const mcp::JsonValue *py = value.Find("y");
  const mcp::JsonValue *pz = value.Find("z");
  if (px == nullptr || py == nullptr || pz == nullptr) {
    error = mesh_invalid_param(name, "an object {x, y, z}");
    return false;
  }
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  if (!mesh_read_number(*px, "x", x, error) ||
      !mesh_read_number(*py, "y", y, error) ||
      !mesh_read_number(*pz, "z", z, error)) {
    return false;
  }
  out = godot::Vector3(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(z));
  return true;
}

bool mesh_read_color(const mcp::JsonValue &value, const char *name,
                     godot::Color &out, std::string &error) {
  if (!value.IsObject()) {
    error = mesh_invalid_param(name, "an object {r, g, b, a}");
    return false;
  }
  const char *channels[4] = {"r", "g", "b", "a"};
  for (const auto &entry : value.GetObject()) {
    bool known = false;
    for (const char *channel : channels) {
      if (entry.first == channel) {
        known = true;
        break;
      }
    }
    if (!known) {
      error = std::string("invalid parameter: ") + name +
              " has unknown field '" + entry.first +
              "' (allowed: r, g, b, a)";
      return false;
    }
  }
  for (const char *channel : channels) {
    const mcp::JsonValue *component = value.Find(channel);
    if (component == nullptr) {
      if (std::string(channel) == "a") {
        continue;
      }
      error = std::string("invalid parameter: ") + name + " requires " +
              channel;
      return false;
    }
    if (!component->IsNumber()) {
      error = std::string("invalid parameter: ") + name + "." + channel +
              " must be a number";
      return false;
    }
  }
  out = util::json_to_color(value);
  return true;
}

mcp::JsonValue mesh_aabb_to_json(const godot::AABB &aabb) {
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["position"] = util::vec3_to_json(aabb.position);
  j["size"] = util::vec3_to_json(aabb.size);
  return j;
}

template <typename T>
std::string mesh_effective_name(const godot::Ref<T> &res,
                                const std::string &name) {
  if (!name.empty()) {
    return name;
  }
  return util::to_std(res->get_name());
}

template <typename T>
mcp::JsonValue mesh_resource_info(const godot::Ref<T> &res,
                                  const std::string &name,
                                  const char *ref_key) {
  const std::string effective = mesh_effective_name(res, name);
  const int64_t object_id = static_cast<int64_t>(res->get_instance_id());
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["class"] = mcp::JsonValue(util::to_std(res->get_class()));
  j["object_id"] = mcp::JsonValue(object_id);
  j["object_id_str"] = mcp::JsonValue(std::to_string(object_id));
  j["name"] = mcp::JsonValue(effective);
  j["path"] = mcp::JsonValue(util::to_std(res->get_path()));
  j[ref_key] = mcp::JsonValue(
      effective.empty() ? std::to_string(object_id) : "memory://" + effective);
  return j;
}

template <typename T>
void mesh_register_named_resource(const godot::Ref<T> &res,
                                  const std::string &name) {
  if (res.is_null()) {
    return;
  }
  if (!name.empty()) {
    res->set_name(godot::String(name.c_str()));
    res->set_path(godot::String(("memory://" + name).c_str()));
  }
  godot::Ref<godot::Resource> as_resource = res;
  resource_registry::register_resource(as_resource, name);
}

std::string mesh_describe_locator(const mcp::JsonValue &value) {
  if (value.IsString()) {
    return value.GetString();
  }
  if (value.IsObject()) {
    const mcp::JsonValue *name = value.Find("name");
    if (name != nullptr && name->IsString() && !name->GetString().empty()) {
      return name->GetString();
    }
    const mcp::JsonValue *path = value.Find("path");
    if (path != nullptr && path->IsString() && !path->GetString().empty()) {
      return path->GetString();
    }
    const mcp::JsonValue *oid = value.Find("object_id");
    if (oid != nullptr && oid->IsNumber()) {
      return "object_id " + std::to_string(oid->IsInt()
                                               ? oid->GetInt()
                                               : static_cast<int64_t>(
                                                     oid->GetDouble()));
    }
  }
  return "locator";
}

bool mesh_parse_primitive(const std::string &name,
                          godot::Mesh::PrimitiveType &out) {
  struct PrimitiveName {
    const char *name;
    godot::Mesh::PrimitiveType type;
  };
  static const PrimitiveName kPrimitiveNames[] = {
      {"points", godot::Mesh::PRIMITIVE_POINTS},
      {"lines", godot::Mesh::PRIMITIVE_LINES},
      {"line_strip", godot::Mesh::PRIMITIVE_LINE_STRIP},
      {"triangles", godot::Mesh::PRIMITIVE_TRIANGLES},
      {"triangle_strip", godot::Mesh::PRIMITIVE_TRIANGLE_STRIP},
  };
  const std::string key = util::ascii_lowercase(name);
  for (const PrimitiveName &entry : kPrimitiveNames) {
    if (key == entry.name) {
      out = entry.type;
      return true;
    }
  }
  return false;
}

std::string mesh_primitive_name_of(godot::Mesh::PrimitiveType type) {
  switch (type) {
    case godot::Mesh::PRIMITIVE_POINTS:
      return "points";
    case godot::Mesh::PRIMITIVE_LINES:
      return "lines";
    case godot::Mesh::PRIMITIVE_LINE_STRIP:
      return "line_strip";
    case godot::Mesh::PRIMITIVE_TRIANGLES:
      return "triangles";
    case godot::Mesh::PRIMITIVE_TRIANGLE_STRIP:
      return "triangle_strip";
  }
  return "unknown";
}

bool mesh_parse_vec3_list(const mcp::JsonValue &value, const char *name,
                          godot::PackedVector3Array &out,
                          std::string &error) {
  if (!value.IsArray()) {
    error = mesh_invalid_param(name, "an array of {x, y, z} objects");
    return false;
  }
  out = godot::PackedVector3Array();
  for (size_t i = 0; i < value.Size(); ++i) {
    const std::string item_name =
        std::string(name) + "[" + std::to_string(i) + "]";
    godot::Vector3 vertex;
    if (!mesh_read_vec3(value[i], item_name.c_str(), vertex, error)) {
      return false;
    }
    out.append(vertex);
  }
  return true;
}

bool mesh_parse_vec2_list(const mcp::JsonValue &value, const char *name,
                          godot::PackedVector2Array &out,
                          std::string &error) {
  if (!value.IsArray()) {
    error = mesh_invalid_param(name, "an array of {x, y} objects");
    return false;
  }
  out = godot::PackedVector2Array();
  for (size_t i = 0; i < value.Size(); ++i) {
    const std::string item_name =
        std::string(name) + "[" + std::to_string(i) + "]";
    godot::Vector2 uv;
    if (!mesh_read_vec2(value[i], item_name.c_str(), uv, error)) {
      return false;
    }
    out.append(uv);
  }
  return true;
}

bool mesh_parse_color_list(const mcp::JsonValue &value, const char *name,
                           godot::PackedColorArray &out, std::string &error) {
  if (!value.IsArray()) {
    error = mesh_invalid_param(name, "an array of {r, g, b, a} objects");
    return false;
  }
  out = godot::PackedColorArray();
  for (size_t i = 0; i < value.Size(); ++i) {
    const std::string item_name =
        std::string(name) + "[" + std::to_string(i) + "]";
    godot::Color color;
    if (!mesh_read_color(value[i], item_name.c_str(), color, error)) {
      return false;
    }
    out.append(color);
  }
  return true;
}

bool mesh_parse_float_list(const mcp::JsonValue &value, const char *name,
                           godot::PackedFloat32Array &out, std::string &error) {
  if (!value.IsArray()) {
    error = mesh_invalid_param(name, "an array of numbers");
    return false;
  }
  const int64_t count = static_cast<int64_t>(value.Size());
  if (count > kMeshMaxBufferValues) {
    error = "invalid parameter: " + std::string(name) + " has " +
            std::to_string(count) + " values, above the " +
            std::to_string(kMeshMaxBufferValues) + " value limit";
    return false;
  }
  out = godot::PackedFloat32Array();
  out.resize(static_cast<int>(count));
  float *target = out.ptrw();
  for (int64_t i = 0; i < count; ++i) {
    double number = 0.0;
    if (!mesh_read_number(value[static_cast<size_t>(i)], name, number, error)) {
      return false;
    }
    target[i] = static_cast<float>(number);
  }
  return true;
}

bool mesh_resolve_resource_locator(const mcp::JsonValue &value,
                                   godot::Ref<godot::Resource> &out,
                                   std::string &error) {
  util::ObjectLocatorFields fields;
  std::string other_class;
  const std::string descriptor = mesh_describe_locator(value);
  if (value.IsString()) {
    bool out_of_range = false;
    util::parse_locator_string(value.GetString(), true, fields, out_of_range);
    if (out_of_range) {
      error = "invalid locator: object_id is out of range";
      return false;
    }
    if (fields.path.rfind("user://", 0) == 0) {
      fields.name = value.GetString();
      fields.path.clear();
    }
    if (fields.has_object_id) {
      fields.name = value.GetString();
    }
  } else if (value.IsObject()) {
    const char *bad_field = nullptr;
    const util::LocatorFieldStatus status =
        util::parse_object_locator(value, true, fields, &bad_field);
    if (status == util::LocatorFieldStatus::UnknownField) {
      error = "invalid locator: unknown field '" + std::string(bad_field) +
              "' (allowed: name, path, object_id, object_id_str)";
      return false;
    }
    if (status == util::LocatorFieldStatus::WrongType) {
      const std::string field = bad_field != nullptr ? bad_field : "locator";
      const char *expected = std::string(field) == "object_id"
                                 ? "a number"
                                 : (std::string(field) == "object_id_str"
                                        ? "a decimal integer string"
                                        : "a string");
      error = mesh_invalid_param(field.c_str(), expected);
      return false;
    }
    if (status == util::LocatorFieldStatus::OutOfRange) {
      error = mesh_invalid_param("object_id_str", "an in-range decimal integer");
      return false;
    }
  } else {
    error = "invalid locator: expected a string or object";
    return false;
  }
  const std::string &name = fields.name;
  const std::string &path = fields.path;
  const bool has_object_id = fields.has_object_id;
  const int64_t object_id = fields.object_id;

  if (has_object_id) {
    godot::Object *object =
        godot::ObjectDB::get_instance(static_cast<uint64_t>(object_id));
    if (object != nullptr) {
      auto *resource = godot::Object::cast_to<godot::Resource>(object);
      if (resource != nullptr) {
        out = godot::Ref<godot::Resource>(resource);
      } else {
        other_class = util::to_std(object->get_class());
      }
    }
  }
  if (out.is_null() && !name.empty()) {
    out = resource_registry::lookup_memory(name);
    if (out.is_valid()) {
      other_class.clear();
    }
  }
  if (out.is_null() && !path.empty()) {
    out = resource_registry::lookup_memory(path);
    if (out.is_null()) {
      std::string load_error;
      if (!resource_ops::load_resource_or_error(path, out, load_error)) {
        error = load_error;
        return false;
      }
      resource_registry::register_resource(out, path);
    }
  }
  if (out.is_null()) {
    if (!other_class.empty()) {
      error = "resource '" + descriptor + "' is " + other_class +
              ", not a Resource";
      return false;
    }
    error = "resource not found: " + descriptor +
            " — create or register it first, or pass a res:// path";
    return false;
  }
  return true;
}

bool mesh_resolve_required_mesh(const mcp::JsonValue &args,
                                const std::string &action, const char *key,
                                godot::Ref<godot::Mesh> &out,
                                std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    error = mesh_missing_param(action, key);
    return false;
  }
  const std::string descriptor = mesh_describe_locator(*value);
  godot::Ref<godot::Resource> resource;
  if (!mesh_resolve_resource_locator(*value, resource, error)) {
    return false;
  }
  godot::Ref<godot::Mesh> mesh = resource;
  if (mesh.is_null()) {
    error = "resource '" + descriptor + "' is " +
            util::to_std(resource->get_class()) +
            ", not a Mesh — pass a Mesh handle such as memory://name from "
            "mesh_surface action='commit' or resource_create action='array'";
    return false;
  }
  out = mesh;
  return true;
}

bool mesh_resolve_required_multimesh(const mcp::JsonValue &args,
                                     const std::string &action,
                                     godot::Ref<godot::MultiMesh> &out,
                                     std::string &error) {
  const mcp::JsonValue *value = args.Find("multimesh");
  if (value == nullptr) {
    error = mesh_missing_param(action, "multimesh");
    return false;
  }
  const std::string descriptor = mesh_describe_locator(*value);
  godot::Ref<godot::Resource> resource;
  if (!mesh_resolve_resource_locator(*value, resource, error)) {
    return false;
  }
  godot::Ref<godot::MultiMesh> multimesh = resource;
  if (multimesh.is_null()) {
    error = "resource '" + descriptor + "' is " +
            util::to_std(resource->get_class()) +
            ", not a MultiMesh — create one with multimesh action='create'";
    return false;
  }
  out = multimesh;
  return true;
}

bool mesh_resolve_required_material(const mcp::JsonValue &args,
                                    const std::string &action,
                                    const char *key,
                                    godot::Ref<godot::Material> &out,
                                    std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    error = mesh_missing_param(action, key);
    return false;
  }
  const std::string descriptor = mesh_describe_locator(*value);
  godot::Ref<godot::Resource> resource;
  if (!mesh_resolve_resource_locator(*value, resource, error)) {
    return false;
  }
  godot::Ref<godot::Material> material = resource;
  if (material.is_null()) {
    error = "resource '" + descriptor + "' is " +
            util::to_std(resource->get_class()) +
            ", not a Material — pass a res:// material path or a registered "
            "Material";
    return false;
  }
  out = material;
  return true;
}

bool mesh_parse_basis(const mcp::JsonValue &value, godot::Basis &out,
                      std::string &error) {
  if (value.IsArray()) {
    if (value.Size() != 3) {
      error = mesh_invalid_param("basis", "an array of 3 rows of 3 numbers");
      return false;
    }
    for (int row = 0; row < 3; ++row) {
      const mcp::JsonValue &entry = value[static_cast<size_t>(row)];
      if (!entry.IsArray() || entry.Size() != 3) {
        error = mesh_invalid_param("basis", "an array of 3 rows of 3 numbers");
        return false;
      }
      double numbers[3] = {0.0, 0.0, 0.0};
      for (int column = 0; column < 3; ++column) {
        if (!mesh_read_number(entry[static_cast<size_t>(column)], "basis",
                              numbers[column], error)) {
          return false;
        }
      }
      out.rows[row] = godot::Vector3(static_cast<float>(numbers[0]),
                                     static_cast<float>(numbers[1]),
                                     static_cast<float>(numbers[2]));
    }
    return true;
  }
  if (value.IsObject()) {
    for (const auto &entry : value.GetObject()) {
      if (entry.first != "x" && entry.first != "y" && entry.first != "z") {
        error = "invalid parameter: basis has unknown field '" + entry.first +
                "' (allowed: x, y, z)";
        return false;
      }
    }
    const char *axes[3] = {"x", "y", "z"};
    for (int row = 0; row < 3; ++row) {
      const mcp::JsonValue *axis = value.Find(axes[row]);
      if (axis == nullptr) {
        error = mesh_invalid_param("basis", "an object with x, y and z rows");
        return false;
      }
      godot::Vector3 row_value;
      const std::string row_name = std::string("basis.") + axes[row];
      if (!mesh_read_vec3(*axis, row_name.c_str(), row_value, error)) {
        return false;
      }
      out.rows[row] = row_value;
    }
    return true;
  }
  error = mesh_invalid_param("basis",
                             "an array of 3 rows or an object {x, y, z}");
  return false;
}

bool mesh_parse_transform3d(const mcp::JsonValue &value,
                            godot::Transform3D &out, std::string &error) {
  if (!value.IsObject()) {
    error = mesh_invalid_param("transform", "a transform object");
    return false;
  }
  for (const auto &entry : value.GetObject()) {
    if (entry.first != "origin" && entry.first != "rotation" &&
        entry.first != "scale" && entry.first != "basis") {
      error = "invalid parameter: transform has unknown field '" + entry.first +
              "' (allowed: origin, rotation, scale, basis)";
      return false;
    }
  }
  godot::Vector3 origin;
  const mcp::JsonValue *origin_p = value.Find("origin");
  if (origin_p != nullptr &&
      !mesh_read_vec3(*origin_p, "transform.origin", origin, error)) {
    return false;
  }
  godot::Basis basis;
  const mcp::JsonValue *basis_p = value.Find("basis");
  const mcp::JsonValue *rotation_p = value.Find("rotation");
  if (basis_p != nullptr && rotation_p != nullptr) {
    error = "invalid parameter: provide either basis or rotation, not both";
    return false;
  }
  if (basis_p != nullptr) {
    if (!mesh_parse_basis(*basis_p, basis, error)) {
      return false;
    }
  } else if (rotation_p != nullptr) {
    godot::Vector3 rotation;
    if (!mesh_read_vec3(*rotation_p, "transform.rotation", rotation, error)) {
      return false;
    }
    basis = godot::Basis::from_euler(
        rotation * static_cast<float>(kMeshDegToRad));
  }
  const mcp::JsonValue *scale_p = value.Find("scale");
  if (scale_p != nullptr) {
    godot::Vector3 scale;
    if (!mesh_read_vec3(*scale_p, "transform.scale", scale, error)) {
      return false;
    }
    basis = basis.scaled(scale);
  }
  out = godot::Transform3D(basis, origin);
  return true;
}

bool mesh_parse_transform2d(const mcp::JsonValue &value,
                            godot::Transform2D &out, std::string &error) {
  if (!value.IsObject()) {
    error = mesh_invalid_param("transform", "a transform object");
    return false;
  }
  for (const auto &entry : value.GetObject()) {
    if (entry.first != "origin" && entry.first != "rotation" &&
        entry.first != "scale") {
      error = "invalid parameter: transform has unknown field '" + entry.first +
              "' (allowed: origin, rotation, scale)";
      return false;
    }
  }
  godot::Vector2 origin;
  const mcp::JsonValue *origin_p = value.Find("origin");
  if (origin_p != nullptr &&
      !mesh_read_vec2(*origin_p, "transform.origin", origin, error)) {
    return false;
  }
  double rotation_degrees = 0.0;
  const mcp::JsonValue *rotation_p = value.Find("rotation");
  if (rotation_p != nullptr &&
      !mesh_read_number(*rotation_p, "transform.rotation", rotation_degrees,
                        error)) {
    return false;
  }
  godot::Vector2 scale(1.0f, 1.0f);
  const mcp::JsonValue *scale_p = value.Find("scale");
  if (scale_p != nullptr &&
      !mesh_read_vec2(*scale_p, "transform.scale", scale, error)) {
    return false;
  }
  out = godot::Transform2D(
      static_cast<float>(rotation_degrees * kMeshDegToRad), origin);
  out = out.scaled(scale);
  return true;
}

godot::Variant mesh_array_at(const godot::Array &arrays,
                             godot::Mesh::ArrayType type) {
  const int index = static_cast<int>(type);
  if (index < 0 || index >= arrays.size()) {
    return godot::Variant();
  }
  return arrays[index];
}

int64_t mesh_packed_count(const godot::Variant &value) {
  switch (value.get_type()) {
    case godot::Variant::PACKED_VECTOR3_ARRAY:
      return static_cast<int64_t>(
          static_cast<godot::PackedVector3Array>(value).size());
    case godot::Variant::PACKED_VECTOR2_ARRAY:
      return static_cast<int64_t>(
          static_cast<godot::PackedVector2Array>(value).size());
    case godot::Variant::PACKED_COLOR_ARRAY:
      return static_cast<int64_t>(
          static_cast<godot::PackedColorArray>(value).size());
    case godot::Variant::PACKED_FLOAT32_ARRAY:
      return static_cast<int64_t>(
          static_cast<godot::PackedFloat32Array>(value).size());
    case godot::Variant::PACKED_INT32_ARRAY:
      return static_cast<int64_t>(
          static_cast<godot::PackedInt32Array>(value).size());
    case godot::Variant::ARRAY:
      return static_cast<int64_t>(static_cast<godot::Array>(value).size());
    default:
      return 0;
  }
}

MeshSessionState *mesh_find_session(const mcp::JsonValue &args,
                                    std::string &resolved,
                                    std::string &error) {
  const mcp::JsonValue *value = args.Find("session");
  if (value == nullptr) {
    error = "missing required parameter: session";
    return nullptr;
  }
  util::ObjectLocatorFields fields;
  const std::string descriptor = mesh_describe_locator(*value);
  if (value->IsString()) {
    const std::string text = value->GetString();
    auto named = g_mesh_sessions.find(text);
    if (named != g_mesh_sessions.end()) {
      resolved = named->first;
      return &named->second;
    }
    bool out_of_range = false;
    util::parse_locator_string(text, true, fields, out_of_range);
    if (out_of_range) {
      error = "invalid session locator: object_id is out of range";
      return nullptr;
    }
    if (!fields.path.empty()) {
      fields.name = text;
      fields.path.clear();
    }
  } else if (value->IsObject()) {
    const char *bad_field = nullptr;
    const util::LocatorFieldStatus status =
        util::parse_object_locator(*value, false, fields, &bad_field);
    if (status == util::LocatorFieldStatus::UnknownField) {
      error = "invalid session locator: unknown field '" +
              std::string(bad_field) +
              "' (allowed: name, object_id, object_id_str)";
      return nullptr;
    }
    if (status == util::LocatorFieldStatus::WrongType) {
      const std::string field = bad_field != nullptr ? bad_field : "locator";
      const char *expected = std::string(field) == "object_id"
                                 ? "a number"
                                 : (std::string(field) == "object_id_str"
                                        ? "a decimal integer string"
                                        : "a string");
      error = mesh_invalid_param(("session." + field).c_str(), expected);
      return nullptr;
    }
    if (status == util::LocatorFieldStatus::OutOfRange) {
      error = mesh_invalid_param("session.object_id_str",
                                 "an in-range decimal integer");
      return nullptr;
    }
  } else {
    error = "invalid session locator: expected a string or object";
    return nullptr;
  }
  const std::string &name = fields.name;
  const bool has_object_id = fields.has_object_id;
  const int64_t object_id = fields.object_id;
  if (!name.empty()) {
    auto it = g_mesh_sessions.find(name);
    if (it != g_mesh_sessions.end()) {
      resolved = it->first;
      return &it->second;
    }
  }
  if (has_object_id) {
    for (auto &entry : g_mesh_sessions) {
      if (entry.second.tool->get_instance_id() ==
          static_cast<uint64_t>(object_id)) {
        resolved = entry.first;
        return &entry.second;
      }
    }
  }
  error = "surface session not found: " + descriptor +
          " — start one with mesh_surface action='surface_begin'";
  return nullptr;
}

mcp::JsonValue mesh_session_info(const MeshSessionState &state,
                                 const std::string &name) {
  const int64_t object_id =
      static_cast<int64_t>(state.tool->get_instance_id());
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["session"] = mcp::JsonValue(name);
  j["object_id"] = mcp::JsonValue(object_id);
  j["object_id_str"] = mcp::JsonValue(std::to_string(object_id));
  j["primitive"] =
      mcp::JsonValue(mesh_primitive_name_of(state.tool->get_primitive_type()));
  j["vertex_count"] = mcp::JsonValue(state.vertex_count);
  j["active_sessions"] =
      mcp::JsonValue(static_cast<int64_t>(g_mesh_sessions.size()));
  return j;
}

mcp::JsonValue mesh_multimesh_info(const godot::Ref<godot::MultiMesh> &multimesh,
                                   const std::string &name) {
  mcp::JsonValue j = mesh_resource_info(multimesh, name, "multimesh_ref");
  const int64_t stride =
      (multimesh->get_transform_format() == godot::MultiMesh::TRANSFORM_3D
           ? 12
           : 8) +
      (multimesh->is_using_colors() ? 4 : 0) +
      (multimesh->is_using_custom_data() ? 4 : 0);
  j["instance_count"] =
      mcp::JsonValue(static_cast<int64_t>(multimesh->get_instance_count()));
  j["visible_instance_count"] = mcp::JsonValue(
      static_cast<int64_t>(multimesh->get_visible_instance_count()));
  j["transform_format"] = mcp::JsonValue(
      multimesh->get_transform_format() == godot::MultiMesh::TRANSFORM_2D
          ? "2d"
          : "3d");
  j["use_colors"] = mcp::JsonValue(multimesh->is_using_colors());
  j["use_custom_data"] = mcp::JsonValue(multimesh->is_using_custom_data());
  const godot::Ref<godot::Mesh> mesh = multimesh->get_mesh();
  j["mesh"] = mesh.is_null() ? mcp::JsonValue()
                             : mesh_resource_info(mesh, "", "mesh_ref");
  j["aabb"] = mesh_aabb_to_json(multimesh->get_aabb());
  j["buffer_stride"] = mcp::JsonValue(stride);
  j["buffer_length"] = mcp::JsonValue(
      static_cast<int64_t>(multimesh->get_instance_count()) * stride);
  return j;
}

bool mesh_read_object_id_locator(const mcp::JsonValue &value, int64_t &out,
                                 std::string &descriptor, std::string &error) {
  if (value.IsString()) {
    const std::string text = value.GetString();
    if (!util::is_decimal_digits(text)) {
      error = mesh_invalid_param("node", "a decimal object_id string");
      return false;
    }
    if (!util::parse_decimal_int64(text, out)) {
      error = mesh_invalid_param("node", "an in-range decimal object_id string");
      return false;
    }
    descriptor = "object_id=" + text;
    return true;
  }
  if (!value.IsObject()) {
    error = mesh_invalid_param(
        "node", "a decimal object_id string or {object_id|object_id_str}");
    return false;
  }
  for (const auto &entry : value.GetObject()) {
    if (entry.first != "object_id" && entry.first != "object_id_str") {
      error = "invalid parameter: node has unknown field '" + entry.first +
              "' (allowed: object_id, object_id_str)";
      return false;
    }
  }
  const mcp::JsonValue *oid_p = value.Find("object_id");
  if (oid_p != nullptr) {
    if (!oid_p->IsNumber()) {
      error = mesh_invalid_param("node.object_id", "a number");
      return false;
    }
    out = oid_p->IsInt() ? oid_p->GetInt()
                         : static_cast<int64_t>(oid_p->GetDouble());
    descriptor = "object_id=" + std::to_string(out);
    return true;
  }
  const mcp::JsonValue *oid_str_p = value.Find("object_id_str");
  if (oid_str_p == nullptr || !oid_str_p->IsString() ||
      !util::is_decimal_digits(oid_str_p->GetString())) {
    error = mesh_invalid_param("node", "object_id or object_id_str");
    return false;
  }
  if (!util::parse_decimal_int64(oid_str_p->GetString(), out)) {
    error = mesh_invalid_param("node", "an in-range object_id_str");
    return false;
  }
  descriptor = "object_id=" + std::to_string(out);
  return true;
}

mcp::JsonValue mesh_handle_surface_begin(const mcp::JsonValue &args) {
  std::string error;
  godot::Mesh::PrimitiveType primitive = godot::Mesh::PRIMITIVE_TRIANGLES;
  std::string primitive_text;
  if (!mesh_read_optional_string(args, "primitive", primitive_text, error)) {
    return util::error_json(error);
  }
  if (!primitive_text.empty() &&
      !mesh_parse_primitive(primitive_text, primitive)) {
    return util::error_json(
        "invalid parameter: primitive must be points, lines, line_strip, "
        "triangles or triangle_strip");
  }
  std::string name;
  if (!mesh_read_optional_string(args, "name", name, error)) {
    return util::error_json(error);
  }
  if (name.rfind("memory://", 0) == 0) {
    name = name.substr(9);
  }
  if (name.empty()) {
    name = "surface_" + std::to_string(++g_mesh_session_sequence);
  }
  if (g_mesh_sessions.find(name) != g_mesh_sessions.end()) {
    return util::error_json(
        "session already exists: " + name +
        " — release it with mesh_inspect action='free' or commit it first");
  }
  if (static_cast<int64_t>(g_mesh_sessions.size()) >= kMeshMaxSessions) {
    return util::error_json(
        "too many active surface sessions (limit " +
        std::to_string(kMeshMaxSessions) +
        "); release one with mesh_inspect action='free'");
  }
  godot::Ref<godot::SurfaceTool> tool;
  tool.instantiate();
  if (tool.is_null()) {
    return util::error_json("failed to instantiate SurfaceTool");
  }
  tool->begin(primitive);
  MeshSessionState state;
  state.tool = tool;
  state.vertex_count = 0;
  g_mesh_sessions[name] = std::move(state);
  return util::ok_result(mesh_session_info(g_mesh_sessions[name], name));
}

mcp::JsonValue mesh_handle_add_vertex(const mcp::JsonValue &args) {
  const std::string action = "add_vertex";
  std::string error;
  std::string resolved;
  MeshSessionState *state = mesh_find_session(args, resolved, error);
  if (state == nullptr) {
    return util::error_json(error);
  }
  const mcp::JsonValue *single = args.Find("vertex");
  const mcp::JsonValue *many = args.Find("vertices");
  if (single != nullptr && many != nullptr) {
    return util::error_json(
        "invalid parameter: provide either vertex or vertices, not both");
  }
  if (single == nullptr && many == nullptr) {
    return util::error_json(mesh_missing_param(action, "vertex"));
  }
  int64_t added = 0;
  if (single != nullptr) {
    godot::Vector3 vertex;
    if (!mesh_read_vec3(*single, "vertex", vertex, error)) {
      return util::error_json(error);
    }
    if (state->vertex_count + 1 > kMeshMaxVertices) {
      return util::error_json(
          "vertex limit exceeded: a session holds at most " +
          std::to_string(kMeshMaxVertices) + " vertices");
    }
    state->tool->add_vertex(vertex);
    ++state->vertex_count;
    added = 1;
  } else {
    godot::PackedVector3Array vertices;
    if (!mesh_parse_vec3_list(*many, "vertices", vertices, error)) {
      return util::error_json(error);
    }
    if (vertices.is_empty()) {
      return util::error_json(
          "invalid parameter: vertices must contain at least 1 vertex");
    }
    if (state->vertex_count + vertices.size() > kMeshMaxVertices) {
      return util::error_json(
          "vertex limit exceeded: a session holds at most " +
          std::to_string(kMeshMaxVertices) + " vertices");
    }
    for (int i = 0; i < vertices.size(); ++i) {
      state->tool->add_vertex(vertices[i]);
    }
    state->vertex_count += vertices.size();
    added = vertices.size();
  }
  mcp::JsonValue result = mesh_session_info(*state, resolved);
  result["added"] = mcp::JsonValue(added);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_add_triangle_fan(const mcp::JsonValue &args) {
  const std::string action = "add_triangle_fan";
  std::string error;
  std::string resolved;
  MeshSessionState *state = mesh_find_session(args, resolved, error);
  if (state == nullptr) {
    return util::error_json(error);
  }
  const mcp::JsonValue *vertices_p = args.Find("vertices");
  if (vertices_p == nullptr) {
    return util::error_json(mesh_missing_param(action, "vertices"));
  }
  godot::PackedVector3Array vertices;
  if (!mesh_parse_vec3_list(*vertices_p, "vertices", vertices, error)) {
    return util::error_json(error);
  }
  if (vertices.size() < 3) {
    return util::error_json(
        "invalid parameter: vertices must contain at least 3 vertices");
  }
  if (state->vertex_count + vertices.size() > kMeshMaxVertices) {
    return util::error_json(
        "vertex limit exceeded: a session holds at most " +
        std::to_string(kMeshMaxVertices) + " vertices");
  }
  godot::PackedVector2Array uvs;
  godot::PackedColorArray colors;
  godot::PackedVector2Array uv2s;
  godot::PackedVector3Array normals;
  bool has_uvs = false;
  bool has_colors = false;
  bool has_uv2s = false;
  bool has_normals = false;
  const mcp::JsonValue *uvs_p = args.Find("uvs");
  if (uvs_p != nullptr) {
    if (!mesh_parse_vec2_list(*uvs_p, "uvs", uvs, error)) {
      return util::error_json(error);
    }
    has_uvs = true;
  }
  const mcp::JsonValue *colors_p = args.Find("colors");
  if (colors_p != nullptr) {
    if (!mesh_parse_color_list(*colors_p, "colors", colors, error)) {
      return util::error_json(error);
    }
    has_colors = true;
  }
  const mcp::JsonValue *uv2s_p = args.Find("uv2s");
  if (uv2s_p != nullptr) {
    if (!mesh_parse_vec2_list(*uv2s_p, "uv2s", uv2s, error)) {
      return util::error_json(error);
    }
    has_uv2s = true;
  }
  const mcp::JsonValue *normals_p = args.Find("normals");
  if (normals_p != nullptr) {
    if (!mesh_parse_vec3_list(*normals_p, "normals", normals, error)) {
      return util::error_json(error);
    }
    has_normals = true;
  }
  const int vertex_count = vertices.size();
  if ((has_uvs && uvs.size() != vertex_count) ||
      (has_colors && colors.size() != vertex_count) ||
      (has_uv2s && uv2s.size() != vertex_count) ||
      (has_normals && normals.size() != vertex_count)) {
    return util::error_json(
        "invalid parameter: uvs, colors, uv2s and normals must each match "
        "vertices in length");
  }
  state->tool->add_triangle_fan(vertices, uvs, colors, uv2s, normals,
                                godot::TypedArray<godot::Plane>());
  state->vertex_count += vertex_count;
  if (has_uvs) {
    state->has_uv = true;
  }
  if (has_normals) {
    state->has_normal = true;
  }
  if (has_colors) {
    state->has_color = true;
  }
  mcp::JsonValue result = mesh_session_info(*state, resolved);
  result["added"] = mcp::JsonValue(static_cast<int64_t>(vertex_count));
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_set_attribute(const mcp::JsonValue &args,
                                         const std::string &action,
                                         const char *key,
                                         bool MeshSessionState::*flag) {
  std::string error;
  std::string resolved;
  MeshSessionState *state = mesh_find_session(args, resolved, error);
  if (state == nullptr) {
    return util::error_json(error);
  }
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    return util::error_json(mesh_missing_param(action, key));
  }
  if (std::string(key) == "uv") {
    godot::Vector2 uv;
    if (!mesh_read_vec2(*value, key, uv, error)) {
      return util::error_json(error);
    }
    state->tool->set_uv(uv);
  } else if (std::string(key) == "normal") {
    godot::Vector3 normal;
    if (!mesh_read_vec3(*value, key, normal, error)) {
      return util::error_json(error);
    }
    state->tool->set_normal(normal);
  } else {
    godot::Color color;
    if (!mesh_read_color(*value, key, color, error)) {
      return util::error_json(error);
    }
    state->tool->set_color(color);
  }
  state->*flag = true;
  mcp::JsonValue result = mesh_session_info(*state, resolved);
  result["attribute"] = mcp::JsonValue(key);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_set_material(const mcp::JsonValue &args) {
  const std::string action = "set_material";
  std::string error;
  std::string resolved;
  MeshSessionState *state = mesh_find_session(args, resolved, error);
  if (state == nullptr) {
    return util::error_json(error);
  }
  godot::Ref<godot::Material> material;
  if (!mesh_resolve_required_material(args, action, "material", material,
                                      error)) {
    return util::error_json(error);
  }
  state->tool->set_material(material);
  mcp::JsonValue result = mesh_session_info(*state, resolved);
  result["material"] = mesh_resource_info(material, "", "material_ref");
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_commit(const mcp::JsonValue &args) {
  std::string error;
  std::string resolved;
  MeshSessionState *state = mesh_find_session(args, resolved, error);
  if (state == nullptr) {
    return util::error_json(error);
  }
  bool generate_normals = false;
  bool flip_normals = false;
  bool generate_tangents = false;
  bool do_index = false;
  bool do_deindex = false;
  if (!mesh_read_bool_or(args, "generate_normals", false, generate_normals,
                         error) ||
      !mesh_read_bool_or(args, "flip_normals", false, flip_normals, error) ||
      !mesh_read_bool_or(args, "generate_tangents", false, generate_tangents,
                         error) ||
      !mesh_read_bool_or(args, "index", false, do_index, error) ||
      !mesh_read_bool_or(args, "deindex", false, do_deindex, error)) {
    return util::error_json(error);
  }
  if (do_index && do_deindex) {
    return util::error_json(
        "invalid parameter: index and deindex are mutually exclusive");
  }
  if (flip_normals && !generate_normals) {
    return util::error_json(
        "invalid parameter: flip_normals requires generate_normals=true");
  }
  std::string name;
  std::string surface_name;
  if (!mesh_read_optional_string(args, "name", name, error) ||
      !mesh_read_optional_string(args, "surface_name", surface_name, error)) {
    return util::error_json(error);
  }
  godot::Ref<godot::ArrayMesh> existing;
  const mcp::JsonValue *mesh_p = args.Find("mesh");
  if (mesh_p != nullptr) {
    const std::string descriptor = mesh_describe_locator(*mesh_p);
    godot::Ref<godot::Resource> resource;
    if (!mesh_resolve_resource_locator(*mesh_p, resource, error)) {
      return util::error_json(error);
    }
    existing = resource;
    if (existing.is_null()) {
      return util::error_json("resource '" + descriptor + "' is " +
                              util::to_std(resource->get_class()) +
                              ", not an ArrayMesh — commit can only append to "
                              "an ArrayMesh");
    }
  }
  if (state->vertex_count == 0) {
    return util::error_json(
        "session '" + resolved +
        "' has no vertices; call add_vertex or add_triangle_fan first");
  }
  if (generate_tangents && !state->has_uv) {
    return util::error_json(
        "generate_tangents requires UVs; call set_uv before adding vertices, "
        "or pass uvs to add_triangle_fan");
  }
  if (generate_tangents && !state->has_normal && !generate_normals) {
    return util::error_json(
        "generate_tangents requires normals; call set_normal before adding "
        "vertices, pass normals to add_triangle_fan, or pass "
        "generate_normals=true");
  }
  if (generate_normals) {
    state->tool->generate_normals(flip_normals);
  }
  if (generate_tangents) {
    state->tool->generate_tangents();
  }
  if (do_index) {
    state->tool->index();
  }
  if (do_deindex) {
    state->tool->deindex();
  }
  godot::Ref<godot::ArrayMesh> committed = state->tool->commit(existing, 0);
  if (committed.is_null()) {
    return util::error_json(
        "failed to commit the SurfaceTool session to an ArrayMesh");
  }
  const int32_t surface_index = committed->get_surface_count() - 1;
  if (surface_index < 0) {
    return util::error_json("commit produced an ArrayMesh without surfaces");
  }
  if (!surface_name.empty()) {
    committed->surface_set_name(surface_index,
                                godot::String(surface_name.c_str()));
  }
  mesh_register_named_resource(committed, name);
  mcp::JsonValue result = mesh_resource_info(committed, name, "mesh_ref");
  result["session"] = mcp::JsonValue(resolved);
  result["session_released"] = mcp::JsonValue(true);
  result["session_vertex_count"] = mcp::JsonValue(state->vertex_count);
  result["surface_index"] = mcp::JsonValue(static_cast<int64_t>(surface_index));
  result["surface_count"] =
      mcp::JsonValue(static_cast<int64_t>(committed->get_surface_count()));
  result["vertex_count"] = mcp::JsonValue(
      static_cast<int64_t>(committed->surface_get_array_len(surface_index)));
  result["index_count"] = mcp::JsonValue(
      static_cast<int64_t>(committed->surface_get_array_index_len(surface_index)));
  const bool session_had_normals = state->has_normal;
  g_mesh_sessions.erase(resolved);
  if (!session_had_normals && !generate_normals) {
    result["warning"] = mcp::JsonValue(
        "no normals were set on this session; shaded materials may render "
        "flat — call set_normal before add_vertex or pass "
        "generate_normals=true");
  }
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_surface_action(const mcp::JsonValue &args) {
  const std::string action = mesh_action_of(args);
  if (action.empty()) {
    return util::error_json("missing required parameter: action");
  }
  if (action == "surface_begin") {
    return mesh_handle_surface_begin(args);
  }
  if (action == "add_vertex") {
    return mesh_handle_add_vertex(args);
  }
  if (action == "add_triangle_fan") {
    return mesh_handle_add_triangle_fan(args);
  }
  if (action == "set_uv") {
    return mesh_handle_set_attribute(args, action, "uv",
                                     &MeshSessionState::has_uv);
  }
  if (action == "set_normal") {
    return mesh_handle_set_attribute(args, action, "normal",
                                     &MeshSessionState::has_normal);
  }
  if (action == "set_color") {
    return mesh_handle_set_attribute(args, action, "color",
                                     &MeshSessionState::has_color);
  }
  if (action == "set_material") {
    return mesh_handle_set_material(args);
  }
  if (action == "commit") {
    return mesh_handle_commit(args);
  }
  return util::error_json("unknown action: " + action);
}

mcp::JsonValue mesh_handle_inspect(const mcp::JsonValue &args) {
  const std::string action = "inspect";
  std::string error;
  godot::Ref<godot::Mesh> mesh;
  if (!mesh_resolve_required_mesh(args, action, "mesh", mesh, error)) {
    return util::error_json(error);
  }
  mcp::JsonValue result = mesh_resource_info(mesh, "", "mesh_ref");
  const int32_t surface_count = mesh->get_surface_count();
  result["surface_count"] =
      mcp::JsonValue(static_cast<int64_t>(surface_count));
  result["aabb"] = mesh_aabb_to_json(mesh->get_aabb());
  auto *array_mesh = godot::Object::cast_to<godot::ArrayMesh>(mesh.ptr());
  if (array_mesh != nullptr) {
    result["blend_shape_count"] = mcp::JsonValue(
        static_cast<int64_t>(array_mesh->get_blend_shape_count()));
  }
  mcp::JsonValue surfaces(mcp::JsonValue::array_tag);
  for (int32_t index = 0; index < surface_count; ++index) {
    godot::Array arrays = mesh->surface_get_arrays(index);
    const uint32_t format =
        array_mesh != nullptr
            ? static_cast<uint32_t>(array_mesh->surface_get_format(index))
            : 0;
    const godot::Variant vertex_array =
        mesh_array_at(arrays, godot::Mesh::ARRAY_VERTEX);
    const godot::Variant normal_array =
        mesh_array_at(arrays, godot::Mesh::ARRAY_NORMAL);
    const godot::Variant tangent_array =
        mesh_array_at(arrays, godot::Mesh::ARRAY_TANGENT);
    const godot::Variant color_array =
        mesh_array_at(arrays, godot::Mesh::ARRAY_COLOR);
    const godot::Variant uv_array =
        mesh_array_at(arrays, godot::Mesh::ARRAY_TEX_UV);
    const godot::Variant uv2_array =
        mesh_array_at(arrays, godot::Mesh::ARRAY_TEX_UV2);
    const godot::Variant index_array =
        mesh_array_at(arrays, godot::Mesh::ARRAY_INDEX);
    const bool has_vertex = vertex_array.get_type() != godot::Variant::NIL;
    const bool has_normal =
        (format & static_cast<uint32_t>(godot::Mesh::ARRAY_FORMAT_NORMAL)) !=
            0 ||
        normal_array.get_type() != godot::Variant::NIL;
    const bool has_tangent =
        (format & static_cast<uint32_t>(godot::Mesh::ARRAY_FORMAT_TANGENT)) !=
            0 ||
        tangent_array.get_type() != godot::Variant::NIL;
    const bool has_color =
        (format & static_cast<uint32_t>(godot::Mesh::ARRAY_FORMAT_COLOR)) != 0 ||
        color_array.get_type() != godot::Variant::NIL;
    const bool has_uv =
        (format & static_cast<uint32_t>(godot::Mesh::ARRAY_FORMAT_TEX_UV)) !=
            0 ||
        uv_array.get_type() != godot::Variant::NIL;
    const bool has_uv2 =
        (format & static_cast<uint32_t>(godot::Mesh::ARRAY_FORMAT_TEX_UV2)) !=
            0 ||
        uv2_array.get_type() != godot::Variant::NIL;
    mcp::JsonValue entry(mcp::JsonValue::object_tag);
    entry["index"] = mcp::JsonValue(static_cast<int64_t>(index));
    entry["format"] = mcp::JsonValue(static_cast<int64_t>(format));
    entry["vertex_count"] =
        mcp::JsonValue(mesh_packed_count(vertex_array));
    entry["index_count"] = mcp::JsonValue(mesh_packed_count(index_array));
    entry["has_vertex"] = mcp::JsonValue(has_vertex);
    entry["has_normals"] = mcp::JsonValue(has_normal);
    entry["has_tangents"] = mcp::JsonValue(has_tangent);
    entry["has_colors"] = mcp::JsonValue(has_color);
    entry["has_uv"] = mcp::JsonValue(has_uv);
    entry["has_uv2"] = mcp::JsonValue(has_uv2);
    if (array_mesh != nullptr) {
      entry["name"] = mcp::JsonValue(
          util::to_std(array_mesh->surface_get_name(index)));
      entry["primitive"] = mcp::JsonValue(mesh_primitive_name_of(
          array_mesh->surface_get_primitive_type(index)));
      entry["array_len"] = mcp::JsonValue(
          static_cast<int64_t>(array_mesh->surface_get_array_len(index)));
      entry["index_array_len"] = mcp::JsonValue(
          static_cast<int64_t>(array_mesh->surface_get_array_index_len(index)));
    } else {
      entry["name"] = mcp::JsonValue("");
      entry["primitive"] = mcp::JsonValue("unknown");
    }
    godot::Ref<godot::Material> material = mesh->surface_get_material(index);
    entry["material"] = material.is_null()
                            ? mcp::JsonValue()
                            : mesh_resource_info(material, "", "material_ref");
    surfaces.PushBack(std::move(entry));
  }
  result["surfaces"] = std::move(surfaces);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_surface_arrays(const mcp::JsonValue &args) {
  const std::string action = "surface_arrays";
  std::string error;
  godot::Ref<godot::Mesh> mesh;
  if (!mesh_resolve_required_mesh(args, action, "mesh", mesh, error)) {
    return util::error_json(error);
  }
  int64_t surface_index = 0;
  if (!mesh_read_int_or(args, "surface_index", 0, surface_index, error)) {
    return util::error_json(error);
  }
  const int32_t surface_count = mesh->get_surface_count();
  if (surface_count == 0) {
    return util::error_json("mesh has no surfaces");
  }
  if (surface_index < 0 || surface_index >= surface_count) {
    return util::error_json(
        "invalid parameter: surface_index " + std::to_string(surface_index) +
        " is out of range (mesh has " + std::to_string(surface_count) +
        " surface(s))");
  }
  const int index = static_cast<int>(surface_index);
  godot::Array arrays = mesh->surface_get_arrays(index);
  mcp::JsonValue result = mesh_resource_info(mesh, "", "mesh_ref");
  result["surface_index"] = mcp::JsonValue(surface_index);
  auto *array_mesh = godot::Object::cast_to<godot::ArrayMesh>(mesh.ptr());
  if (array_mesh != nullptr) {
    result["surface_name"] =
        mcp::JsonValue(util::to_std(array_mesh->surface_get_name(index)));
    result["primitive"] = mcp::JsonValue(
        mesh_primitive_name_of(array_mesh->surface_get_primitive_type(index)));
  } else {
    result["surface_name"] = mcp::JsonValue("");
    result["primitive"] = mcp::JsonValue("unknown");
  }
  result["format"] = mcp::JsonValue(static_cast<int64_t>(
      array_mesh != nullptr
          ? static_cast<uint32_t>(array_mesh->surface_get_format(index))
          : 0u));
  mcp::JsonValue arrays_json(mcp::JsonValue::object_tag);
  mcp::JsonValue counts(mcp::JsonValue::object_tag);
  mcp::JsonValue present(mcp::JsonValue::array_tag);
  bool truncated = false;
  int64_t vertex_count = 0;
  int64_t index_count = 0;
  for (const MeshArraySlot &slot : kMeshArraySlots) {
    const godot::Variant value = mesh_array_at(arrays, slot.type);
    if (value.get_type() == godot::Variant::NIL) {
      continue;
    }
    const int64_t count = mesh_packed_count(value);
    if (count > static_cast<int64_t>(GDA_VARIANT_MAX_ARRAY_ELEMENTS)) {
      truncated = true;
    }
    if (slot.type == godot::Mesh::ARRAY_VERTEX) {
      vertex_count = count;
    } else if (slot.type == godot::Mesh::ARRAY_INDEX) {
      index_count = count;
    }
    arrays_json[slot.key] = VariantJson::serialize(value);
    counts[slot.key] = mcp::JsonValue(count);
    present.PushBack(mcp::JsonValue(slot.key));
  }
  result["vertex_count"] = mcp::JsonValue(vertex_count);
  result["index_count"] = mcp::JsonValue(index_count);
  result["arrays"] = std::move(arrays_json);
  result["counts"] = std::move(counts);
  result["array_types"] = std::move(present);
  result["truncated"] = mcp::JsonValue(truncated);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_faces(const mcp::JsonValue &args) {
  const std::string action = "faces";
  std::string error;
  godot::Ref<godot::Mesh> mesh;
  if (!mesh_resolve_required_mesh(args, action, "mesh", mesh, error)) {
    return util::error_json(error);
  }
  const godot::PackedVector3Array faces = mesh->get_faces();
  const int64_t count = static_cast<int64_t>(faces.size());
  mcp::JsonValue result = mesh_resource_info(mesh, "", "mesh_ref");
  result["face_count"] = mcp::JsonValue(count / 3);
  result["vertex_count"] = mcp::JsonValue(count);
  result["faces"] =
      VariantJson::serialize(godot::Variant(faces));
  result["truncated"] = mcp::JsonValue(
      count > static_cast<int64_t>(GDA_VARIANT_MAX_ARRAY_ELEMENTS));
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_create_trimesh_shape(const mcp::JsonValue &args) {
  const std::string action = "create_trimesh_shape";
  std::string error;
  godot::Ref<godot::Mesh> mesh;
  if (!mesh_resolve_required_mesh(args, action, "mesh", mesh, error)) {
    return util::error_json(error);
  }
  std::string name;
  if (!mesh_read_optional_string(args, "name", name, error)) {
    return util::error_json(error);
  }
  godot::Ref<godot::ConcavePolygonShape3D> shape =
      mesh->create_trimesh_shape();
  if (shape.is_null()) {
    return util::error_json(
        "failed to derive a trimesh shape from the mesh");
  }
  const int64_t face_count =
      static_cast<int64_t>(shape->get_faces().size()) / 3;
  if (face_count == 0) {
    return util::error_json(
        "derived trimesh shape is empty; the mesh has no triangle faces");
  }
  mesh_register_named_resource(shape, name);
  mcp::JsonValue result = mesh_resource_info(mesh, "", "mesh_ref");
  result["shape"] = mesh_resource_info(shape, name, "shape_ref");
  result["face_count"] = mcp::JsonValue(face_count);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_create_convex_shape(const mcp::JsonValue &args) {
  const std::string action = "create_convex_shape";
  std::string error;
  godot::Ref<godot::Mesh> mesh;
  if (!mesh_resolve_required_mesh(args, action, "mesh", mesh, error)) {
    return util::error_json(error);
  }
  bool clean = true;
  bool simplify = false;
  if (!mesh_read_bool_or(args, "clean", true, clean, error) ||
      !mesh_read_bool_or(args, "simplify", false, simplify, error)) {
    return util::error_json(error);
  }
  std::string name;
  if (!mesh_read_optional_string(args, "name", name, error)) {
    return util::error_json(error);
  }
  godot::Ref<godot::ConvexPolygonShape3D> shape =
      mesh->create_convex_shape(clean, simplify);
  if (shape.is_null()) {
    return util::error_json(
        "failed to derive a convex shape from the mesh");
  }
  const int64_t point_count =
      static_cast<int64_t>(shape->get_points().size());
  if (point_count == 0) {
    return util::error_json(
        "derived convex shape is empty; the mesh has no usable vertices");
  }
  mesh_register_named_resource(shape, name);
  mcp::JsonValue result = mesh_resource_info(mesh, "", "mesh_ref");
  result["shape"] = mesh_resource_info(shape, name, "shape_ref");
  result["point_count"] = mcp::JsonValue(point_count);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_csg_bake(const mcp::JsonValue &args) {
  const std::string action = "csg_bake";
  std::string error;
  std::string shape_text = "both";
  if (!mesh_read_optional_string(args, "shape", shape_text, error)) {
    return util::error_json(error);
  }
  const std::string shape_mode = util::ascii_lowercase(shape_text);
  if (shape_mode != "mesh" && shape_mode != "collision" &&
      shape_mode != "both") {
    return util::error_json(
        "invalid parameter: shape must be mesh, collision or both");
  }
  std::string name;
  std::string shape_name;
  if (!mesh_read_optional_string(args, "name", name, error) ||
      !mesh_read_optional_string(args, "shape_name", shape_name, error)) {
    return util::error_json(error);
  }
  const mcp::JsonValue *node_path_p = args.Find("node_path");
  const mcp::JsonValue *node_p = args.Find("node");
  if (node_path_p != nullptr && node_p != nullptr) {
    return util::error_json(
        "invalid parameter: provide exactly one of node_path and node");
  }
  if (node_path_p == nullptr && node_p == nullptr) {
    return util::error_json(mesh_missing_param(action, "node_path"));
  }
  godot::Node *node = nullptr;
  std::string node_descriptor;
  if (node_path_p != nullptr) {
    if (!node_path_p->IsString() || node_path_p->GetString().empty()) {
      return util::error_json(
          mesh_invalid_param("node_path", "a non-empty string"));
    }
    node_descriptor = node_path_p->GetString();
    auto *editor = godot::EditorInterface::get_singleton();
    if (editor == nullptr) {
      return util::error_json("EditorInterface not available");
    }
    godot::Node *scene_root = editor->get_edited_scene_root();
    if (scene_root == nullptr) {
      return util::error_json(
          "no edited scene is open: open a scene before baking CSG");
    }
    std::string path_hint;
    node = util::resolve_scene_node(node_descriptor, scene_root, &path_hint);
    if (node == nullptr) {
      std::string detail = "node not found: " + node_descriptor;
      if (!path_hint.empty()) {
        detail += " (" + path_hint + ")";
      }
      return util::error_json(detail);
    }
  } else {
    int64_t object_id = 0;
    if (!mesh_read_object_id_locator(*node_p, object_id, node_descriptor,
                                     error)) {
      return util::error_json(error);
    }
    godot::Object *object =
        godot::ObjectDB::get_instance(static_cast<uint64_t>(object_id));
    if (object == nullptr) {
      return util::error_json("node not found: " + node_descriptor);
    }
    node = godot::Object::cast_to<godot::Node>(object);
    if (node == nullptr) {
      return util::error_json("object is not a Node in " + node_descriptor);
    }
  }
  auto *csg = godot::Object::cast_to<godot::CSGShape3D>(node);
  if (csg == nullptr) {
    return util::error_json(
        "node is not a CSGShape3D: " + node_descriptor + " (actual class: " +
        util::to_std(node->get_class()) + ")");
  }
  if (!csg->is_root_shape()) {
    return util::error_json(
        "node is not the root CSG shape: " + node_descriptor +
        " — bake the top CSG shape of the tree");
  }
  godot::Ref<godot::ArrayMesh> baked_mesh;
  godot::Ref<godot::ConcavePolygonShape3D> baked_collision;
  if (shape_mode == "mesh" || shape_mode == "both") {
    baked_mesh = csg->bake_static_mesh();
    if (baked_mesh.is_null() || baked_mesh->get_surface_count() == 0) {
      return util::error_json(
          "CSG bake produced no static mesh; the CSG tree may be empty");
    }
  }
  if (shape_mode == "collision" || shape_mode == "both") {
    baked_collision = csg->bake_collision_shape();
    if (baked_collision.is_null() ||
        baked_collision->get_faces().size() == 0) {
      return util::error_json(
          "CSG bake produced no collision shape; the CSG tree may be empty");
    }
  }
  std::string mesh_name = name;
  if (mesh_name.empty()) {
    const std::string node_name = util::to_std(csg->get_name());
    mesh_name = (node_name.empty() ? std::string("csg") : node_name) + "_baked";
  }
  std::string collision_name = shape_name;
  if (collision_name.empty()) {
    collision_name = mesh_name + "_shape";
  }
  mcp::JsonValue node_info(mcp::JsonValue::object_tag);
  node_info["class"] = mcp::JsonValue(util::to_std(csg->get_class()));
  node_info["name"] = mcp::JsonValue(util::to_std(csg->get_name()));
  node_info["object_id"] =
      mcp::JsonValue(static_cast<int64_t>(csg->get_instance_id()));
  node_info["object_id_str"] =
      mcp::JsonValue(std::to_string(static_cast<int64_t>(csg->get_instance_id())));
  node_info["bake_source"] = mcp::JsonValue(node_descriptor);
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["node"] = std::move(node_info);
  result["shape_mode"] = mcp::JsonValue(shape_mode);
  if (!baked_mesh.is_null()) {
    mesh_register_named_resource(baked_mesh, mesh_name);
    result["mesh"] = mesh_resource_info(baked_mesh, mesh_name, "mesh_ref");
  }
  if (!baked_collision.is_null()) {
    mesh_register_named_resource(baked_collision, collision_name);
    result["collision"] =
        mesh_resource_info(baked_collision, collision_name, "shape_ref");
  }
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_free_session(const mcp::JsonValue &args) {
  std::string error;
  std::string resolved;
  if (mesh_find_session(args, resolved, error) == nullptr) {
    return util::error_json(error);
  }
  g_mesh_sessions.erase(resolved);
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["session"] = mcp::JsonValue(resolved);
  result["released"] = mcp::JsonValue(true);
  result["active_sessions"] =
      mcp::JsonValue(static_cast<int64_t>(g_mesh_sessions.size()));
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_inspect_action(const mcp::JsonValue &args) {
  const std::string action = mesh_action_of(args);
  if (action.empty()) {
    return util::error_json("missing required parameter: action");
  }
  if (action == "inspect") {
    return mesh_handle_inspect(args);
  }
  if (action == "surface_arrays") {
    return mesh_handle_surface_arrays(args);
  }
  if (action == "faces") {
    return mesh_handle_faces(args);
  }
  if (action == "create_trimesh_shape") {
    return mesh_handle_create_trimesh_shape(args);
  }
  if (action == "create_convex_shape") {
    return mesh_handle_create_convex_shape(args);
  }
  if (action == "csg_bake") {
    return mesh_handle_csg_bake(args);
  }
  if (action == "free") {
    return mesh_handle_free_session(args);
  }
  return util::error_json("unknown action: " + action);
}

mcp::JsonValue mesh_handle_multimesh_create(const mcp::JsonValue &args) {
  std::string error;
  std::string transform_format = "3d";
  if (!mesh_read_optional_string(args, "transform_format", transform_format,
                                 error)) {
    return util::error_json(error);
  }
  const std::string format_key = util::ascii_lowercase(transform_format);
  if (format_key != "3d" && format_key != "2d") {
    return util::error_json(
        "invalid parameter: transform_format must be 3d or 2d");
  }
  bool use_colors = false;
  bool use_custom_data = false;
  int64_t instance_count = 0;
  if (!mesh_read_bool_or(args, "use_colors", false, use_colors, error) ||
      !mesh_read_bool_or(args, "use_custom_data", false, use_custom_data,
                         error) ||
      !mesh_read_int_or(args, "instance_count", 0, instance_count, error)) {
    return util::error_json(error);
  }
  if (instance_count < 0 || instance_count > kMeshMaxInstances) {
    return util::error_json(
        "invalid parameter: instance_count must be between 0 and " +
        std::to_string(kMeshMaxInstances));
  }
  godot::Ref<godot::Mesh> mesh;
  const mcp::JsonValue *mesh_p = args.Find("mesh");
  if (mesh_p != nullptr) {
    if (!mesh_resolve_required_mesh(args, "create", "mesh", mesh, error)) {
      return util::error_json(error);
    }
  }
  godot::Ref<godot::MultiMesh> multimesh;
  multimesh.instantiate();
  if (multimesh.is_null()) {
    return util::error_json("failed to instantiate MultiMesh");
  }
  multimesh->set_transform_format(format_key == "2d"
                                      ? godot::MultiMesh::TRANSFORM_2D
                                      : godot::MultiMesh::TRANSFORM_3D);
  multimesh->set_use_colors(use_colors);
  multimesh->set_use_custom_data(use_custom_data);
  multimesh->set_instance_count(static_cast<int32_t>(instance_count));
  if (!mesh.is_null()) {
    multimesh->set_mesh(mesh);
  }
  std::string name;
  if (!mesh_read_optional_string(args, "name", name, error)) {
    return util::error_json(error);
  }
  mesh_register_named_resource(multimesh, name);
  return util::ok_result(mesh_multimesh_info(multimesh, name));
}

mcp::JsonValue mesh_handle_multimesh_set_transforms(
    const mcp::JsonValue &args) {
  const std::string action = "set_transforms";
  std::string error;
  godot::Ref<godot::MultiMesh> multimesh;
  if (!mesh_resolve_required_multimesh(args, action, multimesh, error)) {
    return util::error_json(error);
  }
  const mcp::JsonValue *transforms_p = args.Find("transforms");
  if (transforms_p == nullptr) {
    return util::error_json(mesh_missing_param(action, "transforms"));
  }
  if (!transforms_p->IsArray() || transforms_p->Size() == 0) {
    return util::error_json(
        "invalid parameter: transforms must be a non-empty array");
  }
  const int64_t count = static_cast<int64_t>(transforms_p->Size());
  const int64_t instance_count =
      static_cast<int64_t>(multimesh->get_instance_count());
  if (instance_count > kMeshMaxInstances) {
    return util::error_json(
        "MultiMesh instance_count exceeds the " +
        std::to_string(kMeshMaxInstances) + " instance limit");
  }
  if (count > instance_count) {
    return util::error_json(
        "invalid parameter: transforms has " + std::to_string(count) +
        " entries but instance_count is " + std::to_string(instance_count));
  }
  const bool is_3d =
      multimesh->get_transform_format() == godot::MultiMesh::TRANSFORM_3D;
  if (is_3d) {
    std::vector<godot::Transform3D> parsed;
    parsed.reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; ++i) {
      godot::Transform3D transform;
      if (!mesh_parse_transform3d((*transforms_p)[static_cast<size_t>(i)],
                                  transform, error)) {
        return util::error_json(error);
      }
      parsed.push_back(transform);
    }
    for (int64_t i = 0; i < count; ++i) {
      multimesh->set_instance_transform(
          static_cast<int32_t>(i), parsed[static_cast<size_t>(i)]);
    }
  } else {
    std::vector<godot::Transform2D> parsed;
    parsed.reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; ++i) {
      godot::Transform2D transform;
      if (!mesh_parse_transform2d((*transforms_p)[static_cast<size_t>(i)],
                                  transform, error)) {
        return util::error_json(error);
      }
      parsed.push_back(transform);
    }
    for (int64_t i = 0; i < count; ++i) {
      multimesh->set_instance_transform_2d(
          static_cast<int32_t>(i), parsed[static_cast<size_t>(i)]);
    }
  }
  mcp::JsonValue result = mesh_multimesh_info(multimesh, "");
  result["applied"] = mcp::JsonValue(count);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_multimesh_set_colors(
    const mcp::JsonValue &args) {
  const std::string action = "set_colors";
  std::string error;
  godot::Ref<godot::MultiMesh> multimesh;
  if (!mesh_resolve_required_multimesh(args, action, multimesh, error)) {
    return util::error_json(error);
  }
  if (!multimesh->is_using_colors()) {
    return util::error_json(
        "this MultiMesh does not use colors; recreate it with "
        "multimesh action='create' and use_colors=true");
  }
  const mcp::JsonValue *colors_p = args.Find("colors");
  if (colors_p == nullptr) {
    return util::error_json(mesh_missing_param(action, "colors"));
  }
  if (!colors_p->IsArray() || colors_p->Size() == 0) {
    return util::error_json(
        "invalid parameter: colors must be a non-empty array");
  }
  const int64_t count = static_cast<int64_t>(colors_p->Size());
  const int64_t instance_count =
      static_cast<int64_t>(multimesh->get_instance_count());
  if (instance_count > kMeshMaxInstances) {
    return util::error_json(
        "MultiMesh instance_count exceeds the " +
        std::to_string(kMeshMaxInstances) + " instance limit");
  }
  if (count > instance_count) {
    return util::error_json(
        "invalid parameter: colors has " + std::to_string(count) +
        " entries but instance_count is " + std::to_string(instance_count));
  }
  godot::PackedColorArray colors;
  if (!mesh_parse_color_list(*colors_p, "colors", colors, error)) {
    return util::error_json(error);
  }
  for (int64_t i = 0; i < count; ++i) {
    multimesh->set_instance_color(static_cast<int32_t>(i),
                                  colors[static_cast<int>(i)]);
  }
  mcp::JsonValue result = mesh_multimesh_info(multimesh, "");
  result["applied"] = mcp::JsonValue(count);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_multimesh_set_buffer(
    const mcp::JsonValue &args) {
  const std::string action = "set_buffer";
  std::string error;
  godot::Ref<godot::MultiMesh> multimesh;
  if (!mesh_resolve_required_multimesh(args, action, multimesh, error)) {
    return util::error_json(error);
  }
  const mcp::JsonValue *buffer_p = args.Find("buffer");
  if (buffer_p == nullptr) {
    return util::error_json(mesh_missing_param(action, "buffer"));
  }
  if (!buffer_p->IsArray()) {
    return util::error_json(
        mesh_invalid_param("buffer", "an array of numbers"));
  }
  const int64_t stride =
      (multimesh->get_transform_format() == godot::MultiMesh::TRANSFORM_3D
           ? 12
           : 8) +
      (multimesh->is_using_colors() ? 4 : 0) +
      (multimesh->is_using_custom_data() ? 4 : 0);
  const int64_t instance_count =
      static_cast<int64_t>(multimesh->get_instance_count());
  if (instance_count > kMeshMaxInstances) {
    return util::error_json(
        "MultiMesh instance_count exceeds the " +
        std::to_string(kMeshMaxInstances) + " instance limit");
  }
  const int64_t expected = instance_count * stride;
  const int64_t provided = static_cast<int64_t>(buffer_p->Size());
  if (provided != expected) {
    return util::error_json(
        "invalid parameter: buffer has " + std::to_string(provided) +
        " values but instance_count x stride is " + std::to_string(expected) +
        " (" + std::to_string(instance_count) + " x " +
        std::to_string(stride) + ")");
  }
  godot::PackedFloat32Array buffer;
  if (!mesh_parse_float_list(*buffer_p, "buffer", buffer, error)) {
    return util::error_json(error);
  }
  multimesh->set_buffer(buffer);
  mcp::JsonValue result = mesh_multimesh_info(multimesh, "");
  result["buffer_length"] = mcp::JsonValue(provided);
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_multimesh_info(const mcp::JsonValue &args) {
  const std::string action = "info";
  std::string error;
  godot::Ref<godot::MultiMesh> multimesh;
  if (!mesh_resolve_required_multimesh(args, action, multimesh, error)) {
    return util::error_json(error);
  }
  bool include_buffer = false;
  if (!mesh_read_bool_or(args, "include_buffer", false, include_buffer,
                         error)) {
    return util::error_json(error);
  }
  mcp::JsonValue result = mesh_multimesh_info(multimesh, "");
  if (include_buffer) {
    const godot::PackedFloat32Array buffer = multimesh->get_buffer();
    const int64_t length = static_cast<int64_t>(buffer.size());
    result["buffer"] = VariantJson::serialize(godot::Variant(buffer));
    result["buffer_length"] = mcp::JsonValue(length);
    result["buffer_truncated"] = mcp::JsonValue(
        length > static_cast<int64_t>(GDA_VARIANT_MAX_ARRAY_ELEMENTS));
  }
  return util::ok_result(std::move(result));
}

mcp::JsonValue mesh_handle_multimesh_action(const mcp::JsonValue &args) {
  const std::string action = mesh_action_of(args);
  if (action.empty()) {
    return util::error_json("missing required parameter: action");
  }
  if (action == "create") {
    return mesh_handle_multimesh_create(args);
  }
  if (action == "set_transforms") {
    return mesh_handle_multimesh_set_transforms(args);
  }
  if (action == "set_colors") {
    return mesh_handle_multimesh_set_colors(args);
  }
  if (action == "set_buffer") {
    return mesh_handle_multimesh_set_buffer(args);
  }
  if (action == "info") {
    return mesh_handle_multimesh_info(args);
  }
  return util::error_json("unknown action: " + action);
}

mcp::JsonValue mesh_handle_multimesh_instance_action(
    const mcp::JsonValue &args) {
  const std::string action = mesh_action_of(args);
  if (action.empty()) {
    return util::error_json("missing required parameter: action");
  }
  if (action != "assign_to_instance") {
    return util::error_json("unknown action: " + action);
  }
  std::string error;
  std::string node_path;
  if (!mesh_read_required_string(args, "node_path", action, node_path,
                                 error)) {
    return util::error_json(error);
  }
  godot::Ref<godot::MultiMesh> multimesh;
  if (!mesh_resolve_required_multimesh(args, action, multimesh, error)) {
    return util::error_json(error);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor == nullptr) {
    return util::error_json("EditorInterface not available");
  }
  godot::Node *scene_root = editor->get_edited_scene_root();
  if (scene_root == nullptr) {
    return util::error_json(
        "no edited scene is open: open a scene before assigning a MultiMesh");
  }
  std::string path_hint;
  godot::Node *node = util::resolve_scene_node(node_path, scene_root,
                                               &path_hint);
  if (node == nullptr) {
    std::string detail = "node not found: " + node_path;
    if (!path_hint.empty()) {
      detail += " (" + path_hint + ")";
    }
    return util::error_json(detail);
  }
  auto *instance = godot::Object::cast_to<godot::MultiMeshInstance3D>(node);
  if (instance == nullptr) {
    return util::error_json(
        "node is not a MultiMeshInstance3D: " + node_path +
        " (actual class: " + util::to_std(node->get_class()) + ")");
  }
  instance->set_multimesh(multimesh);
  const godot::Ref<godot::MultiMesh> back = instance->get_multimesh();
  if (back.is_null() || back.ptr() != multimesh.ptr()) {
    return util::error_json(
        "failed to assign the MultiMesh to " + node_path);
  }
  const std::string effective = util::to_std(multimesh->get_name());
  const int64_t object_id =
      static_cast<int64_t>(multimesh->get_instance_id());
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["assigned"] = mcp::JsonValue(true);
  result["node_path"] = mcp::JsonValue(node_path);
  result["class"] = mcp::JsonValue(util::to_std(node->get_class()));
  result["multimesh_ref"] = mcp::JsonValue(
      effective.empty() ? std::to_string(object_id) : "memory://" + effective);
  result["instance_count"] =
      mcp::JsonValue(static_cast<int64_t>(multimesh->get_instance_count()));
  return util::ok_result(std::move(result));
}

} // namespace

mcp::JsonValue handle_surface_action(const mcp::JsonValue &args) {
  return mesh_handle_surface_action(args);
}

mcp::JsonValue handle_inspect_action(const mcp::JsonValue &args) {
  return mesh_handle_inspect_action(args);
}

mcp::JsonValue handle_multimesh_action(const mcp::JsonValue &args) {
  return mesh_handle_multimesh_action(args);
}

mcp::JsonValue handle_multimesh_instance_action(const mcp::JsonValue &args) {
  return mesh_handle_multimesh_instance_action(args);
}

} // namespace mesh_ops
} // namespace godot_autopilot
