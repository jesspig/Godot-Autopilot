#ifndef GODOT_AUTOPILOT_AUDIO_TOOLS_HPP
#define GODOT_AUTOPILOT_AUDIO_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/audio_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace audio_tools {

namespace {

const std::vector<ParamSpec> kGetAudioBusLayoutParams = {};

const std::vector<ParamSpec> kSetAudioBusLayoutParams = {
    {"layout", "object", "Serialized AudioBusLayout object as returned by get_audio_bus_layout, typically edited before applying", true},
};

const std::vector<ParamSpec> kGetAudioBusCountParams = {};

const std::vector<ParamSpec> kGetAudioBusNameParams = {
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus and the index must be below the count from get_audio_bus_count", true},
};

const std::vector<ParamSpec> kSetAudioBusVolumeDbParams = {
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
    {"volume_db", "number", "Volume in decibels; negative attenuates, positive amplifies (typical range -80 to +24)", true},
};

const std::vector<ParamSpec> kSetAudioBusMuteParams = {
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
    {"muted", "boolean", "Mute state; true silences the bus regardless of volume", true},
};

const std::vector<ParamSpec> kSetAudioBusBypassEffectsParams = {
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
    {"bypass", "boolean", "Bypass state; true skips all effects on the bus without removing them", true},
};

const std::vector<ParamSpec> kAddAudioBusEffectParams = {
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
    {"effect_type", "string", "Class name of an instantiable AudioEffect subclass, e.g. 'AudioEffectReverb' or 'AudioEffectDistortion'", true},
    {"at_position", "integer", "Zero-based slot to insert the effect at; -1 (default) appends at the end", false},
};

const std::vector<ParamSpec> kRemoveAudioBusEffectParams = {
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
    {"effect_index", "integer", "Zero-based index of the effect within the bus effect chain", true},
};

const std::vector<ParamSpec> kPlayAudioPlayerParams = {
    {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
    {"stream_path", "string", "Optional path to an audio resource file (.ogg, .mp3, .wav) to load and assign to the node before playing", false},
    {"from_position", "number", "Start playback from this time offset in seconds (default: 0.0)", false},
};

const std::vector<ParamSpec> kStopAudioPlayerParams = {
    {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
};

const std::vector<ParamSpec> kSetAudioPlayerVolumeDbParams = {
    {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
    {"volume_db", "number", "Volume in decibels; negative attenuates, positive amplifies (typical range -80 to +24)", true},
};

const std::vector<ParamSpec> kSetAudioPlayerPitchScaleParams = {
    {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
    {"pitch_scale", "number", "Pitch multiplier where 1.0 is normal speed; above 1.0 raises pitch and speed, below lowers them", true},
};

const std::vector<ParamSpec> kGetAudioPlayerPlaybackPositionParams = {
    {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
};

const std::vector<ParamSpec> kSeekAudioPlayerParams = {
    {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
    {"to_position", "number", "Playback position to seek to in seconds; must be within the stream length", true},
};

const std::vector<ParamSpec> kSetAudioBusSoloParams = {
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
    {"solo", "boolean", "Solo state; true mutes all other buses so only this one is heard", true},
};

const std::vector<ParamSpec> kGetAudioDeviceOutputsParams = {};

const std::vector<ParamSpec> kSetAudioDeviceOutputParams = {
    {"device", "string", "Output device name as returned by get_audio_device_outputs", true},
};

const std::vector<ParamSpec> kGetAudioDeviceInputsParams = {};

const std::vector<ParamSpec> kSetAudioDeviceInputParams = {
    {"device", "string", "Input device name as returned by get_audio_device_inputs", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(20);
  v.push_back(make_spec_tool(ToolSpec{
      "get_audio_bus_layout",
      "Get the complete audio bus layout from the audio server, including the Master bus and every bus effect chain. Use this to inspect the current bus routing or to snapshot a layout before making changes. Returns the serialized AudioBusLayout object in 'result'. No parameters are required.",
      "Audio", {"audio", "bus", "layout", "get"}, SideEffect::None, tool_flags::kNone,
      kGetAudioBusLayoutParams, audio_ops::handle_bus_get_layout}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_bus_layout",
      "Replace the whole audio bus layout of the audio server, including bus count, names, volume and effect chains. Use the object returned by get_audio_bus_layout as input, typically after editing the layout. Returns 'result' set to 'ok'. The change applies immediately to editor playback.",
      "Audio", {"audio", "bus", "layout", "set"}, SideEffect::None, tool_flags::kNone,
      kSetAudioBusLayoutParams, audio_ops::handle_bus_set_layout}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_audio_bus_count",
      "Get the number of audio buses currently defined, including the default Master bus which always occupies index 0. Use the returned count to bound 'bus_index' values passed to the other bus tools. Returns the count as an integer in 'result'.",
      "Audio", {"audio", "bus", "count"}, SideEffect::None, tool_flags::kNone,
      kGetAudioBusCountParams, audio_ops::handle_bus_get_count}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_audio_bus_name",
      "Get the name of an audio bus by its zero-based index. Index 0 is always the Master bus, and the index must be smaller than the count returned by get_audio_bus_count or an error is returned. Returns the bus name string in 'result'.",
      "Audio", {"audio", "bus", "name"}, SideEffect::None, tool_flags::kNone,
      kGetAudioBusNameParams, audio_ops::handle_bus_get_name}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_bus_volume_db",
      "Set the volume of an audio bus in decibels, applied immediately to bus playback. Negative values attenuate and positive values amplify, with typical ranges from -80 (silence) to +24. Use set_audio_player_volume_db instead to adjust a single player node. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "bus", "volume"}, SideEffect::None, tool_flags::kNone,
      kSetAudioBusVolumeDbParams, audio_ops::handle_bus_set_volume}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_bus_mute",
      "Set or clear the mute state of an audio bus. While 'muted' is true the bus produces no sound regardless of its volume; pass false to restore it. Returns 'result' set to 'ok'. Use set_audio_bus_bypass_effects when only the effect chain should be skipped.",
      "Audio", {"audio", "bus", "mute"}, SideEffect::None, tool_flags::kNone,
      kSetAudioBusMuteParams, audio_ops::handle_bus_set_mute}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_bus_bypass_effects",
      "Enable or disable all effects on an audio bus without removing them. When 'bypass' is true the bus plays its raw sound and skips the whole effect chain, which stays in place for later re-enable. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "bus", "bypass"}, SideEffect::None, tool_flags::kNone,
      kSetAudioBusBypassEffectsParams, audio_ops::handle_bus_set_bypass}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_audio_bus_effect",
      "Add a new audio effect to a bus. 'effect_type' must be the class name of an instantiable AudioEffect subclass, for example AudioEffectReverb or AudioEffectDistortion; unknown or abstract class names return an error. 'at_position' inserts the effect at that zero-based slot and defaults to -1, which appends at the end. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "effect", "add"}, SideEffect::None, tool_flags::kNone,
      kAddAudioBusEffectParams, audio_ops::handle_effect_add}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_audio_bus_effect",
      "Remove an audio effect from a bus using its zero-based index within the bus effect chain. An out-of-range 'effect_index' returns an error, and removing the last effect leaves the bus with a plain chain. Query the current chain with get_audio_bus_layout first. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "effect", "remove"}, SideEffect::None, tool_flags::kNone,
      kRemoveAudioBusEffectParams, audio_ops::handle_effect_remove}));
  v.push_back(make_spec_tool(ToolSpec{
      "play_audio_player",
      "Play a stream on an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. Optionally pass 'stream_path' to load an audio resource before playing, or 'from_position' to start at a time offset in seconds. If the node has no stream assigned and 'stream_path' is not given, an error is returned. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "stream", "play"}, SideEffect::None, tool_flags::kNone,
      kPlayAudioPlayerParams, audio_ops::handle_stream_play}));
  v.push_back(make_spec_tool(ToolSpec{
      "stop_audio_player",
      "Stop playback on an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. The stream stays assigned, so the node can be played again with play_audio_player. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "stream", "stop"}, SideEffect::None, tool_flags::kNone,
      kStopAudioPlayerParams, audio_ops::handle_stream_stop}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_player_volume_db",
      "Set the volume of a single audio player node in decibels, applied on top of its bus volume. Negative values attenuate and positive values amplify. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. Use set_audio_bus_volume_db to adjust the whole bus instead. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "stream", "volume"}, SideEffect::None, tool_flags::kNone,
      kSetAudioPlayerVolumeDbParams, audio_ops::handle_stream_set_volume}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_player_pitch_scale",
      "Set the pitch of an audio player node, where 1.0 plays at normal speed. Values above 1.0 raise pitch and playback speed, values below 1.0 lower them. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "stream", "pitch"}, SideEffect::None, tool_flags::kNone,
      kSetAudioPlayerPitchScaleParams, audio_ops::handle_stream_set_pitch}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_audio_player_playback_position",
      "Get the current playback position of an audio player node in seconds. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path, and must resolve to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D. Returns the position as a float in 'result'.",
      "Audio", {"audio", "stream", "position"}, SideEffect::None, tool_flags::kNone,
      kGetAudioPlayerPlaybackPositionParams, audio_ops::handle_stream_get_playback_position}));
  v.push_back(make_spec_tool(ToolSpec{
      "seek_audio_player",
      "Seek an audio player node to a playback position given in seconds. Seeking works while the node is playing or stopped and applies to the current stream. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "stream", "seek"}, SideEffect::None, tool_flags::kNone,
      kSeekAudioPlayerParams, audio_ops::handle_stream_seek}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_bus_solo",
      "Set the solo state of an audio bus. When 'solo' is true all other buses are muted so only this bus is heard; pass false to restore the full mix. Solo on several buses keeps exactly those audible. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "bus"}, SideEffect::None, tool_flags::kNone,
      kSetAudioBusSoloParams, audio_ops::handle_bus_set_solo}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_audio_device_outputs",
      "List the audio output devices available on this machine, such as speakers and headphones. Use the returned device name strings directly as the 'device' value for set_audio_device_output. Returns an array of device name strings in 'result'. The list reflects hardware attached to the system, not project settings.",
      "Audio", {"audio", "device"}, SideEffect::None, tool_flags::kNone,
      kGetAudioDeviceOutputsParams, audio_ops::handle_get_output_device_list}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_device_output",
      "Switch the audio output device of the editor process, for example to speakers or headphones. 'device' must be one of the names returned by get_audio_device_outputs; a running game process keeps its own device. List devices first to get valid names. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "device"}, SideEffect::None, tool_flags::kNone,
      kSetAudioDeviceOutputParams, audio_ops::handle_set_output_device}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_audio_device_inputs",
      "List the audio input devices available on this machine, such as microphones. Use the returned device name strings directly as the 'device' value for set_audio_device_input. Returns an array of device name strings in 'result'. The list reflects hardware attached to the system, not project settings.",
      "Audio", {"audio", "device"}, SideEffect::None, tool_flags::kNone,
      kGetAudioDeviceInputsParams, audio_ops::handle_get_input_device_list}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_audio_device_input",
      "Switch the audio input device of the editor process, for example to a different microphone. 'device' must be one of the names returned by get_audio_device_inputs; a running game process keeps its own device. List devices first to get valid names. Returns 'result' set to 'ok'.",
      "Audio", {"audio", "device"}, SideEffect::None, tool_flags::kNone,
      kSetAudioDeviceInputParams, audio_ops::handle_set_input_device}));
  return v;
}

} // namespace audio_tools
} // namespace godot_autopilot

#endif