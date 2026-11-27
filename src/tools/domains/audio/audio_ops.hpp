#ifndef GODOT_AUTOPILOT_AUDIO_OPS_HPP
#define GODOT_AUTOPILOT_AUDIO_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace audio_ops {

godot_autopilot::util::JsonValue handle_audio_bus(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_audio_player(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_audio_device(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_audio_capture(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bus_info(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bus_metrics(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_input_status(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bus_get_layout(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bus_set_layout(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bus_set_volume(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bus_set_mute(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bus_set_bypass(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_effect_add(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_effect_remove(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_stream_play(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_stream_stop(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_stream_set_volume(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_stream_set_pitch(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_stream_get_playback_position(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_stream_seek(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bus_set_solo(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_output_device_list(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_output_device(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_input_device(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_start_input_capture(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_stop_input_capture(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_wav(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_input_devices(const godot_autopilot::util::JsonValue &args);

} // namespace audio_ops
} // namespace godot_autopilot

#endif
