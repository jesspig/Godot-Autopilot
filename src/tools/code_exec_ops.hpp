#ifndef GODOT_SELF_DRIVING_CODE_EXEC_OPS_HPP
#define GODOT_SELF_DRIVING_CODE_EXEC_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace code_exec_ops {

mcp::JsonValue handle_batch_execute(const mcp::JsonValue& args);
mcp::JsonValue handle_code_execute(const mcp::JsonValue& args);

} // namespace code_exec_ops
} // namespace godot_self_driving
#endif
