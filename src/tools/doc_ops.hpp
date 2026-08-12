#ifndef GODOT_AUTOPILOT_DOC_OPS_HPP
#define GODOT_AUTOPILOT_DOC_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace doc_ops {

mcp::JsonValue handle_get_class(const mcp::JsonValue &args);
mcp::JsonValue handle_search(const mcp::JsonValue &args);
mcp::JsonValue handle_get_method(const mcp::JsonValue &args);
mcp::JsonValue handle_get_property(const mcp::JsonValue &args);

} // namespace doc_ops
} // namespace godot_autopilot
#endif
