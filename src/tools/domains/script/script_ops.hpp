#ifndef GODOT_AUTOPILOT_SCRIPT_OPS_HPP
#define GODOT_AUTOPILOT_SCRIPT_OPS_HPP

#include "util/json_value.hpp"

#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace script_ops {

uint64_t fnv1a64(const std::string &bytes);
std::string fnv1a64_hex(const std::string &bytes);
inline const char *content_hash_algo_name() { return "fnv1a64"; }

bool try_compile_source(const std::string &source_code,
                        const std::string &path_for_cache,
                        std::string &error_out);

bool refresh_script_cache(const std::string &path, std::string &error_out);

godot_autopilot::util::JsonValue handle_execute_gdscript(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_load(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_attach_to_node(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_detach_from_node(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_property(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_property(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_call_function(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_reload(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_variable_list(const godot_autopilot::util::JsonValue &args);

} // namespace script_ops
} // namespace godot_autopilot

#endif
