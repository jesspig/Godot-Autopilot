#ifndef GODOT_AUTOPILOT_INLINE_RESOURCE_JSON_HPP
#define GODOT_AUTOPILOT_INLINE_RESOURCE_JSON_HPP

#include <mcp/JsonValue.hpp>
#include <algorithm>
#include <string>

namespace godot_autopilot {
namespace util {
namespace inline_resource {

constexpr int kMaxDepth = 4;

enum class Shape { not_inline, inline_description, invalid_properties };

struct Description {
  Shape shape = Shape::not_inline;
  std::string type;
  bool has_properties = false;
};

inline Description inspect(const mcp::JsonValue &value) {
  Description result;
  if (!value.IsObject()) {
    return result;
  }
  const mcp::JsonValue *type = value.Find("type");
  if (type == nullptr || !type->IsString() || type->GetString().empty()) {
    return result;
  }
  result.type = type->GetString();
  const mcp::JsonValue *properties = value.Find("properties");
  if (properties == nullptr) {
    result.shape = Shape::inline_description;
    return result;
  }
  if (!properties->IsObject()) {
    result.shape = Shape::invalid_properties;
    return result;
  }
  result.shape = Shape::inline_description;
  result.has_properties = true;
  return result;
}

inline int nested_depth(const mcp::JsonValue &value) {
  if (value.IsArray()) {
    int deepest = 0;
    for (const mcp::JsonValue &element : value.GetArray()) {
      deepest = std::max(deepest, nested_depth(element));
    }
    return deepest;
  }
  if (!value.IsObject()) {
    return 0;
  }
  const Description description = inspect(value);
  int deepest = 0;
  for (auto it = value.begin(); it != value.end(); ++it) {
    if (description.shape != Shape::not_inline && it->first == "type") {
      continue;
    }
    deepest = std::max(deepest, nested_depth(it->second));
  }
  if (description.shape == Shape::inline_description) {
    return 1 + deepest;
  }
  return deepest;
}

} // namespace inline_resource
} // namespace util
} // namespace godot_autopilot

#endif
