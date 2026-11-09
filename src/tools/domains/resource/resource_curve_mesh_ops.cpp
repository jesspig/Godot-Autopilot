#include "tools/domains/resource/resource_ops.hpp"
#include "tools/domains/editor/editor_ops.hpp"
#include "core/config.hpp"
#include "core/editor_readiness.hpp"
#include "core/observability/log_system.hpp"
#include "core/resource_registry.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/json_godot.hpp"
#include "util/project_path.hpp"
#include "util/resource_fs.hpp"
#include "util/scene_path.hpp"
#include "util/type_hint.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/curve2d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance2d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace resource_ops {

namespace {

constexpr size_t kCurveMaxPoints = 256;

bool read_curve_vec(const mcp::JsonValue &obj, bool need_z, float &ox,
                    float &oy, float &oz, std::string &error) {
  error.clear();
  if (!obj.IsObject()) {
    error = "expected an object of the form {x, y} or {x, y, z}";
    return false;
  }
  for (const auto &kv : obj.GetObject()) {
    const bool known = kv.first == "x" || kv.first == "y" ||
                       (need_z && kv.first == "z");
    if (!known) {
      error = "unknown field '" + kv.first + "' (allowed: x, y" +
              std::string(need_z ? ", z" : "") + ")";
      return false;
    }
  }
  auto *px = obj.Find("x");
  auto *py = obj.Find("y");
  if (!px || !px->IsNumber()) {
    error = "requires numeric 'x'";
    return false;
  }
  if (!py || !py->IsNumber()) {
    error = "requires numeric 'y'";
    return false;
  }
  ox = static_cast<float>(util::json_number(px, 0.0));
  oy = static_cast<float>(util::json_number(py, 0.0));
  oz = 0.0f;
  if (need_z) {
    auto *pz = obj.Find("z");
    if (!pz || !pz->IsNumber()) {
      error = "requires numeric 'z' for Curve3D points";
      return false;
    }
    oz = static_cast<float>(util::json_number(pz, 0.0));
  }
  return true;
}

struct CurvePoint {
  float px = 0.0f;
  float py = 0.0f;
  float pz = 0.0f;
  float ix = 0.0f;
  float iy = 0.0f;
  float iz = 0.0f;
  float ox = 0.0f;
  float oy = 0.0f;
  float oz = 0.0f;
  bool has_in = false;
  bool has_out = false;
};

bool parse_curve_point(const mcp::JsonValue &entry, bool is_3d, CurvePoint &out,
                       std::string &error) {
  error.clear();
  if (!entry.IsObject()) {
    error = "must be an object of the form {pos: {x, y[, z]}, in?: {...}, "
            "out?: {...}}";
    return false;
  }
  for (const auto &kv : entry.GetObject()) {
    if (kv.first != "pos" && kv.first != "in" && kv.first != "out") {
      error = "has unknown field '" + kv.first + "' (allowed: pos, in, out)";
      return false;
    }
  }
  auto *pos = entry.Find("pos");
  if (!pos) {
    error = "requires 'pos' ({x, y} for Curve2D, {x, y, z} for Curve3D)";
    return false;
  }
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  std::string vec_err;
  if (!read_curve_vec(*pos, is_3d, x, y, z, vec_err)) {
    error = "invalid 'pos': " + vec_err;
    return false;
  }
  out.px = x;
  out.py = y;
  out.pz = z;
  out.has_in = false;
  out.has_out = false;
  auto *in_v = entry.Find("in");
  if (in_v) {
    if (!read_curve_vec(*in_v, is_3d, x, y, z, vec_err)) {
      error = "invalid 'in': " + vec_err;
      return false;
    }
    out.ix = x;
    out.iy = y;
    out.iz = z;
    out.has_in = true;
  }
  auto *out_v = entry.Find("out");
  if (out_v) {
    if (!read_curve_vec(*out_v, is_3d, x, y, z, vec_err)) {
      error = "invalid 'out': " + vec_err;
      return false;
    }
    out.ox = x;
    out.oy = y;
    out.oz = z;
    out.has_out = true;
  }
  return true;
}

} // namespace

