#pragma once
#include <cstdint>
#include <godot_cpp/variant/string.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace capture_ops {

mcp::JsonValue handle_capture_viewport(const mcp::JsonValue &args);
mcp::JsonValue handle_review_scene(const mcp::JsonValue &args);

std::string base64_encode(const uint8_t *data, size_t len);

void prune_capture_files(const godot::String &dir_path, int keep);

} // namespace capture_ops
} // namespace godot_autopilot
