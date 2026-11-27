#ifndef GODOT_AUTOPILOT_ERROR_UTIL_HPP
#define GODOT_AUTOPILOT_ERROR_UTIL_HPP

#include <godot_cpp/variant/string.hpp>
#include "util/json_value.hpp"
#include <string>
#include <utility>

namespace godot_autopilot {
namespace util {

godot_autopilot::util::JsonValue error_json(const std::string &msg);

godot_autopilot::util::JsonValue ok_result(godot_autopilot::util::JsonValue value);

const godot_autopilot::util::JsonValue *find_result_or_data(const godot_autopilot::util::JsonValue &body);

godot_autopilot::util::JsonValue *find_result_or_data(godot_autopilot::util::JsonValue &body);

std::string to_std(const godot::String &s);

godot_autopilot::util::JsonValue error_detail(const std::string &fact,
                            const std::string &position,
                            const std::string &expected,
                            const std::string &action);

} // namespace util
} // namespace godot_autopilot

#endif
