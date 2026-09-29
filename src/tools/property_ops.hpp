#ifndef GODOT_AUTOPILOT_PROPERTY_OPS_HPP
#define GODOT_AUTOPILOT_PROPERTY_OPS_HPP

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace property_ops {

struct InlineResourceBuild {
  bool active = false;
  bool has_error = false;
  mcp::JsonValue error;
  godot::Variant value;
  std::string type;
};

InlineResourceBuild
build_inline_resource_value(const godot::Dictionary &prop_info,
                            const mcp::JsonValue &raw_value,
                            const std::string &node_property,
                            const std::string &path_str, int depth);

std::vector<std::string> find_property_family(godot::Object *object,
                                              const std::string &prefix,
                                              size_t limit = 16);

mcp::JsonValue handle_get(const mcp::JsonValue &args);
mcp::JsonValue handle_set(const mcp::JsonValue &args);
mcp::JsonValue handle_get_list(const mcp::JsonValue &args);
mcp::JsonValue handle_signal_connect(const mcp::JsonValue &args);
mcp::JsonValue handle_signal_disconnect(const mcp::JsonValue &args);

} // namespace property_ops
} // namespace godot_autopilot

#endif
