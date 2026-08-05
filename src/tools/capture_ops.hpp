#pragma once
#include <mcp/JsonValue.hpp>
#include <cstdint>
#include <string>

namespace godot_self_driving {
namespace capture_ops {

mcp::JsonValue handle_capture_viewport(const mcp::JsonValue& args);

std::string base64_encode(const uint8_t* data, size_t len);

} // namespace capture_ops
} // namespace godot_self_driving
