#pragma once
#include <cstdint>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace capture_ops {

mcp::JsonValue handle_capture_viewport(const mcp::JsonValue &args);

std::string base64_encode(const uint8_t *data, size_t len);

} // namespace capture_ops
} // namespace godot_autopilot