namespace {

constexpr size_t kMeshMaxVertices = 65536;

bool is_digits(const std::string &s) {
  if (s.empty()) {
    return false;
  }
  for (char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

bool primitive_class_for_type(const std::string &type, std::string &out_cls) {
  out_cls.clear();
  if (type == "box") {
    out_cls = "BoxMesh";
  } else if (type == "capsule") {
    out_cls = "CapsuleMesh";
  } else if (type == "plane") {
    out_cls = "PlaneMesh";
  } else if (type == "quad") {
    out_cls = "QuadMesh";
  } else if (type == "sphere") {
    out_cls = "SphereMesh";
  } else if (type == "torus") {
    out_cls = "TorusMesh";
  } else if (type == "tube") {
    out_cls = "TubeTrailMesh";
  } else if (type == "text") {
    out_cls = "TextMesh";
  } else {
    return false;
  }
  return true;
}

std::string canonical_mesh_ref(const godot::Ref<godot::Resource> &res) {
  const std::string path = util::to_std(res->get_path());
  if (!path.empty()) {
    return path;
  }
  const std::string name = util::to_std(res->get_name());
  if (!name.empty()) {
    return "memory://" + name;
  }
  return std::to_string(static_cast<int64_t>(res->get_instance_id()));
}

bool resolve_resource_value_ref(const mcp::JsonValue &v,
                                godot::Ref<godot::Resource> &out,
                                std::string &error) {
  error.clear();
  out = godot::Ref<godot::Resource>();
  if (v.IsString()) {
    const std::string s = v.GetString();
    const std::string mem_prefix = "memory://";
    if (s.compare(0, mem_prefix.size(), mem_prefix) == 0) {
      const std::string name = s.substr(mem_prefix.size());
      if (name.empty()) {
        error = "empty mesh reference: expected memory://<name>";
        return false;
      }
      out = resolve_memory_resource(name);
      if (out.is_null()) {
        error = "memory resource not found: " + name +
                " (create it with create_primitive_mesh or create_array_mesh "
                "first)";
        return false;
      }
      return true;
    }
    const std::string res_prefix = "res://";
    if (s.compare(0, res_prefix.size(), res_prefix) == 0) {
      return load_resource_or_error(s, out, error, {}, "");
    }
    if (is_digits(s)) {
      out = resolve_memory_resource(s);
      if (out.is_null()) {
        int64_t oid = 0;
        try {
          oid = std::stoll(s);
        } catch (...) {
          oid = 0;
        }
        auto *obj = godot::ObjectDB::get_instance(static_cast<uint64_t>(oid));
        if (obj) {
          auto *robj = godot::Object::cast_to<godot::Resource>(obj);
          if (robj) {
            out = godot::Ref<godot::Resource>(robj);
          }
        }
      }
      if (out.is_null()) {
        error = "resource not found for object_id: " + s;
        return false;
      }
      return true;
    }
    if (s.empty()) {
      error = "empty mesh reference: expected memory://<name>, a registration "
              "name, a res:// path or an object_id";
      return false;
    }
    out = resolve_memory_resource(s);
    if (out.is_null()) {
      error = "memory resource not found: " + s +
              " (create it with create_primitive_mesh or create_array_mesh "
              "first)";
      return false;
    }
    return true;
  }
  if (v.IsObject()) {
    auto *it_resource = v.Find("resource");
    if (it_resource && it_resource->IsString()) {
      return resolve_resource_value_ref(*it_resource, out, error);
    }
    auto *it_path = v.Find("path");
    if (it_path && it_path->IsString()) {
      return load_resource_or_error(it_path->GetString(), out, error, {}, "");
    }
    bool has_oid = false;
    int64_t oid = 0;
    std::string resolve_err;
    out = resolve_resource(v, has_oid, oid, resolve_err);
    if (!resolve_err.empty()) {
      error = resolve_err;
      return false;
    }
    if (out.is_null()) {
      error = "resource not found: " + describe_target(v);
      return false;
    }
    return true;
  }
  error = "invalid resource reference: expected a memory://<name> string, a "
          "registration name, a res:// path, an object_id string or a locator "
          "object";
  return false;
}

bool read_mesh_vertex(const mcp::JsonValue &obj, godot::Vector3 &out,
                      std::string &error) {
  error.clear();
  if (!obj.IsObject()) {
    error = "expected an object of the form {x, y, z}";
    return false;
  }
  for (const auto &kv : obj.GetObject()) {
    if (kv.first != "x" && kv.first != "y" && kv.first != "z") {
      error = "unknown field '" + kv.first + "' (allowed: x, y, z)";
      return false;
    }
  }
  auto *px = obj.Find("x");
  auto *py = obj.Find("y");
  auto *pz = obj.Find("z");
  if (!px || !px->IsNumber() || !py || !py->IsNumber() || !pz ||
      !pz->IsNumber()) {
    error = "requires numeric 'x', 'y' and 'z'";
    return false;
  }
  out = godot::Vector3(static_cast<float>(util::json_number(px, 0.0)),
                       static_cast<float>(util::json_number(py, 0.0)),
                       static_cast<float>(util::json_number(pz, 0.0)));
  return true;
}

bool parse_vec3_list(const mcp::JsonValue *list, const char *field,
                     godot::PackedVector3Array &out, std::string &error) {
  error.clear();
  const auto &arr = list->GetArray();
  if (arr.empty()) {
    error = std::string(field) + " must not be empty";
    return false;
  }
  if (arr.size() > kMeshMaxVertices) {
    error = std::string(field) + " must contain 1-65536 entries (got " +
            std::to_string(arr.size()) + ")";
    return false;
  }
  for (size_t i = 0; i < arr.size(); i++) {
    godot::Vector3 v;
    std::string perr;
    if (!read_mesh_vertex(arr[i], v, perr)) {
      error = std::string(field) + "[" + std::to_string(i) + "] " + perr;
      return false;
    }
    out.append(v);
  }
  return true;
}

bool parse_index_list(const mcp::JsonValue *list, size_t vertex_count,
                      godot::PackedInt32Array &out, std::string &error) {
  error.clear();
  const auto &arr = list->GetArray();
  if (arr.empty()) {
    error = "indices must not be empty";
    return false;
  }
  for (size_t i = 0; i < arr.size(); i++) {
    const mcp::JsonValue &e = arr[i];
    if (!e.IsNumber()) {
      error = "indices[" + std::to_string(i) + "] must be an integer in [0, " +
              std::to_string(vertex_count) + ")";
      return false;
    }
    const double d = util::json_number(&e, -1.0);
    const int64_t idx = static_cast<int64_t>(d);
    if (static_cast<double>(idx) != d || idx < 0 ||
        static_cast<size_t>(idx) >= vertex_count) {
      error = "indices[" + std::to_string(i) + "] out of range: " +
              std::to_string(idx) + " (vertex_count " +
              std::to_string(vertex_count) + ")";
      return false;
    }
    out.append(static_cast<int32_t>(idx));
  }
  return true;
}

bool read_surface_idx(const mcp::JsonValue *v, int &out, std::string &error) {
  error.clear();
  if (!v || !v->IsNumber()) {
    error = "missing required parameter: surface_idx (non-negative integer)";
    return false;
  }
  const double d = util::json_number(v, -1.0);
  const int64_t i = static_cast<int64_t>(d);
  if (static_cast<double>(i) != d || i < 0) {
    error = "surface_idx must be a non-negative integer";
    return false;
  }
  out = static_cast<int>(i);
  return true;
}

bool apply_primitive_params(godot::Object *obj, const std::string &cls,
                            const mcp::JsonValue &params, std::string &error) {
  error.clear();
  if (!params.IsObject()) {
    error = "params must be an object mapping property names to values";
    return false;
  }
  godot::TypedArray<godot::Dictionary> props = obj->get_property_list();
  for (const auto &kv : params.GetObject()) {
    const std::string &key = kv.first;
    std::string hint;
    bool found = false;
    for (int64_t i = 0; i < props.size(); i++) {
      godot::Dictionary dict = props[i];
      if (dict.has("name") &&
          util::to_std(dict["name"].operator godot::String()) == key) {
        found = true;
        hint = util::infer_type_hint(dict, hint);
        break;
      }
    }
    if (!found) {
      error = "unknown property '" + key + "' for " + cls;
      return false;
    }
    godot::Variant val;
    try {
      val = VariantJson::deserialize_strict(kv.second, hint);
    } catch (const std::exception &e) {
      error =
          "invalid value for property '" + key + "': " + std::string(e.what());
      return false;
    }
    obj->set(godot::StringName(key.c_str()), val);
  }
  return true;
}

bool mesh_surface_has_normals(const godot::Ref<godot::Mesh> &mesh) {
  godot::Ref<godot::ArrayMesh> am = mesh;
  if (am.is_valid()) {
    if (am->get_surface_count() <= 0) {
      return false;
    }
    return am->surface_get_format(0).has_flag(
        godot::Mesh::ARRAY_FORMAT_NORMAL);
  }
  return true;
}

} // namespace

mcp::JsonValue handle_create_curve(const mcp::JsonValue &args) {
  auto *n = args.Find("name");
  if (!n || !n->IsString() || n->GetString().empty())
    return util::error_json("missing required parameter: name");
  std::string name = n->GetString();
  std::string dimension = "2d";
  auto *d = args.Find("dimension");
  if (d) {
    if (!d->IsString())
      return util::error_json("dimension must be \"2d\" or \"3d\"");
    dimension = d->GetString();
  }
  if (dimension != "2d" && dimension != "3d")
    return util::error_json("invalid dimension '" + dimension +
                            "': expected \"2d\" or \"3d\"");
  bool has_bake = false;
  float bake = 5.0f;
  auto *b = args.Find("bake_interval");
  if (b) {
    if (!b->IsNumber())
      return util::error_json("bake_interval must be a number greater than 0");
    bake = static_cast<float>(util::json_number(b, 0.0));
    if (bake <= 0.0f)
      return util::error_json("bake_interval must be a number greater than 0");
    has_bake = true;
  }
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs)
    return util::error_json("ClassDB singleton not available");
  const std::string cls = dimension == "2d" ? "Curve2D" : "Curve3D";
  godot::Variant obj_var = cdbs->instantiate(godot::StringName(cls.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL)
    return util::error_json("failed to instantiate " + cls);
  godot::Ref<godot::Resource> res = obj_var;
  if (res.is_null())
    return util::error_json("instantiated object is not a Resource: " + cls);
  if (has_bake) {
    if (dimension == "2d") {
      godot::Ref<godot::Curve2D> c2 = res;
      if (c2.is_valid())
        c2->set_bake_interval(bake);
    } else {
      godot::Ref<godot::Curve3D> c3 = res;
      if (c3.is_valid())
        c3->set_bake_interval(bake);
    }
  }
  res->set_name(godot::String(name.c_str()));
  res->set_path(godot::String(("memory://" + name).c_str()));
  resource_registry::register_resource(res, name);
  mcp::JsonValue info(mcp::JsonValue::object_tag);
  info["class"] = mcp::JsonValue(cls);
  info["name"] = mcp::JsonValue(name);
  info["path"] = mcp::JsonValue("memory://" + name);
  info["object_id"] =
      mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
  info["object_id_str"] = mcp::JsonValue(
      std::to_string(static_cast<int64_t>(res->get_instance_id())));
  info["dimension"] = mcp::JsonValue(dimension);
  if (has_bake)
    info["bake_interval"] = mcp::JsonValue(static_cast<double>(bake));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_curve_resource completed: " + name);
  return r;
}

mcp::JsonValue handle_set_curve_points(const mcp::JsonValue &args) {
  auto *n = args.Find("name");
  if (!n || !n->IsString() || n->GetString().empty())
    return util::error_json("missing required parameter: name");
  std::string name = n->GetString();
  auto *p = args.Find("points");
  if (!p || !p->IsArray())
    return util::error_json("missing required parameter: points (array of "
                            "1-256 {pos, in?, out?} objects)");
  const auto &arr = p->GetArray();
  if (arr.empty() || arr.size() > kCurveMaxPoints)
    return util::error_json("points must contain 1-256 entries (got " +
                            std::to_string(arr.size()) + ")");
  godot::Ref<godot::Resource> res = resolve_memory_resource(name);
  if (res.is_null())
    return util::error_json("memory resource not found: " + name +
                            " (create it with create_curve_resource first)");
  godot::Ref<godot::Curve2D> c2 = res;
  godot::Ref<godot::Curve3D> c3 = res;
  if (c2.is_null() && c3.is_null())
    return util::error_json("memory resource is not a Curve2D/Curve3D: " + name);
  const bool is_3d = c3.is_valid();
  std::vector<CurvePoint> parsed;
  parsed.reserve(arr.size());
  for (size_t i = 0; i < arr.size(); i++) {
    CurvePoint pt;
    std::string perr;
    if (!parse_curve_point(arr[i], is_3d, pt, perr))
      return util::error_json("points[" + std::to_string(i) + "] " + perr);
    parsed.push_back(pt);
  }
  if (is_3d) {
    c3->clear_points();
    for (const CurvePoint &pt : parsed) {
      const godot::Vector3 pos(pt.px, pt.py, pt.pz);
      const godot::Vector3 in_v(pt.has_in ? pt.ix : 0.0f,
                                pt.has_in ? pt.iy : 0.0f,
                                pt.has_in ? pt.iz : 0.0f);
      const godot::Vector3 out_v(pt.has_out ? pt.ox : 0.0f,
                                 pt.has_out ? pt.oy : 0.0f,
                                 pt.has_out ? pt.oz : 0.0f);
      c3->add_point(pos, in_v, out_v);
    }
  } else {
    c2->clear_points();
    for (const CurvePoint &pt : parsed) {
      const godot::Vector2 pos(pt.px, pt.py);
      const godot::Vector2 in_v(pt.has_in ? pt.ix : 0.0f,
                                pt.has_in ? pt.iy : 0.0f);
      const godot::Vector2 out_v(pt.has_out ? pt.ox : 0.0f,
                                 pt.has_out ? pt.oy : 0.0f);
      c2->add_point(pos, in_v, out_v);
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["name"] = mcp::JsonValue(name);
  r["class"] = mcp::JsonValue(is_3d ? "Curve3D" : "Curve2D");
  r["point_count"] = mcp::JsonValue(static_cast<int64_t>(parsed.size()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_curve_points completed: " + name);
  return r;
}

mcp::JsonValue handle_get_curve_baked_points(const mcp::JsonValue &args) {
  auto *n = args.Find("name");
  if (!n || !n->IsString() || n->GetString().empty())
    return util::error_json("missing required parameter: name");
  std::string name = n->GetString();
  godot::Ref<godot::Resource> res = resolve_memory_resource(name);
  if (res.is_null())
    return util::error_json("memory resource not found: " + name +
                            " (create it with create_curve_resource first)");
  godot::Ref<godot::Curve2D> c2 = res;
  godot::Ref<godot::Curve3D> c3 = res;
  if (c2.is_null() && c3.is_null())
    return util::error_json("memory resource is not a Curve2D/Curve3D: " + name);
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  if (c3.is_valid()) {
    const godot::PackedVector3Array baked = c3->get_baked_points();
    mcp::JsonValue pts(mcp::JsonValue::array_tag);
    for (int i = 0; i < baked.size(); i++) {
      const godot::Vector3 v = baked[i];
      pts.PushBack(util::vec3_to_json(v));
    }
    inner["points"] = std::move(pts);
    inner["count"] = mcp::JsonValue(static_cast<int64_t>(baked.size()));
    inner["baked_length"] =
        mcp::JsonValue(static_cast<double>(c3->get_baked_length()));
    inner["class"] = mcp::JsonValue("Curve3D");
  } else {
    const godot::PackedVector2Array baked = c2->get_baked_points();
    mcp::JsonValue pts(mcp::JsonValue::array_tag);
    for (int i = 0; i < baked.size(); i++) {
      const godot::Vector2 v = baked[i];
      pts.PushBack(util::vec2_to_json(v));
    }
    inner["points"] = std::move(pts);
    inner["count"] = mcp::JsonValue(static_cast<int64_t>(baked.size()));
    inner["baked_length"] =
        mcp::JsonValue(static_cast<double>(c2->get_baked_length()));
    inner["class"] = mcp::JsonValue("Curve2D");
  }
  inner["name"] = mcp::JsonValue(name);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

mcp::JsonValue handle_create_primitive_mesh(const mcp::JsonValue &args) {
  auto *t = args.Find("type");
  if (!t || !t->IsString() || t->GetString().empty()) {
    return util::error_json("missing required parameter: type (one of box, "
                            "capsule, plane, quad, sphere, torus, tube, text)");
  }
  const std::string type = t->GetString();
  std::string cls;
  if (!primitive_class_for_type(type, cls)) {
    return util::error_json("invalid type '" + type +
                            "': expected one of box, capsule, plane, quad, "
                            "sphere, torus, tube, text");
  }
  std::string name;
  auto *n = args.Find("name");
  if (n) {
    if (!n->IsString()) {
      return util::error_json("name must be a string");
    }
    name = n->GetString();
  }
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    return util::error_json("ClassDB singleton not available");
  }
  godot::Variant obj_var = cdbs->instantiate(godot::StringName(cls.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL) {
    return util::error_json("failed to instantiate " + cls);
  }
  godot::Ref<godot::Resource> res = obj_var;
  if (res.is_null()) {
    return util::error_json("instantiated object is not a Resource: " + cls);
  }
  auto *p = args.Find("params");
  if (p) {
    std::string perr;
    if (!apply_primitive_params(res.ptr(), cls, *p, perr)) {
      return util::error_json(perr);
    }
  }
  if (!name.empty()) {
    res->set_name(godot::String(name.c_str()));
    res->set_path(godot::String(("memory://" + name).c_str()));
  }
  resource_registry::register_resource(res, name);
  mcp::JsonValue info(mcp::JsonValue::object_tag);
  info["class"] = mcp::JsonValue(cls);
  info["name"] = mcp::JsonValue(name);
  info["path"] = mcp::JsonValue(util::to_std(res->get_path()));
  info["object_id"] =
      mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
  info["object_id_str"] = mcp::JsonValue(
      std::to_string(static_cast<int64_t>(res->get_instance_id())));
  info["type"] = mcp::JsonValue(type);
  info["mesh_ref"] = mcp::JsonValue(canonical_mesh_ref(res));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_primitive_mesh completed: " + type);
  return r;
}

mcp::JsonValue handle_create_array_mesh(const mcp::JsonValue &args) {
  auto *v = args.Find("vertices");
  if (!v || !v->IsArray()) {
    return util::error_json(
        "missing required parameter: vertices (array of 1-65536 {x, y, z} "
        "objects)");
  }
  godot::PackedVector3Array verts;
  std::string verr;
  if (!parse_vec3_list(v, "vertices", verts, verr)) {
    return util::error_json(verr);
  }
  godot::PackedVector3Array normals;
  bool has_normals = false;
  auto *no = args.Find("normals");
  if (no) {
    if (!no->IsArray()) {
      return util::error_json("normals must be an array of {x, y, z} objects "
                              "matching vertices in length");
    }
    std::string nerr;
    if (!parse_vec3_list(no, "normals", normals, nerr)) {
      return util::error_json(nerr);
    }
    if (normals.size() != verts.size()) {
      return util::error_json("normals length " +
                              std::to_string(normals.size()) +
                              " does not match vertices length " +
                              std::to_string(verts.size()));
    }
    has_normals = true;
  }
  godot::PackedInt32Array indices;
  bool has_indices = false;
  auto *io = args.Find("indices");
  if (io) {
    if (!io->IsArray()) {
      return util::error_json(
          "indices must be an array of integers in [0, vertex_count)");
    }
    std::string ierr;
    if (!parse_index_list(io, static_cast<size_t>(verts.size()), indices,
                          ierr)) {
      return util::error_json(ierr);
    }
    has_indices = true;
  }
  std::string name;
  auto *n = args.Find("name");
  if (n) {
    if (!n->IsString()) {
      return util::error_json("name must be a string");
    }
    name = n->GetString();
  }
  godot::Ref<godot::ArrayMesh> mesh;
  mesh.instantiate();
  if (mesh.is_null()) {
    return util::error_json("failed to instantiate ArrayMesh");
  }
  godot::Array arrays;
  arrays.resize(godot::Mesh::ARRAY_MAX);
  arrays[godot::Mesh::ARRAY_VERTEX] = verts;
  if (has_normals) {
    arrays[godot::Mesh::ARRAY_NORMAL] = normals;
  }
  if (has_indices) {
    arrays[godot::Mesh::ARRAY_INDEX] = indices;
  }
  mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
  godot::Ref<godot::Resource> res = mesh;
  if (!name.empty()) {
    res->set_name(godot::String(name.c_str()));
    res->set_path(godot::String(("memory://" + name).c_str()));
  }
  resource_registry::register_resource(res, name);
  mcp::JsonValue info(mcp::JsonValue::object_tag);
  info["class"] = mcp::JsonValue("ArrayMesh");
  info["name"] = mcp::JsonValue(name);
  info["path"] = mcp::JsonValue(util::to_std(res->get_path()));
  info["object_id"] =
      mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
  info["object_id_str"] = mcp::JsonValue(
      std::to_string(static_cast<int64_t>(res->get_instance_id())));
  info["mesh_ref"] = mcp::JsonValue(canonical_mesh_ref(res));
  info["vertex_count"] =
      mcp::JsonValue(static_cast<int64_t>(verts.size()));
  info["index_count"] =
      mcp::JsonValue(static_cast<int64_t>(indices.size()));
  info["has_normals"] = mcp::JsonValue(has_normals);
  info["surface_count"] =
      mcp::JsonValue(static_cast<int64_t>(mesh->get_surface_count()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(info);
  if (!has_normals) {
    r["warning"] = mcp::JsonValue(
        "no normals provided; shaded materials may render flat — recreate the "
        "mesh with normals for correct lighting");
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_array_mesh completed");
  return r;
}

mcp::JsonValue handle_set_mesh_surface_material(const mcp::JsonValue &args) {
  auto *mr = args.Find("mesh_ref");
  if (!mr) {
    return util::error_json("missing required parameter: mesh_ref");
  }
  godot::Ref<godot::Resource> res;
  std::string rerr;
  if (!resolve_resource_value_ref(*mr, res, rerr)) {
    return util::error_json(rerr);
  }
  godot::Ref<godot::Mesh> mesh = res;
  if (mesh.is_null()) {
    return util::error_json("resource is not a Mesh (actual class: " +
                            util::to_std(res->get_class()) + ")");
  }
  int idx = 0;
  std::string ierr;
  if (!read_surface_idx(args.Find("surface_idx"), idx, ierr)) {
    return util::error_json(ierr);
  }
  const int32_t count = mesh->get_surface_count();
  if (idx >= count) {
    return util::error_json("surface_idx out of range: " +
                            std::to_string(idx) + " (mesh has " +
                            std::to_string(count) + " surface(s))");
  }
  auto *mp = args.Find("material_path");
  if (!mp) {
    return util::error_json("missing required parameter: material_path");
  }
  godot::Ref<godot::Resource> mres;
  std::string merr;
  if (!resolve_resource_value_ref(*mp, mres, merr)) {
    return util::error_json(merr);
  }
  godot::Ref<godot::Material> mat = mres;
  if (mat.is_null()) {
    return util::error_json("resource is not a Material (actual class: " +
                            util::to_std(mres->get_class()) + ")");
  }
  mesh->surface_set_material(idx, mat);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["class"] = mcp::JsonValue(util::to_std(mesh->get_class()));
  r["surface_idx"] = mcp::JsonValue(static_cast<int64_t>(idx));
  r["surface_count"] = mcp::JsonValue(static_cast<int64_t>(count));
  r["material"] = mcp::JsonValue(util::to_std(mat->get_class()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_mesh_surface_material completed");
  return r;
}

mcp::JsonValue handle_assign_mesh_to_instance(const mcp::JsonValue &args) {
  auto *np = args.Find("node_path");
  if (!np || !np->IsString() || np->GetString().empty()) {
    return util::error_json("missing required parameter: node_path");
  }
  const std::string path = np->GetString();
  auto *mr = args.Find("mesh_ref");
  if (!mr) {
    return util::error_json("missing required parameter: mesh_ref");
  }
  godot::Ref<godot::Resource> res;
  std::string rerr;
  if (!resolve_resource_value_ref(*mr, res, rerr)) {
    return util::error_json(rerr);
  }
  godot::Ref<godot::Mesh> mesh = res;
  if (mesh.is_null()) {
    return util::error_json("resource is not a Mesh (actual class: " +
                            util::to_std(res->get_class()) + ")");
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  godot::Node *scene_root = editor->get_edited_scene_root();
  if (!scene_root) {
    return util::error_json(
        "no edited scene is open: open a scene before assigning a mesh");
  }
  std::string path_hint;
  godot::Node *node = util::resolve_scene_node(path, scene_root, &path_hint);
  if (!node) {
    std::string detail = "node not found: " + path;
    if (!path_hint.empty()) {
      detail += " (" + path_hint + ")";
    }
    return util::error_json(detail);
  }
  auto *mi3 = godot::Object::cast_to<godot::MeshInstance3D>(node);
  auto *mi2 = godot::Object::cast_to<godot::MeshInstance2D>(node);
  if (!mi3 && !mi2) {
    return util::error_json("node is not a MeshInstance2D/MeshInstance3D: " +
                            path + " (actual class: " +
                            util::to_std(node->get_class()) + ")");
  }
  if (mi3) {
    mi3->set_mesh(mesh);
  } else {
    mi2->set_mesh(mesh);
  }
  godot::Ref<godot::Mesh> back = mi3 ? mi3->get_mesh() : mi2->get_mesh();
  if (back.is_null() || back.ptr() != mesh.ptr()) {
    return util::error_json(
        "failed to assign mesh to " + path +
        ": the instance rejected the in-memory mesh — save it with "
        "save_resource (pass name or object_id) then assign the saved res:// "
        "path instead");
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["assigned"] = mcp::JsonValue(true);
  r["node_path"] = mcp::JsonValue(path);
  r["mesh_ref"] = mcp::JsonValue(canonical_mesh_ref(res));
  r["class"] = mcp::JsonValue(util::to_std(node->get_class()));
  if (!mesh_surface_has_normals(mesh)) {
    r["warning"] = mcp::JsonValue(
        "mesh has no normals; shaded materials may render flat — recreate the "
        "mesh with normals for correct lighting");
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "assign_mesh_to_instance completed: " + path);
  return r;
}

} // namespace resource_ops
} // namespace godot_autopilot
