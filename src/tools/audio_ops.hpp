#ifndef GODOT_SELF_DRIVING_AUDIO_OPS_HPP
#define GODOT_SELF_DRIVING_AUDIO_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace audio_ops {

mcp::JsonValue handle_bus_get_layout(const mcp::JsonValue &args);
mcp::JsonValue handle_bus_set_layout(const mcp::JsonValue &args);
mcp::JsonValue handle_bus_get_count(const mcp::JsonValue &args);
mcp::JsonValue handle_bus_get_name(const mcp::JsonValue &args);
mcp::JsonValue handle_bus_set_volume(const mcp::JsonValue &args);
mcp::JsonValue handle_bus_set_mute(const mcp::JsonValue &args);
mcp::JsonValue handle_bus_set_bypass(const mcp::JsonValue &args);
mcp::JsonValue handle_effect_add(const mcp::JsonValue &args);
mcp::JsonValue handle_effect_remove(const mcp::JsonValue &args);
mcp::JsonValue handle_stream_play(const mcp::JsonValue &args);
mcp::JsonValue handle_stream_stop(const mcp::JsonValue &args);
mcp::JsonValue handle_stream_set_volume(const mcp::JsonValue &args);
mcp::JsonValue handle_stream_set_pitch(const mcp::JsonValue &args);
mcp::JsonValue handle_stream_get_playback_position(const mcp::JsonValue &args);
mcp::JsonValue handle_stream_seek(const mcp::JsonValue &args);
mcp::JsonValue handle_bus_set_solo(const mcp::JsonValue &args);
mcp::JsonValue handle_get_output_device_list(const mcp::JsonValue &args);
mcp::JsonValue handle_set_output_device(const mcp::JsonValue &args);
mcp::JsonValue handle_get_input_device_list(const mcp::JsonValue &args);
mcp::JsonValue handle_set_input_device(const mcp::JsonValue &args);

} // namespace audio_ops
} // namespace godot_self_driving

#endif
