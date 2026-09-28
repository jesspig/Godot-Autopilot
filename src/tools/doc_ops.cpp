#include "doc_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include <algorithm>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace doc_ops {

namespace {

mcp::JsonValue variant_to_json(const godot::Variant &v) {
  switch (v.get_type()) {
  case godot::Variant::NIL:
    return mcp::JsonValue(nullptr);
  case godot::Variant::BOOL:
    return mcp::JsonValue(static_cast<bool>(v));
  case godot::Variant::INT:
    return mcp::JsonValue(static_cast<int64_t>(v));
  case godot::Variant::FLOAT:
    return mcp::JsonValue(static_cast<double>(v));
  case godot::Variant::STRING: {
    godot::String s = static_cast<godot::String>(v);
    return mcp::JsonValue(util::to_std(s));
  }
  case godot::Variant::DICTIONARY: {
    mcp::JsonValue obj(mcp::JsonValue::object_tag);
    godot::Dictionary d = static_cast<godot::Dictionary>(v);
    godot::Array keys = d.keys();
    for (int i = 0; i < keys.size(); i++) {
      godot::String key = static_cast<godot::String>(keys[i]);
      obj[util::to_std(key)] = variant_to_json(d[keys[i]]);
    }
    return obj;
  }
  case godot::Variant::ARRAY:
  case godot::Variant::PACKED_STRING_ARRAY:
  case godot::Variant::PACKED_INT32_ARRAY:
  case godot::Variant::PACKED_FLOAT32_ARRAY:
  case godot::Variant::PACKED_INT64_ARRAY:
  case godot::Variant::PACKED_FLOAT64_ARRAY: {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::Array a = static_cast<godot::Array>(v);
    for (int i = 0; i < a.size(); i++) {
      arr.PushBack(variant_to_json(a[i]));
    }
    return arr;
  }
  default:
    return mcp::JsonValue(util::to_std(v.stringify()));
  }
}

std::string lower_text(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

std::string trim_text(const std::string &s) {
  size_t begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return std::string();
  }
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}

std::string compact_name(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : lower_text(s)) {
    if (c != '_') {
      out.push_back(c);
    }
  }
  return out;
}

struct VariantBuiltin {
  const char *canonical;
  godot::Variant::Type type;
  const char *json_shape;
  std::vector<std::string> methods;
  std::vector<std::pair<std::string, std::string>> constants;
};

const std::vector<std::string> kPackedArrayMethods = {
    "append",   "append_array", "bsearch", "clear",  "count",
    "duplicate", "has",        "insert",  "is_empty", "remove_at",
    "resize",   "reverse",     "rfind",   "set",    "size",
    "slice",    "sort",        "to_byte_array"};

