#ifndef GODOT_AUTOPILOT_VARIANT_JSON_HPP
#define GODOT_AUTOPILOT_VARIANT_JSON_HPP

#include <godot_cpp/variant/variant.hpp>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {

bool node_ref_path_is_safe(const std::string &path);

struct VariantJson {
  static mcp::JsonValue serialize(const godot::Variant &v);
  static godot::Variant deserialize(const mcp::JsonValue &j,
                                    const std::string &type_hint = "");
  static godot::Variant deserialize_strict(const mcp::JsonValue &j,
                                           const std::string &type_hint);
};

} // namespace godot_autopilot

#endif
