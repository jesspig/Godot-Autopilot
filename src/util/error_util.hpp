#ifndef GODOT_AUTOPILOT_ERROR_UTIL_HPP
#define GODOT_AUTOPILOT_ERROR_UTIL_HPP

#include <godot_cpp/variant/string.hpp>
#include <mcp/JsonValue.hpp>
#include <mcp/McpError.hpp>
#include <optional>
#include <string>
#include <utility>

namespace godot_autopilot {
namespace util {

mcp::JsonValue error_json(const std::string &msg);

class StableMcpError : public mcp::McpError {
public:
  StableMcpError(mcp::McpErrorCode code, std::string message,
                 std::optional<mcp::JsonValue> data = std::nullopt)
      : mcp::McpError(code, message, std::move(data)),
        message_(std::move(message)) {}

  const char *what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

[[noreturn]] inline void throw_mcp_error(
    mcp::McpErrorCode code, std::string message,
    std::optional<mcp::JsonValue> data = std::nullopt) {
  throw StableMcpError(code, std::move(message), std::move(data));
}

mcp::JsonValue ok_result(mcp::JsonValue value);

const mcp::JsonValue *find_result_or_data(const mcp::JsonValue &body);

mcp::JsonValue *find_result_or_data(mcp::JsonValue &body);

std::string to_std(const godot::String &s);

mcp::JsonValue error_detail(const std::string &fact,
                            const std::string &position,
                            const std::string &expected,
                            const std::string &action);

} // namespace util
} // namespace godot_autopilot

#endif