const std::vector<VariantBuiltin> kVariantBuiltins = {
    {"Nil", godot::Variant::NIL, "JSON null", {}, {}},
    {"bool", godot::Variant::BOOL, "JSON boolean", {}, {}},
    {"int", godot::Variant::INT, "JSON number (integer)", {}, {}},
    {"float", godot::Variant::FLOAT, "JSON number", {}, {}},
    {"String",
     godot::Variant::STRING,
     "JSON string",
     {"begins_with", "capitalize", "contains", "count", "dedent", "ends_with",
      "erase", "find", "format", "get_base_dir", "get_basename",
      "get_extension", "get_file", "get_slice", "insert", "is_empty",
      "is_valid_float", "is_valid_identifier", "is_valid_int", "join", "left",
      "length", "match", "replace", "right", "split", "strip_edges", "substr",
      "to_float", "to_int", "to_lower", "to_upper"},
     {}},
    {"Vector2",
     godot::Variant::VECTOR2,
     "{\"x\": 0, \"y\": 0}",
     {"abs", "angle", "angle_to", "angle_to_point", "aspect",
      "bezier_interpolate", "bounce", "ceil", "clamp", "cross",
      "cubic_interpolate", "distance_squared_to", "distance_to", "dot", "floor",
      "is_equal_approx", "is_finite", "is_normalized", "is_zero_approx",
      "length", "length_squared", "lerp", "limit_length", "move_toward",
      "normalized", "posmod", "project", "reflect", "rotated", "round", "sign",
      "slide", "snapped"},
     {{"AXIS_X", "0"},
      {"AXIS_Y", "1"},
      {"ZERO", "(0, 0)"},
      {"ONE", "(1, 1)"},
      {"INF", "(inf, inf)"},
      {"LEFT", "(-1, 0)"},
      {"RIGHT", "(1, 0)"},
      {"UP", "(0, -1)"},
      {"DOWN", "(0, 1)"}}},
    {"Vector2i",
     godot::Variant::VECTOR2I,
     "{\"x\": 0, \"y\": 0}",
     {"abs", "aspect", "clamp", "distance_squared_to", "distance_to", "length",
      "length_squared", "max_axis_index", "max_axis_value", "min_axis_index",
      "min_axis_value", "snapped"},
     {{"AXIS_X", "0"},
      {"AXIS_Y", "1"},
      {"ZERO", "(0, 0)"},
      {"ONE", "(1, 1)"},
      {"MIN", "(-2147483648, -2147483648)"},
      {"MAX", "(2147483647, 2147483647)"},
      {"LEFT", "(-1, 0)"},
      {"RIGHT", "(1, 0)"},
      {"UP", "(0, -1)"},
      {"DOWN", "(0, 1)"}}},
    {"Rect2",
     godot::Variant::RECT2,
     "{\"position\": {\"x\": 0, \"y\": 0}, \"size\": {\"x\": 64, \"y\": 32}}",
     {"abs", "encloses", "expand", "get_area", "get_center", "get_end",
      "get_position", "get_size", "grow", "grow_individual", "grow_side",
      "has_area", "has_point", "intersection", "intersects", "is_equal_approx",
      "is_finite", "merge"},
     {}},
    {"Rect2i",
     godot::Variant::RECT2I,
     "{\"position\": {\"x\": 0, \"y\": 0}, \"size\": {\"x\": 64, \"y\": 32}}",
     {"abs", "encloses", "expand", "get_area", "get_center", "get_end",
      "get_position", "get_size", "grow", "grow_individual", "has_area",
      "has_point", "intersection", "intersects", "merge"},
     {}},
    {"Vector3",
     godot::Variant::VECTOR3,
     "{\"x\": 0, \"y\": 0, \"z\": 0}",
     {"abs", "angle_to", "bounce", "ceil", "clamp", "cross",
      "cubic_interpolate", "distance_squared_to", "distance_to", "dot", "floor",
      "is_equal_approx", "is_finite", "is_normalized", "is_zero_approx",
      "length", "length_squared", "lerp", "limit_length", "move_toward",
      "normalized", "outer", "posmod", "reflect", "rotated", "round", "sign",
      "slide", "snapped"},
     {{"AXIS_X", "0"},
      {"AXIS_Y", "1"},
      {"AXIS_Z", "2"},
      {"ZERO", "(0, 0, 0)"},
      {"ONE", "(1, 1, 1)"},
      {"INF", "(inf, inf, inf)"},
      {"LEFT", "(-1, 0, 0)"},
      {"RIGHT", "(1, 0, 0)"},
      {"UP", "(0, 1, 0)"},
      {"DOWN", "(0, -1, 0)"},
      {"FORWARD", "(0, 0, -1)"},
      {"BACK", "(0, 0, 1)"}}},
    {"Vector3i",
     godot::Variant::VECTOR3I,
     "{\"x\": 0, \"y\": 0, \"z\": 0}",
     {"abs", "clamp", "distance_squared_to", "distance_to", "length",
      "length_squared", "max_axis_index", "max_axis_value", "min_axis_index",
      "min_axis_value", "snapped"},
     {{"AXIS_X", "0"},
      {"AXIS_Y", "1"},
      {"AXIS_Z", "2"},
      {"ZERO", "(0, 0, 0)"},
      {"ONE", "(1, 1, 1)"},
      {"MIN", "(-2147483648, -2147483648, -2147483648)"},
      {"MAX", "(2147483647, 2147483647, 2147483647)"}}},
    {"Transform2D",
     godot::Variant::TRANSFORM2D,
     "{\"columns\": [[1, 0], [0, 1], [0, 0]]}",
     {"affine_inverse", "determinant", "get_origin", "get_rotation",
      "get_scale", "get_skew", "interpolate_with", "inverse", "is_conformal",
      "is_equal_approx", "is_finite", "looking_at", "orthonormalized",
      "rotated", "scaled", "translated", "xform", "xform_inv"},
     {{"IDENTITY", "((1, 0), (0, 1), (0, 0))"},
      {"FLIP_X", "((-1, 0), (0, 1), (0, 0))"},
      {"FLIP_Y", "((1, 0), (0, -1), (0, 0))"}}},
    {"Vector4",
     godot::Variant::VECTOR4,
     "{\"x\": 0, \"y\": 0, \"z\": 0, \"w\": 0}",
     {"abs", "clamp", "cubic_interpolate", "distance_squared_to", "distance_to",
      "dot", "inverse", "is_equal_approx", "is_finite", "is_normalized",
      "is_zero_approx", "length", "length_squared", "lerp", "normalized",
      "posmod", "snapped"},
     {{"AXIS_X", "0"},
      {"AXIS_Y", "1"},
      {"AXIS_Z", "2"},
      {"AXIS_W", "3"},
      {"ZERO", "(0, 0, 0, 0)"},
      {"ONE", "(1, 1, 1, 1)"},
      {"INF", "(inf, inf, inf, inf)"}}},
    {"Vector4i",
     godot::Variant::VECTOR4I,
     "{\"x\": 0, \"y\": 0, \"z\": 0, \"w\": 0}",
     {"abs", "clamp", "distance_squared_to", "distance_to", "length",
      "length_squared", "max_axis_index", "max_axis_value", "min_axis_index",
      "min_axis_value", "snapped"},
     {{"AXIS_X", "0"},
      {"AXIS_Y", "1"},
      {"AXIS_Z", "2"},
      {"AXIS_W", "3"},
      {"ZERO", "(0, 0, 0, 0)"},
      {"ONE", "(1, 1, 1, 1)"},
      {"MIN", "(-2147483648, -2147483648, -2147483648, -2147483648)"},
      {"MAX", "(2147483647, 2147483647, 2147483647, 2147483647)"}}},
    {"Plane",
     godot::Variant::PLANE,
     "{\"normal\": {\"x\": 0, \"y\": 1, \"z\": 0}, \"d\": 0}",
     {"abs", "distance_to", "get_center", "has_point", "intersect_3",
      "intersects_3", "intersects_ray", "intersects_segment", "is_equal_approx",
      "is_finite", "is_point_over", "normalized", "project"},
     {{"PLANE_YZ", "(1, 0, 0, 0)"},
      {"PLANE_XZ", "(0, 1, 0, 0)"},
      {"PLANE_XY", "(0, 0, 1, 0)"}}},
    {"Quaternion",
     godot::Variant::QUATERNION,
     "{\"x\": 0, \"y\": 0, \"z\": 0, \"w\": 1}",
     {"angle_to", "dot", "exp", "get_angle", "get_axis", "get_euler",
      "inverse", "is_equal_approx", "is_finite", "is_normalized", "length",
      "length_squared", "log", "normalized", "slerp", "xform"},
     {{"IDENTITY", "(0, 0, 0, 1)"}}},
    {"AABB",
     godot::Variant::AABB,
     "{\"position\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"size\": {\"x\": 1, "
     "\"y\": 1, \"z\": 1}}",
     {"abs", "encloses", "expand", "get_area", "get_center", "get_endpoint",
      "get_position", "get_size", "get_volume", "grow", "has_area",
      "has_point", "has_volume", "intersection", "intersects",
      "intersects_ray", "intersects_segment", "is_equal_approx", "is_finite",
      "merge"},
     {}},
    {"Basis",
     godot::Variant::BASIS,
     "{\"rows\": [[1, 0, 0], [0, 1, 0], [0, 0, 1]]}",
     {"determinant", "get_euler", "get_rotation_quaternion", "get_scale",
      "inverse", "is_conformal", "is_equal_approx", "is_finite",
      "orthonormalized", "rotated", "scaled", "slerp", "transposed", "xform",
      "xform_inv"},
     {{"IDENTITY", "((1, 0, 0), (0, 1, 0), (0, 0, 1))"},
      {"FLIP_X", "((-1, 0, 0), (0, 1, 0), (0, 0, 1))"},
      {"FLIP_Y", "((1, 0, 0), (0, -1, 0), (0, 0, 1))"},
      {"FLIP_Z", "((1, 0, 0), (0, 1, 0), (0, 0, -1))"}}},
    {"Transform3D",
     godot::Variant::TRANSFORM3D,
     "{\"basis\": {\"rows\": [[1, 0, 0], [0, 1, 0], [0, 0, 1]]}, \"origin\": "
     "{\"x\": 0, \"y\": 0, \"z\": 0}}",
     {"affine_inverse", "interpolate_with", "inverse", "is_equal_approx",
      "is_finite", "looking_at", "orthonormalized", "rotated", "scaled",
      "translated", "xform", "xform_inv"},
     {{"IDENTITY", "(identity basis, (0, 0, 0))"},
      {"FLIP_X", "(flipped x basis, (0, 0, 0))"},
      {"FLIP_Y", "(flipped y basis, (0, 0, 0))"},
      {"FLIP_Z", "(flipped z basis, (0, 0, 0))"}}},
    {"Projection",
     godot::Variant::PROJECTION,
     "{\"columns\": [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]}",
     {},
     {{"IDENTITY", "identity matrix"}}},
    {"Color",
     godot::Variant::COLOR,
     "{\"r\": 1, \"g\": 1, \"b\": 1, \"a\": 1}",
     {"blend", "darkened", "from_hsv", "from_string", "hex", "html",
      "html_is_valid", "inverted", "is_equal_approx", "lerp", "lightened",
      "linear_to_srgb", "srgb_to_linear", "to_html"},
     {{"BLACK", "(0, 0, 0, 1)"},
      {"BLUE", "(0, 0, 1, 1)"},
      {"GREEN", "(0, 1, 0, 1)"},
      {"RED", "(1, 0, 0, 1)"},
      {"TRANSPARENT", "(1, 1, 1, 0)"},
      {"WHITE", "(1, 1, 1, 1)"}}},
    {"StringName",
     godot::Variant::STRING_NAME,
     "JSON string",
     {"begins_with", "contains", "count", "dedent", "ends_with", "erase",
      "find", "format", "insert", "is_empty", "join", "left", "length",
      "match", "replace", "right", "split", "strip_edges", "substr",
      "to_lower", "to_upper"},
     {}},
    {"NodePath",
     godot::Variant::NODE_PATH,
     "JSON string",
     {"get_concatenated_names", "get_concatenated_subnames", "get_name",
      "get_name_count", "get_subname", "get_subname_count", "is_absolute",
      "is_empty", "slice"},
     {}},
    {"RID", godot::Variant::RID, "{\"id\": 0}", {}, {}},
    {"Object",
     godot::Variant::OBJECT,
     "{\"class\": \"...\", ...}",
     {},
     {}},
    {"Callable",
     godot::Variant::CALLABLE,
     "{\"object_id\": 0, \"method\": \"...\"}",
     {},
     {}},
    {"Signal",
     godot::Variant::SIGNAL,
     "{\"object_id\": 0, \"signal\": \"...\"}",
     {},
     {}},
    {"Dictionary",
     godot::Variant::DICTIONARY,
     "JSON object",
     {"clear", "duplicate", "erase", "find_key", "get", "get_or_add", "has",
      "has_all", "hash", "is_empty", "keys", "merge", "merged", "size",
      "values"},
     {}},
    {"Array",
     godot::Variant::ARRAY,
     "JSON array",
     {"append", "append_array", "assign", "at", "back", "bsearch", "clear",
      "count", "duplicate", "erase", "fill", "filter", "find", "front", "get",
      "has", "insert", "is_empty", "map", "max", "min", "pop_at", "pop_back",
      "pop_front", "push_back", "push_front", "reduce", "remove_at", "resize",
      "reverse", "rfind", "set", "shuffle", "size", "slice", "sort"},
     {}},
    {"PackedByteArray",
     godot::Variant::PACKED_BYTE_ARRAY,
     "JSON array of integers",
     kPackedArrayMethods,
     {}},
    {"PackedInt32Array",
     godot::Variant::PACKED_INT32_ARRAY,
     "JSON array of integers",
     kPackedArrayMethods,
     {}},
    {"PackedInt64Array",
     godot::Variant::PACKED_INT64_ARRAY,
     "JSON array of integers",
     kPackedArrayMethods,
     {}},
    {"PackedFloat32Array",
     godot::Variant::PACKED_FLOAT32_ARRAY,
     "JSON array of numbers",
     kPackedArrayMethods,
     {}},
    {"PackedFloat64Array",
     godot::Variant::PACKED_FLOAT64_ARRAY,
     "JSON array of numbers",
     kPackedArrayMethods,
     {}},
    {"PackedStringArray",
     godot::Variant::PACKED_STRING_ARRAY,
     "JSON array of strings",
     kPackedArrayMethods,
     {}},
    {"PackedVector2Array",
     godot::Variant::PACKED_VECTOR2_ARRAY,
     "JSON array of {\"x\", \"y\"}",
     kPackedArrayMethods,
     {}},
    {"PackedVector3Array",
     godot::Variant::PACKED_VECTOR3_ARRAY,
     "JSON array of {\"x\", \"y\", \"z\"}",
     kPackedArrayMethods,
     {}},
    {"PackedColorArray",
     godot::Variant::PACKED_COLOR_ARRAY,
     "JSON array of {\"r\", \"g\", \"b\", \"a\"}",
     kPackedArrayMethods,
     {}},
    {"PackedVector4Array",
     godot::Variant::PACKED_VECTOR4_ARRAY,
     "JSON array of {\"x\", \"y\", \"z\", \"w\"}",
     kPackedArrayMethods,
     {}},
};

