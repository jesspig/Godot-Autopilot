#ifndef GODOT_SELF_DRIVING_ERROR_UTIL_HPP
#define GODOT_SELF_DRIVING_ERROR_UTIL_HPP

#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_self_driving {
namespace util {

mcp::JsonValue error_json(const std::string &msg);

mcp::JsonValue error_detail(const std::string &fact,
                            const std::string &position,
                            const std::string &expected,
                            const std::string &action);

} // namespace util
} // namespace godot_self_driving

#endif
