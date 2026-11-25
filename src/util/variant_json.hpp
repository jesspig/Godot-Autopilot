#ifndef GODOT_AUTOPILOT_VARIANT_JSON_HPP
#define GODOT_AUTOPILOT_VARIANT_JSON_HPP

#include <godot_cpp/variant/variant.hpp>
#include "util/json_value.hpp"

namespace godot_autopilot {

bool node_ref_path_is_safe(const std::string &path);

struct VariantJson {
  static godot_autopilot::util::JsonValue serialize(const godot::Variant &v);
  static godot::Variant deserialize(const godot_autopilot::util::JsonValue &j,
                                    const std::string &type_hint = "");
  static godot::Variant deserialize_strict(const godot_autopilot::util::JsonValue &j,
                                           const std::string &type_hint);
};

} // namespace godot_autopilot

#endif
