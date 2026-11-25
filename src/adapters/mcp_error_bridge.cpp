#include "adapters/mcp_error_bridge.hpp"

namespace godot_autopilot {
namespace adapters {

SdkError::SdkError(SdkErrorCode code, std::string message,
                   std::optional<JsonValue> data)
    : sdk::McpError(code, message, ToSdkOptional(data)),
      message_(std::move(message)) {}

[[noreturn]] void ThrowSdkError(SdkErrorCode code, std::string message,
                                std::optional<JsonValue> data) {
  throw SdkError(code, std::move(message), std::move(data));
}

} // namespace adapters
} // namespace godot_autopilot
