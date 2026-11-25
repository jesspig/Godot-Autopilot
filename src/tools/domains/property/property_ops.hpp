#ifndef GODOT_AUTOPILOT_PROPERTY_OPS_HPP
#define GODOT_AUTOPILOT_PROPERTY_OPS_HPP

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>
#include "util/json_value.hpp"
#include <string>
#include <vector>

namespace godot_autopilot {
namespace property_ops {

struct InlineResourceBuild {
  bool active = false;
  bool has_error = false;
  godot_autopilot::util::JsonValue error;
  godot::Variant value;
  std::string type;
};

godot::Node *find_edited_scene_root();

InlineResourceBuild
build_inline_resource_value(const godot::Dictionary &prop_info,
                            const godot_autopilot::util::JsonValue &raw_value,
                            const std::string &node_property,
                            const std::string &path_str, int depth);

std::vector<std::string> find_property_family(godot::Object *object,
                                              const std::string &prefix,
                                              size_t limit = 16);

godot_autopilot::util::JsonValue handle_get(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_batch(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_transform(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_list(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_signal_connect(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_signal_disconnect(const godot_autopilot::util::JsonValue &args);

} // namespace property_ops
} // namespace godot_autopilot

#endif
