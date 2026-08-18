#ifndef GODOT_AUTOPILOT_DISPLAY_OPS_HPP
#define GODOT_AUTOPILOT_DISPLAY_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace display_ops {

mcp::JsonValue handle_clipboard_get(const mcp::JsonValue &args);
mcp::JsonValue handle_clipboard_set(const mcp::JsonValue &args);
mcp::JsonValue handle_dialog_show(const mcp::JsonValue &args);
mcp::JsonValue handle_mouse_get_position(const mcp::JsonValue &args);
mcp::JsonValue handle_mouse_set_mode(const mcp::JsonValue &args);
mcp::JsonValue handle_mouse_warp(const mcp::JsonValue &args);
mcp::JsonValue handle_screen_capture(const mcp::JsonValue &args);
mcp::JsonValue handle_screen_get_count(const mcp::JsonValue &args);
mcp::JsonValue handle_screen_get_dpi(const mcp::JsonValue &args);
mcp::JsonValue handle_screen_get_position(const mcp::JsonValue &args);
mcp::JsonValue handle_screen_get_refresh_rate(const mcp::JsonValue &args);
mcp::JsonValue handle_screen_get_size(const mcp::JsonValue &args);
mcp::JsonValue handle_tts_get_voices(const mcp::JsonValue &args);
mcp::JsonValue handle_tts_speak(const mcp::JsonValue &args);
mcp::JsonValue handle_tts_stop(const mcp::JsonValue &args);
mcp::JsonValue handle_window_create(const mcp::JsonValue &args);
mcp::JsonValue handle_window_delete(const mcp::JsonValue &args);
mcp::JsonValue handle_window_move_to_foreground(const mcp::JsonValue &args);
mcp::JsonValue handle_window_request_attention(const mcp::JsonValue &args);
mcp::JsonValue handle_window_set_flag(const mcp::JsonValue &args);
mcp::JsonValue handle_window_set_mode(const mcp::JsonValue &args);
mcp::JsonValue handle_window_set_position(const mcp::JsonValue &args);
mcp::JsonValue handle_window_set_size(const mcp::JsonValue &args);
mcp::JsonValue handle_window_set_title(const mcp::JsonValue &args);

} // namespace display_ops
} // namespace godot_autopilot

#endif
