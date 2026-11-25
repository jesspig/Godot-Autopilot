#ifndef GODOT_AUTOPILOT_TEST_OPS_HPP
#define GODOT_AUTOPILOT_TEST_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace test_ops {

godot_autopilot::util::JsonValue handle_run_gdscript_tests(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_run_gdscript_test_files(const godot_autopilot::util::JsonValue &args);

} // namespace test_ops
} // namespace godot_autopilot
#endif