const VariantBuiltin *find_variant_builtin(const std::string &name) {
  std::string key = compact_name(name);
  if (key == "dict") {
    key = "dictionary";
  }
  for (const auto &entry : kVariantBuiltins) {
    if (compact_name(entry.canonical) == key) {
      return &entry;
    }
  }
  return nullptr;
}

} // namespace

mcp::JsonValue handle_get_class(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_docs_class called");
  auto *np = args.Find("class");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: class");
    return e;
  }
  std::string class_name = np->GetString();
  std::string section_param;
  if (auto *sp = args.Find("section")) {
    if (!sp->IsString()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "invalid parameter: section must be a comma-separated string of "
          "methods, properties, signals, enums, constants");
      return e;
    }
    section_param = sp->GetString();
  }
  std::string member_param;
  if (auto *mp = args.Find("member")) {
    if (!mp->IsString()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "invalid parameter: member must be a case-insensitive substring");
      return e;
    }
    member_param = mp->GetString();
  }
  bool want_methods = true;
  bool want_properties = true;
  bool want_signals = true;
  bool want_enums = true;
  bool want_constants = true;
  if (!trim_text(section_param).empty()) {
    want_methods = false;
    want_properties = false;
    want_signals = false;
    want_enums = false;
    want_constants = false;
    size_t pos = 0;
    while (pos <= section_param.size()) {
      size_t comma = section_param.find(',', pos);
      std::string token = lower_text(trim_text(section_param.substr(
          pos, comma == std::string::npos ? comma : comma - pos)));
      if (token == "methods") {
        want_methods = true;
      } else if (token == "properties") {
        want_properties = true;
      } else if (token == "signals") {
        want_signals = true;
      } else if (token == "enums") {
        want_enums = true;
      } else if (token == "constants") {
        want_constants = true;
      } else if (!token.empty()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "unknown section '" + token +
            "' for get_docs_class (expected methods, properties, signals, "
            "enums, constants)");
        return e;
      }
      if (comma == std::string::npos) {
        break;
      }
      pos = comma + 1;
    }
  }
  std::string member_lower = lower_text(member_param);
  auto member_ok = [&member_lower](const std::string &name) {
    return member_lower.empty() ||
           lower_text(name).find(member_lower) != std::string::npos;
  };
  auto *cd = godot::ClassDBSingleton::get_singleton();
  if (!cd) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDBSingleton not available");
    return e;
  }
  godot::StringName sn(class_name.c_str());
  if (!cd->class_exists(sn)) {
    if (const VariantBuiltin *builtin = find_variant_builtin(class_name)) {
      mcp::JsonValue result(mcp::JsonValue::object_tag);
      result["name"] = mcp::JsonValue(builtin->canonical);
      result["value_kind"] = mcp::JsonValue("builtin");
      result["variant_type"] =
          mcp::JsonValue(static_cast<int64_t>(builtin->type));
      result["json_shape"] = mcp::JsonValue(builtin->json_shape);
      if (want_methods) {
        mcp::JsonValue arr(mcp::JsonValue::array_tag);
        for (const auto &m : builtin->methods) {
          if (!member_ok(m)) {
            continue;
          }
          mcp::JsonValue item(mcp::JsonValue::object_tag);
          item["name"] = mcp::JsonValue(m);
          arr.PushBack(std::move(item));
        }
        result["methods"] = std::move(arr);
      }
      if (want_constants) {
        mcp::JsonValue arr(mcp::JsonValue::array_tag);
        for (const auto &c : builtin->constants) {
          if (!member_ok(c.first)) {
            continue;
          }
          mcp::JsonValue item(mcp::JsonValue::object_tag);
          item["name"] = mcp::JsonValue(c.first);
          item["value"] = mcp::JsonValue(c.second);
          arr.PushBack(std::move(item));
        }
        result["constants"] = std::move(arr);
      }
      result["note"] = mcp::JsonValue(
          "builtin Variant value type (no ClassDB entry); methods and "
          "constants are a curated subset");
      mcp::JsonValue r(mcp::JsonValue::object_tag);
      r["result"] = std::move(result);
      LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                "get_docs_class completed");
      return r;
    }
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("class not found: " + class_name);
    return e;
  }
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["name"] = mcp::JsonValue(class_name);
  result["parent_class"] =
      mcp::JsonValue(util::to_std(godot::String(cd->get_parent_class(sn))));
  result["api_type"] =
      mcp::JsonValue(static_cast<int64_t>(cd->class_get_api_type(sn)));
  result["can_instantiate"] = mcp::JsonValue(cd->can_instantiate(sn));
  if (want_methods) {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::TypedArray<godot::Dictionary> methods =
        cd->class_get_method_list(sn, false);
    for (int i = 0; i < methods.size(); i++) {
      godot::Dictionary d = methods[i];
      std::string mname;
      if (d.has("name")) {
        godot::String s = static_cast<godot::String>(d["name"]);
        mname = util::to_std(s);
      }
      if (!member_ok(mname)) {
        continue;
      }
      arr.PushBack(variant_to_json(methods[i]));
    }
    result["methods"] = std::move(arr);
  }
  if (want_properties) {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::TypedArray<godot::Dictionary> props =
        cd->class_get_property_list(sn, false);
    for (int i = 0; i < props.size(); i++) {
      godot::Dictionary d = props[i];
      std::string pname;
      if (d.has("name")) {
        godot::String s = static_cast<godot::String>(d["name"]);
        pname = util::to_std(s);
      }
      if (!member_ok(pname)) {
        continue;
      }
      arr.PushBack(variant_to_json(props[i]));
    }
    result["properties"] = std::move(arr);
  }
  if (want_signals) {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::TypedArray<godot::Dictionary> signals =
        cd->class_get_signal_list(sn, false);
    for (int i = 0; i < signals.size(); i++) {
      godot::Dictionary d = signals[i];
      std::string sname;
      if (d.has("name")) {
        godot::String s = static_cast<godot::String>(d["name"]);
        sname = util::to_std(s);
      }
      if (!member_ok(sname)) {
        continue;
      }
      arr.PushBack(variant_to_json(signals[i]));
    }
    result["signals"] = std::move(arr);
  }
  if (want_enums) {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::PackedStringArray enum_names = cd->class_get_enum_list(sn, false);
    for (int i = 0; i < enum_names.size(); i++) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      std::string ename = util::to_std(enum_names[i]);
      e["name"] = mcp::JsonValue(ename);
      mcp::JsonValue ec(mcp::JsonValue::array_tag);
      int kept = 0;
      godot::PackedStringArray enum_consts =
          cd->class_get_enum_constants(sn, godot::StringName(ename.c_str()));
      for (int j = 0; j < enum_consts.size(); j++) {
        mcp::JsonValue cv(mcp::JsonValue::object_tag);
        std::string cname = util::to_std(enum_consts[j]);
        if (!member_ok(cname)) {
          continue;
        }
        cv["name"] = mcp::JsonValue(cname);
        cv["value"] =
            mcp::JsonValue(static_cast<int64_t>(cd->class_get_integer_constant(
                sn, godot::StringName(cname.c_str()))));
        ec.PushBack(std::move(cv));
        kept++;
      }
      if (!member_lower.empty() && !member_ok(ename) && kept == 0) {
        continue;
      }
      e["constants"] = std::move(ec);
      arr.PushBack(std::move(e));
    }
    result["enums"] = std::move(arr);
  }
  if (want_constants) {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    godot::PackedStringArray const_names =
        cd->class_get_integer_constant_list(sn, false);
    for (int i = 0; i < const_names.size(); i++) {
      mcp::JsonValue cv(mcp::JsonValue::object_tag);
      std::string cname = util::to_std(const_names[i]);
      if (!member_ok(cname)) {
        continue;
      }
      cv["name"] = mcp::JsonValue(cname);
      cv["value"] =
          mcp::JsonValue(static_cast<int64_t>(cd->class_get_integer_constant(
              sn, godot::StringName(cname.c_str()))));
      arr.PushBack(std::move(cv));
    }
    result["constants"] = std::move(arr);
  }
  result["note"] = mcp::JsonValue("class reference without docstrings");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_docs_class completed");
  return r;
}

