#ifndef GODOT_SELF_DRIVING_SCRIPT_OPS_HPP
#define GODOT_SELF_DRIVING_SCRIPT_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace script_ops {

mcp::JsonValue handle_execute_gdscript(const mcp::JsonValue& args);
mcp::JsonValue handle_load(const mcp::JsonValue& args);
mcp::JsonValue handle_create(const mcp::JsonValue& args);
mcp::JsonValue handle_attach_to_node(const mcp::JsonValue& args);
mcp::JsonValue handle_detach_from_node(const mcp::JsonValue& args);
mcp::JsonValue handle_get_property(const mcp::JsonValue& args);
mcp::JsonValue handle_set_property(const mcp::JsonValue& args);
mcp::JsonValue handle_call_function(const mcp::JsonValue& args);
mcp::JsonValue handle_reload(const mcp::JsonValue& args);
mcp::JsonValue handle_get_variable_list(const mcp::JsonValue& args);

} // namespace script_ops
} // namespace godot_self_driving

#endif
