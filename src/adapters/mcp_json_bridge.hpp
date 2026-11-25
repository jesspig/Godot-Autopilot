#ifndef GODOT_AUTOPILOT_ADAPTERS_MCP_JSON_BRIDGE_HPP
#define GODOT_AUTOPILOT_ADAPTERS_MCP_JSON_BRIDGE_HPP

#include <optional>

#include <mcp/McpTypes.hpp>

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace adapters {

namespace sdk = mcp;

using SdkJson = sdk::JsonValue;
using JsonValue = godot_autopilot::util::JsonValue;
using SdkErrorCode = sdk::McpErrorCode;

JsonValue FromSdk(const SdkJson &value);
SdkJson ToSdk(const JsonValue &value);
std::optional<JsonValue> FromSdkOptional(const std::optional<SdkJson> &value);
std::optional<SdkJson> ToSdkOptional(const std::optional<JsonValue> &value);

} // namespace adapters
} // namespace godot_autopilot

#endif