mcp::JsonValue handle_search(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "find_docs_class called");
  auto *qp = args.Find("query");
  if (!qp || !qp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: query");
    return e;
  }
  std::string query = qp->GetString();
  auto *cd = godot::ClassDBSingleton::get_singleton();
  if (!cd) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDBSingleton not available");
    return e;
  }
  std::string query_lower = query;
  std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                 ::tolower);
  godot::PackedStringArray all_classes = cd->get_class_list();
  mcp::JsonValue results_arr(mcp::JsonValue::array_tag);
  int count = 0;
  for (int i = 0; i < all_classes.size() && count < 50; i++) {
    std::string name = util::to_std(all_classes[i]);
    std::string name_lower = name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                   ::tolower);
    if (name_lower.find(query_lower) != std::string::npos) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["name"] = mcp::JsonValue(name);
      item["parent"] = mcp::JsonValue(util::to_std(godot::String(
          cd->get_parent_class(godot::StringName(name.c_str())))));
      item["api_type"] = mcp::JsonValue(static_cast<int64_t>(
          cd->class_get_api_type(godot::StringName(name.c_str()))));
      results_arr.PushBack(std::move(item));
      count++;
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(results_arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "find_docs_class completed");
  return r;
}

mcp::JsonValue handle_get_method(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_docs_method called");
  auto *cp = args.Find("class");
  auto *mp = args.Find("method");
  if (!cp || !cp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: class");
    return e;
  }
  if (!mp || !mp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: method");
    return e;
  }
  std::string class_name = cp->GetString();
  std::string method_name = mp->GetString();
  auto *cd = godot::ClassDBSingleton::get_singleton();
  if (!cd) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDBSingleton not available");
    return e;
  }
  godot::StringName sn(class_name.c_str());
  if (!cd->class_exists(sn)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("class not found: " + class_name);
    return e;
  }
  godot::TypedArray<godot::Dictionary> methods =
      cd->class_get_method_list(sn, false);
  for (int i = 0; i < methods.size(); i++) {
    godot::Dictionary d = methods[i];
    if (d.has("name")) {
      godot::String mname = static_cast<godot::String>(d["name"]);
      if (util::to_std(mname) == method_name) {
        mcp::JsonValue result = variant_to_json(d);
        result["note"] = mcp::JsonValue("method signature without docstrings");
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = std::move(result);
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                  "get_docs_method completed");
        return r;
      }
    }
  }
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue("method not found: " + method_name +
                              " in class " + class_name);
  return e;
}

