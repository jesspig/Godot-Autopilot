#ifndef GODOT_SELF_DRIVING_DOC_OPS_HPP
#define GODOT_SELF_DRIVING_DOC_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace doc_ops {

mcp::JsonValue handle_get_class(const mcp::JsonValue& args);
mcp::JsonValue handle_search(const mcp::JsonValue& args);
mcp::JsonValue handle_get_method(const mcp::JsonValue& args);
mcp::JsonValue handle_get_property(const mcp::JsonValue& args);

} // namespace doc_ops
} // namespace godot_self_driving
#endif
