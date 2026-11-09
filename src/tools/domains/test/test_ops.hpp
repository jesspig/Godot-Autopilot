#ifndef GODOT_AUTOPILOT_TEST_OPS_HPP
#define GODOT_AUTOPILOT_TEST_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace test_ops {

mcp::JsonValue handle_run_gdscript_tests(const mcp::JsonValue &args);
mcp::JsonValue handle_run_gdscript_test_files(const mcp::JsonValue &args);

} // namespace test_ops
} // namespace godot_autopilot
#endif
