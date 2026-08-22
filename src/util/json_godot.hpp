#ifndef GODOT_AUTOPILOT_JSON_GODOT_HPP
#define GODOT_AUTOPILOT_JSON_GODOT_HPP

#include <cstdint>

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace util {

inline double json_number(const mcp::JsonValue *value, double fallback) {
  if (!value || !value->IsNumber())
    return fallback;
  return value->IsInt() ? static_cast<double>(value->GetInt())
                        : value->GetDouble();
}

inline godot::Vector2 json_to_vec2(const mcp::JsonValue &j) {
  auto *x = j.Find("x");
  auto *y = j.Find("y");
  return godot::Vector2(
      static_cast<float>(json_number(x, 0.0)),
      static_cast<float>(json_number(y, 0.0)));
}

inline godot::Vector3 json_to_vec3(const mcp::JsonValue &j) {
  auto *x = j.Find("x");
  auto *y = j.Find("y");
  auto *z = j.Find("z");
  return godot::Vector3(
      static_cast<float>(json_number(x, 0.0)),
      static_cast<float>(json_number(y, 0.0)),
      static_cast<float>(json_number(z, 0.0)));
}

inline godot::Rect2 json_to_rect2(const mcp::JsonValue &j) {
  return godot::Rect2(json_to_vec2(j.At("position")),
                      json_to_vec2(j.At("size")));
}

inline godot::Color json_to_color(const mcp::JsonValue &j) {
  auto *r = j.Find("r");
  auto *g = j.Find("g");
  auto *b = j.Find("b");
  auto *a = j.Find("a");
  return godot::Color(
      static_cast<float>(json_number(r, 0.0)),
      static_cast<float>(json_number(g, 0.0)),
      static_cast<float>(json_number(b, 0.0)),
      static_cast<float>(json_number(a, 1.0)));
}

inline mcp::JsonValue vec2_to_json(const godot::Vector2 &v) {
  return mcp::JsonValue::FromObject(
      {{"x", mcp::JsonValue(v.x)}, {"y", mcp::JsonValue(v.y)}});
}

inline mcp::JsonValue vec3_to_json(const godot::Vector3 &v) {
  return mcp::JsonValue::FromObject({{"x", mcp::JsonValue(v.x)},
                                     {"y", mcp::JsonValue(v.y)},
                                     {"z", mcp::JsonValue(v.z)}});
}

inline godot::RID rid_from_json(const mcp::JsonValue &j) {
  return godot::UtilityFunctions::rid_from_int64(j.GetInt());
}

inline mcp::JsonValue rid_to_json(const godot::RID &rid) {
  return mcp::JsonValue::FromObject(
      {{"rid", mcp::JsonValue(static_cast<int64_t>(rid.get_id()))}});
}

} // namespace util
} // namespace godot_autopilot

#endif
