#ifndef GODOT_AUTOPILOT_AUDIO_TOOLS_HPP
#define GODOT_AUTOPILOT_AUDIO_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/audio_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace audio_tools {

GDA_TOOL_CLASS(GetAudioBusLayoutTool, "get_audio_bus_layout",
               "Get the complete audio bus layout from the audio server, including the Master bus and every bus effect chain. Use this to inspect the current bus routing or to snapshot a layout before making changes. Returns the serialized AudioBusLayout object in 'result'. No parameters are required.",
               "Audio", std::vector<std::string>({"audio", "bus", "layout", "get"}), audio_ops::handle_bus_get_layout, false)

GDA_TOOL_CLASS(SetAudioBusLayoutTool, "set_audio_bus_layout",
               "Replace the whole audio bus layout of the audio server, including bus count, names, volume and effect chains. Use the object returned by get_audio_bus_layout as input, typically after editing the layout. Returns 'result' set to 'ok'. The change applies immediately to editor playback.",
               "Audio", std::vector<std::string>({"audio", "bus", "layout", "set"}), audio_ops::handle_bus_set_layout, false)

GDA_TOOL_CLASS(GetAudioBusCountTool, "get_audio_bus_count",
               "Get the number of audio buses currently defined, including the default Master bus which always occupies index 0. Use the returned count to bound 'bus_index' values passed to the other bus tools. Returns the count as an integer in 'result'.",
               "Audio", std::vector<std::string>({"audio", "bus", "count"}), audio_ops::handle_bus_get_count, false)

GDA_TOOL_CLASS(GetAudioBusNameTool, "get_audio_bus_name",
               "Get the name of an audio bus by its zero-based index. Index 0 is always the Master bus, and the index must be smaller than the count returned by get_audio_bus_count or an error is returned. Returns the bus name string in 'result'.",
               "Audio", std::vector<std::string>({"audio", "bus", "name"}), audio_ops::handle_bus_get_name, false)