mcp::JsonValue handle_get_property(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_docs_property called");
  auto *cp = args.Find("class");
  auto *pp = args.Find("property");
  if (!cp || !cp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: class");
    return e;
  }
  if (!pp || !pp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
    return e;
  }
  std::string class_name = cp->GetString();
  std::string prop_name = pp->GetString();
  auto *cd = godot::ClassDBSingleton::get_singleton();
  if (!cd) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDBSingleton not available");
    return e;
  }
  godot::StringName sn(class_name.c_str());
  if (!cd->class_exists(sn)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("class not found: " + class_name);
    return e;
  }
  godot::TypedArray<godot::Dictionary> props =
      cd->class_get_property_list(sn, false);
  for (int i = 0; i < props.size(); i++) {
    godot::Dictionary d = props[i];
    if (d.has("name")) {
      godot::String pname = static_cast<godot::String>(d["name"]);
      if (util::to_std(pname) == prop_name) {
        mcp::JsonValue result = variant_to_json(d);
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = std::move(result);
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                  "get_docs_property completed");
        return r;
      }
    }
  }
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  if (prop_name.compare(0, 15, "theme_override_") == 0) {
    e["error"] = mcp::JsonValue(
        "property not found: " + prop_name + " in class " + class_name +
        " — theme_override_* values are per-node theme overrides, not "
        "ClassDB-declared properties. Query the live node with "
        "property_get_list {\"path\": \"<node-path>\", \"property_filter\": "
        "\"" +
        prop_name +
        "\"} to confirm the override exists on that node, then read it with "
        "property_get.");
    return e;
  }
  e["error"] = mcp::JsonValue("property not found: " + prop_name +
                              " in class " + class_name);
  return e;
}

} // namespace doc_ops
} // namespace godot_autopilot
