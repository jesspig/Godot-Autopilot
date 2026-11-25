#ifndef GODOT_AUTOPILOT_ADAPTERS_MCP_ERROR_BRIDGE_HPP
#define GODOT_AUTOPILOT_ADAPTERS_MCP_ERROR_BRIDGE_HPP

#include <mcp/McpError.hpp>

#include <optional>
#include <string>
#include <utility>

#include "adapters/mcp_json_bridge.hpp"

namespace godot_autopilot {
namespace adapters {

namespace sdk = mcp;

class SdkError : public sdk::McpError {
 public:
  SdkError(SdkErrorCode code, std::string message,
           std::optional<JsonValue> data = std::nullopt);

  const char *what() const noexcept override { return message_.c_str(); }

 private:
  std::string message_;
};

[[noreturn]] void ThrowSdkError(SdkErrorCode code, std::string message,
                                 std::optional<JsonValue> data = std::nullopt);

} // namespace adapters
} // namespace godot_autopilot

#endif