GDA_TOOL_CLASS(SetAudioBusVolumeDbTool, "set_audio_bus_volume_db",
               "Set the volume of an audio bus in decibels, applied immediately to bus playback. Negative values attenuate and positive values amplify, with typical ranges from -80 (silence) to +24. Use set_audio_player_volume_db instead to adjust a single player node. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "bus", "volume"}), audio_ops::handle_bus_set_volume, false)

GDA_TOOL_CLASS(SetAudioBusMuteTool, "set_audio_bus_mute",
               "Set or clear the mute state of an audio bus. While 'muted' is true the bus produces no sound regardless of its volume; pass false to restore it. Returns 'result' set to 'ok'. Use set_audio_bus_bypass_effects when only the effect chain should be skipped.",
               "Audio", std::vector<std::string>({"audio", "bus", "mute"}), audio_ops::handle_bus_set_mute, false)

GDA_TOOL_CLASS(SetAudioBusBypassEffectsTool, "set_audio_bus_bypass_effects",
               "Enable or disable all effects on an audio bus without removing them. When 'bypass' is true the bus plays its raw sound and skips the whole effect chain, which stays in place for later re-enable. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "bus", "bypass"}), audio_ops::handle_bus_set_bypass, false)

GDA_TOOL_CLASS(AddAudioBusEffectTool, "add_audio_bus_effect",
               "Add a new audio effect to a bus. 'effect_type' must be the class name of an instantiable AudioEffect subclass, for example AudioEffectReverb or AudioEffectDistortion; unknown or abstract class names return an error. 'at_position' inserts the effect at that zero-based slot and defaults to -1, which appends at the end. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "effect", "add"}), audio_ops::handle_effect_add, false)

GDA_TOOL_CLASS(RemoveAudioBusEffectTool, "remove_audio_bus_effect",
               "Remove an audio effect from a bus using its zero-based index within the bus effect chain. An out-of-range 'effect_index' returns an error, and removing the last effect leaves the bus with a plain chain. Query the current chain with get_audio_bus_layout first. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "effect", "remove"}), audio_ops::handle_effect_remove, false)

GDA_TOOL_CLASS(PlayAudioPlayerTool, "play_audio_player",
               "Play a stream on an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. Optionally pass 'stream_path' to load an audio resource before playing, or 'from_position' to start at a time offset in seconds. If the node has no stream assigned and 'stream_path' is not given, an error is returned. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "stream", "play"}), audio_ops::handle_stream_play, true)

GDA_TOOL_CLASS(StopAudioPlayerTool, "stop_audio_player",
               "Stop playback on an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. The stream stays assigned, so the node can be played again with play_audio_player. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "stream", "stop"}), audio_ops::handle_stream_stop, false)

GDA_TOOL_CLASS(SetAudioPlayerVolumeDbTool, "set_audio_player_volume_db",
               "Set the volume of a single audio player node in decibels, applied on top of its bus volume. Negative values attenuate and positive values amplify. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. Use set_audio_bus_volume_db to adjust the whole bus instead. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "stream", "volume"}), audio_ops::handle_stream_set_volume, false)

GDA_TOOL_CLASS(SetAudioPlayerPitchScaleTool, "set_audio_player_pitch_scale",
               "Set the pitch of an audio player node, where 1.0 plays at normal speed. Values above 1.0 raise pitch and playback speed, values below 1.0 lower them. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "stream", "pitch"}), audio_ops::handle_stream_set_pitch, false)

GDA_TOOL_CLASS(GetAudioPlayerPlaybackPositionTool, "get_audio_player_playback_position",
               "Get the current playback position of an audio player node in seconds. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path, and must resolve to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D. Returns the position as a float in 'result'.",
               "Audio", std::vector<std::string>({"audio", "stream", "position"}), audio_ops::handle_stream_get_playback_position, false)

GDA_TOOL_CLASS(SeekAudioPlayerTool, "seek_audio_player",
               "Seek an audio player node to a playback position given in seconds. Seeking works while the node is playing or stopped and applies to the current stream. 'node_path' is a scene-relative path such as 'Level1/Player' or an absolute '/root/...' path. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "stream", "seek"}), audio_ops::handle_stream_seek, false)

GDA_TOOL_CLASS(SetAudioBusSoloTool, "set_audio_bus_solo",
               "Set the solo state of an audio bus. When 'solo' is true all other buses are muted so only this bus is heard; pass false to restore the full mix. Solo on several buses keeps exactly those audible. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "bus"}), audio_ops::handle_bus_set_solo, false)

GDA_TOOL_CLASS(GetAudioDeviceOutputsTool, "get_audio_device_outputs",
               "List the audio output devices available on this machine, such as speakers and headphones. Use the returned device name strings directly as the 'device' value for set_audio_device_output. Returns an array of device name strings in 'result'. The list reflects hardware attached to the system, not project settings.",
               "Audio", std::vector<std::string>({"audio", "device"}), audio_ops::handle_get_output_device_list, false)

GDA_TOOL_CLASS(SetAudioDeviceOutputTool, "set_audio_device_output",
               "Switch the audio output device of the editor process, for example to speakers or headphones. 'device' must be one of the names returned by get_audio_device_outputs; a running game process keeps its own device. List devices first to get valid names. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "device"}), audio_ops::handle_set_output_device, false)

GDA_TOOL_CLASS(GetAudioDeviceInputsTool, "get_audio_device_inputs",
               "List the audio input devices available on this machine, such as microphones. Use the returned device name strings directly as the 'device' value for set_audio_device_input. Returns an array of device name strings in 'result'. The list reflects hardware attached to the system, not project settings.",
               "Audio", std::vector<std::string>({"audio", "device"}), audio_ops::handle_get_input_device_list, false)

GDA_TOOL_CLASS(SetAudioDeviceInputTool, "set_audio_device_input",
               "Switch the audio input device of the editor process, for example to a different microphone. 'device' must be one of the names returned by get_audio_device_inputs; a running game process keeps its own device. List devices first to get valid names. Returns 'result' set to 'ok'.",
               "Audio", std::vector<std::string>({"audio", "device"}), audio_ops::handle_set_input_device, false)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(20);
  v.push_back(std::make_unique<GetAudioBusLayoutTool>());
  v.push_back(std::make_unique<SetAudioBusLayoutTool>());
  v.push_back(std::make_unique<GetAudioBusCountTool>());
  v.push_back(std::make_unique<GetAudioBusNameTool>());
  v.push_back(std::make_unique<SetAudioBusVolumeDbTool>());
  v.push_back(std::make_unique<SetAudioBusMuteTool>());
  v.push_back(std::make_unique<SetAudioBusBypassEffectsTool>());
  v.push_back(std::make_unique<AddAudioBusEffectTool>());
  v.push_back(std::make_unique<RemoveAudioBusEffectTool>());
  v.push_back(std::make_unique<PlayAudioPlayerTool>());
  v.push_back(std::make_unique<StopAudioPlayerTool>());
  v.push_back(std::make_unique<SetAudioPlayerVolumeDbTool>());
  v.push_back(std::make_unique<SetAudioPlayerPitchScaleTool>());
  v.push_back(std::make_unique<GetAudioPlayerPlaybackPositionTool>());
  v.push_back(std::make_unique<SeekAudioPlayerTool>());
  v.push_back(std::make_unique<SetAudioBusSoloTool>());
  v.push_back(std::make_unique<GetAudioDeviceOutputsTool>());
  v.push_back(std::make_unique<SetAudioDeviceOutputTool>());
  v.push_back(std::make_unique<GetAudioDeviceInputsTool>());
  v.push_back(std::make_unique<SetAudioDeviceInputTool>());
  return v;
}

} // namespace audio_tools
} // namespace godot_autopilot

#endif