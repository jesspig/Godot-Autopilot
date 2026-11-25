#ifndef GODOT_AUTOPILOT_DISPLAY_OPS_HPP
#define GODOT_AUTOPILOT_DISPLAY_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace display_ops {

godot_autopilot::util::JsonValue handle_clipboard_get(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_clipboard_set(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_dialog_show(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mouse_get_position(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mouse_set_mode(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mouse_warp(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_screen_capture(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_screen_get_count(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_screen_get_dpi(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_screen_get_position(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_screen_get_refresh_rate(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_screen_get_size(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tts_get_voices(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tts_speak(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tts_stop(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tts_pause(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tts_resume(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tts_is_speaking(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_delete(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_get_rect(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_popup(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_popup_centered(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_set_transient(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_set_exclusive(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_set_unparent_when_invisible(
    const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_close(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_move_to_foreground(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_request_attention(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_set_flag(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_set_mode(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_set_position(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_set_size(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_set_title(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_info_get(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_window_control(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mouse_control(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tts_control(const godot_autopilot::util::JsonValue &args);

} // namespace display_ops
} // namespace godot_autopilot

#endif
