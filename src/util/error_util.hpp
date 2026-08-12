#ifndef GODOT_AUTOPILOT_ERROR_UTIL_HPP
#define GODOT_AUTOPILOT_ERROR_UTIL_HPP

#include <godot_cpp/variant/string.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace util {

mcp::JsonValue error_json(const std::string &msg);

mcp::JsonValue ok_result(mcp::JsonValue value);

std::string to_std(const godot::String &s);

mcp::JsonValue error_detail(const std::string &fact,
                            const std::string &position,
                            const std::string &expected,
                            const std::string &action);

} // namespace util
} // namespace godot_autopilot

#endif
