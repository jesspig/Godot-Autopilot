#ifndef GODOT_AUTOPILOT_AUDIO_TOOLS_HPP
#define GODOT_AUTOPILOT_AUDIO_TOOLS_HPP

#include <functional>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/domains/audio/audio_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace audio_tools {

namespace {

const std::vector<ParamSpec> kAudioBusParams = {
    {"layout", "object", "Serialized AudioBusLayout object for action='set_layout', as returned by action='get_layout', typically edited before applying", false},
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus; used by action='get_info' (optional) and required by the per-bus setter actions", false},
    {"volume_db", "number", "Volume in decibels for action='set_volume'; negative attenuates, positive amplifies (typical range -80 to +24)", false},
    {"muted", "boolean", "Mute state for action='set_mute'; true silences the bus regardless of volume", false},
    {"solo", "boolean", "Solo state for action='set_solo'; true mutes all other buses so only this one is heard", false},
    {"bypass", "boolean", "Bypass state for action='set_bypass'; true skips all effects on the bus without removing them", false},
    {"effect_type", "string", "Class name of an instantiable AudioEffect subclass for action='add_effect', e.g. 'AudioEffectReverb' or 'AudioEffectDistortion'", false},
    {"at_position", "integer", "Zero-based slot for action='add_effect'; -1 (default) appends at the end", false},
};

const std::vector<ParamSpec> kAudioPlayerParams = {
    {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
    {"stream_path", "string", "Path to an audio resource file (.ogg, .mp3, .wav) to load and assign before action='play'", false},
    {"from_position", "number", "Start playback offset in seconds for action='play' (default: 0.0)", false},
    {"volume_db", "number", "Volume in decibels for action='set_volume'; negative attenuates, positive amplifies (typical range -80 to +24)", false},
    {"pitch_scale", "number", "Pitch multiplier for action='set_pitch' where 1.0 is normal speed; above 1.0 raises pitch and speed, below lowers them", false},
    {"to_position", "number", "Playback position in seconds for action='seek'; must be within the stream length", false},
};

const std::vector<ParamSpec> kAudioDeviceParams = {
    {"device", "string", "Output device name for action='set_output' or input device name for action='set_input', as returned by the matching list action", false},
};

const std::vector<ParamSpec> kRemoveAudioBusEffectParams = {
    {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
    {"effect_index", "integer", "Zero-based index of the effect within the bus effect chain", true},
};

const std::vector<ParamSpec> kAudioCaptureParams = {
    {"device", "string", "Optional input device name for action='start', as returned by audio_device action='list_inputs'; when given, the device is selected before capture starts", false},
    {"name", "string", "Registration name for the captured or created take; the resulting AudioStreamWAV stays alive as 'memory://name' for later tools", false},
    {"format", "integer", "Sample format in bits; 8 or 16 (default 16); any other value is rejected", false},
    {"mix_rate", "integer", "Mix rate in Hz stored on the WAV (default: current input mix rate for action='stop', 44100 for action='create_wav'); must be an integer in 1000..192000", false},
    {"stereo", "boolean", "Capture both channels interleaved when true (default false keeps the left channel only); for action='create_wav' treats samples as flat interleaved L/R pairs", false},
    {"max_frames", "integer", "Maximum frames to drain from the input buffer for action='stop' (default: all available); must be at least 1; captures longer than 1000000 frames keep only the first 1000000 and report truncated true", false},
    {"samples", "array", "Audio samples for action='create_wav' as floats in [-1, 1] (out-of-range values are clamped); mono samples when stereo is false, flat interleaved L/R pairs when stereo is true; total frames must be between 1 and 1000000", false},
};

inline std::vector<std::string> audio_actions(
    std::initializer_list<const char *> actions) {
  std::vector<std::string> out;
  out.reserve(actions.size());
  for (const char *action : actions) {
    out.emplace_back(action);
  }
  return out;
}

inline std::vector<std::string> audio_examples(
    std::initializer_list<const char *> examples) {
  std::vector<std::string> out;
  out.reserve(examples.size());
  for (const char *example : examples) {
    out.emplace_back(example);
  }
  return out;
}

inline ToolSpec audio_action_spec(ToolSpec spec,
                                  std::initializer_list<const char *> actions,
                                  std::initializer_list<const char *> examples) {
  spec.raw_schema = schema::build_action_schema(
      audio_actions(actions), spec.params,
      "Action to perform; per-action parameters are documented in the tool "
      "description");
  spec.examples = audio_examples(examples);
  return spec;
}

inline mcp::JsonValue audio_args_with_action(const mcp::JsonValue &args,
                                             const char *action) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline bool audio_result_has_error(const mcp::JsonValue &result) {
  return result.IsObject() &&
         (result.Find("error") != nullptr ||
          result.Find("structured_error") != nullptr ||
          result.Find("runtime_error") != nullptr);
}

inline ToolAlias audio_alias(const char *name, const char *canonical,
                             const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return audio_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

inline mcp::JsonValue validate_audio_bus_layout_alias(
    const mcp::JsonValue &args) {
  const mcp::JsonValue *action = args.Find("action");
  if (action == nullptr) {
    return mcp::JsonValue();
  }
  if (!action->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] =
        mcp::JsonValue("invalid parameter: action must be layout, count or name");
    return error;
  }
  const std::string view = action->GetString();
  if (view != "layout" && view != "count" && view != "name") {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue(
        "unknown action '" + view +
        "' for get_audio_bus_layout (expected layout, count, name)");
    return error;
  }
  if (view == "name") {
    const mcp::JsonValue *index = args.Find("bus_index");
    if (index == nullptr || !index->IsInt()) {
      mcp::JsonValue error(mcp::JsonValue::object_tag);
      error["error"] = mcp::JsonValue("missing required parameter: bus_index");
      return error;
    }
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue project_audio_bus_layout_alias(
    const mcp::JsonValue &result, const mcp::JsonValue &args) {
  if (audio_result_has_error(result)) {
    return result;
  }
  std::string view = "layout";
  const mcp::JsonValue *action = args.Find("action");
  if (action != nullptr && action->IsString()) {
    view = action->GetString();
  }
  const mcp::JsonValue *inner = result.Find("result");
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (view == "count") {
    const mcp::JsonValue *count =
        inner != nullptr && inner->IsObject() ? inner->Find("count") : nullptr;
    out["result"] =
        count != nullptr ? *count : mcp::JsonValue(static_cast<int64_t>(0));
    return out;
  }
  if (view == "name") {
    const mcp::JsonValue *bus =
        inner != nullptr && inner->IsObject() ? inner->Find("bus") : nullptr;
    const mcp::JsonValue *name =
        bus != nullptr && bus->IsObject() ? bus->Find("name") : nullptr;
    out["result"] = name != nullptr ? *name : mcp::JsonValue(std::string());
    return out;
  }
  out["result"] = inner != nullptr ? *inner : mcp::JsonValue();
  return out;
}

inline mcp::JsonValue project_audio_bus_count(const mcp::JsonValue &result) {
  if (audio_result_has_error(result)) {
    return result;
  }
  const mcp::JsonValue *inner = result.Find("result");
  const mcp::JsonValue *count =
      inner != nullptr && inner->IsObject() ? inner->Find("count") : nullptr;
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] =
      count != nullptr ? *count : mcp::JsonValue(static_cast<int64_t>(0));
  return out;
}

inline mcp::JsonValue project_audio_bus_name(const mcp::JsonValue &result) {
  if (audio_result_has_error(result)) {
    return result;
  }
  const mcp::JsonValue *inner = result.Find("result");
  const mcp::JsonValue *bus =
      inner != nullptr && inner->IsObject() ? inner->Find("bus") : nullptr;
  const mcp::JsonValue *name =
      bus != nullptr && bus->IsObject() ? bus->Find("name") : nullptr;
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = name != nullptr ? *name : mcp::JsonValue(std::string());
  return out;
}

inline mcp::JsonValue validate_audio_bus_name_alias(
    const mcp::JsonValue &args) {
  const mcp::JsonValue *index = args.Find("bus_index");
  if (index == nullptr || !index->IsInt()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: bus_index");
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue project_audio_input_devices(
    const mcp::JsonValue &result) {
  if (audio_result_has_error(result)) {
    return result;
  }
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *inner = result.Find("result");
  const mcp::JsonValue *devices =
      inner != nullptr && inner->IsObject() ? inner->Find("devices") : nullptr;
  if (devices != nullptr) {
    out["result"] = *devices;
  } else {
    out["error"] = mcp::JsonValue("devices not available");
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(make_spec_tool(with_aliases(
      audio_action_spec(
          ToolSpec{
              "audio_bus",
              "Audio bus management, actions: get_layout (default serialized AudioBusLayout with every effect chain), set_layout (replace the whole layout), get_info (bus count, all bus names and, with bus_index, one bus volume/mute/solo/bypass/effect count), set_volume (decibels), set_mute, set_solo, set_bypass (skip effects without removing them), add_effect (instantiate an AudioEffect subclass into a bus slot). Every change applies immediately to editor playback. Requires action plus the parameters named by that action; bus_index 0 is Master. Deprecated aliases: get_audio_bus_layout, get_audio_bus_count, get_audio_bus_name, set_audio_bus_layout, set_audio_bus_volume_db, set_audio_bus_mute, set_audio_bus_solo, set_audio_bus_bypass_effects, add_audio_bus_effect.",
              "Audio",
              {"audio", "bus", "layout", "get_audio_bus_layout",
               "get_audio_bus_count", "get_audio_bus_name",
               "set_audio_bus_layout", "set_audio_bus_volume_db",
               "set_audio_bus_mute", "set_audio_bus_solo",
               "set_audio_bus_bypass_effects", "add_audio_bus_effect",
               "volume", "mute", "solo", "bypass", "effect"},
              SideEffect::None,
              tool_flags::kNone,
              kAudioBusParams,
              audio_ops::handle_audio_bus},
          {"get_layout", "set_layout", "get_info", "set_volume", "set_mute",
           "set_solo", "set_bypass", "add_effect"},
          {"{\"action\":\"get_layout\"}",
           "{\"action\":\"set_layout\",\"layout\":{\"class\":\"AudioBusLayout\"}}",
           "{\"action\":\"get_info\",\"bus_index\":0}",
           "{\"action\":\"set_volume\",\"bus_index\":1,\"volume_db\":-6.0}",
           "{\"action\":\"set_mute\",\"bus_index\":1,\"muted\":true}",
           "{\"action\":\"set_solo\",\"bus_index\":1,\"solo\":true}",
           "{\"action\":\"set_bypass\",\"bus_index\":1,\"bypass\":true}",
           "{\"action\":\"add_effect\",\"bus_index\":0,\"effect_type\":\"AudioEffectReverb\",\"at_position\":-1}"}),
      {
          [](ToolAlias alias) {
            alias.adapt_args = [](const mcp::JsonValue &args) {
              mcp::JsonValue out = args.IsObject()
                                       ? args
                                       : mcp::JsonValue(
                                             mcp::JsonValue::object_tag);
              const mcp::JsonValue *legacy_action = args.Find("action");
              const bool count_view =
                  legacy_action != nullptr && legacy_action->IsString() &&
                  legacy_action->GetString() == "count";
              const bool bus_view =
                  count_view ||
                  (legacy_action != nullptr && legacy_action->IsString() &&
                   legacy_action->GetString() == "name");
              if (count_view) {
                out.GetObject().erase("bus_index");
              }
              out["action"] = mcp::JsonValue(
                  bus_view ? "get_info" : "get_layout");
              return out;
            };
            alias.validate_args = validate_audio_bus_layout_alias;
            alias.project_result_with_args = project_audio_bus_layout_alias;
            return alias;
          }(audio_alias("get_audio_bus_layout", "audio_bus", "get_layout")),
          [](ToolAlias alias) {
            alias.project_result = project_audio_bus_count;
            return alias;
          }(audio_alias("get_audio_bus_count", "audio_bus", "get_info")),
          [](ToolAlias alias) {
            alias.validate_args = validate_audio_bus_name_alias;
            alias.project_result = project_audio_bus_name;
            return alias;
          }(audio_alias("get_audio_bus_name", "audio_bus", "get_info")),
          audio_alias("set_audio_bus_layout", "audio_bus", "set_layout"),
          audio_alias("set_audio_bus_volume_db", "audio_bus", "set_volume"),
          audio_alias("set_audio_bus_mute", "audio_bus", "set_mute"),
          audio_alias("set_audio_bus_solo", "audio_bus", "set_solo"),
          audio_alias("set_audio_bus_bypass_effects", "audio_bus",
                      "set_bypass"),
          audio_alias("add_audio_bus_effect", "audio_bus", "add_effect"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      audio_action_spec(
          ToolSpec{
              "audio_player",
              "Playback control for AudioStreamPlayer, AudioStreamPlayer2D and AudioStreamPlayer3D nodes, actions: play (optionally load stream_path first and start at from_position seconds), stop (keeps the stream assigned), set_volume (decibels on top of the bus volume), set_pitch (1.0 is normal speed), seek (to_position seconds, playing or stopped), get_position (current playback position in seconds). Requires node_path (scene-relative like 'Level1/Player' or absolute '/root/...') plus the parameters named by the action. Deprecated aliases: play_audio_player, stop_audio_player, set_audio_player_volume_db, set_audio_player_pitch_scale, seek_audio_player, get_audio_player_playback_position.",
              "Audio",
              {"audio", "stream", "player", "play_audio_player",
               "stop_audio_player", "set_audio_player_volume_db",
               "set_audio_player_pitch_scale", "seek_audio_player",
               "get_audio_player_playback_position", "volume", "pitch",
               "seek", "position"},
              SideEffect::None,
              tool_flags::kNone,
              kAudioPlayerParams,
              audio_ops::handle_audio_player},
          {"play", "stop", "set_volume", "set_pitch", "seek", "get_position"},
          {"{\"action\":\"play\",\"node_path\":\"Level1/Player\",\"from_position\":0.0}",
           "{\"action\":\"stop\",\"node_path\":\"Level1/Player\"}",
           "{\"action\":\"set_volume\",\"node_path\":\"Level1/Player\",\"volume_db\":-3.0}",
           "{\"action\":\"set_pitch\",\"node_path\":\"Level1/Player\",\"pitch_scale\":1.2}",
           "{\"action\":\"seek\",\"node_path\":\"Level1/Player\",\"to_position\":5.0}",
           "{\"action\":\"get_position\",\"node_path\":\"Level1/Player\"}"}),
      {
          audio_alias("play_audio_player", "audio_player", "play"),
          audio_alias("stop_audio_player", "audio_player", "stop"),
          audio_alias("set_audio_player_volume_db", "audio_player",
                      "set_volume"),
          audio_alias("set_audio_player_pitch_scale", "audio_player",
                      "set_pitch"),
          audio_alias("seek_audio_player", "audio_player", "seek"),
          audio_alias("get_audio_player_playback_position", "audio_player",
                      "get_position"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      audio_action_spec(
          ToolSpec{
              "audio_device",
              "Audio device management for the editor process, actions: list_outputs (array of output device names such as speakers and headphones), set_output (switch the editor output device), list_inputs (input device names plus current device, input_mix_rate and frames_available), set_input (switch the editor input device), get_input_status (current input device, input_mix_rate and frames_available without the device list). Device names returned by list_outputs and list_inputs are the valid values for set_output and set_input. A running game keeps its own devices. Deprecated aliases: get_audio_device_outputs, set_audio_device_output, get_audio_input_devices, get_audio_device_inputs, set_audio_device_input.",
              "Audio",
              {"audio", "device", "output", "input", "get_audio_device_outputs",
               "set_audio_device_output", "get_audio_input_devices",
               "get_audio_device_inputs", "set_audio_device_input"},
              SideEffect::None,
              tool_flags::kNone,
              kAudioDeviceParams,
              audio_ops::handle_audio_device},
          {"list_outputs", "set_output", "list_inputs", "set_input",
           "get_input_status"},
          {"{\"action\":\"list_outputs\"}",
           "{\"action\":\"set_output\",\"device\":\"Speakers\"}",
           "{\"action\":\"list_inputs\"}",
           "{\"action\":\"set_input\",\"device\":\"Microphone\"}",
           "{\"action\":\"get_input_status\"}"}),
      {
          audio_alias("get_audio_device_outputs", "audio_device",
                      "list_outputs"),
          audio_alias("set_audio_device_output", "audio_device", "set_output"),
          audio_alias("get_audio_input_devices", "audio_device",
                      "list_inputs"),
          audio_alias("set_audio_device_input", "audio_device", "set_input"),
          [](ToolAlias alias) {
            alias.project_result = project_audio_input_devices;
            return alias;
          }(audio_alias("get_audio_device_inputs", "audio_device",
                        "list_inputs")),
      })));
  v.push_back(make_spec_tool(with_aliases(
      audio_action_spec(
          ToolSpec{
              "audio_capture",
              "Audio input capture and in-memory WAV creation, actions: start (begin capturing the input device, optionally selecting device first), stop (drain the buffer into an AudioStreamWAV registered as 'memory://name'; optional format 8/16, mix_rate, stereo and max_frames, with 'truncated' when more than 1000000 frames were requested), create_wav (build an AudioStreamWAV from inline samples and register it as 'memory://name'; format 8/16, mix_rate default 44100, stereo means flat interleaved L/R pairs). WAV locators carry class, name, path, object_id, format, mix_rate, stereo and frames; save them to disk before assigning to node properties. Deprecated aliases: start_audio_input_capture, stop_audio_input_capture, create_audiostream_wav.",
              "Audio",
              {"audio", "input", "capture", "wav",
               "start_audio_input_capture", "stop_audio_input_capture",
               "create_audiostream_wav"},
              SideEffect::None,
              tool_flags::kNone,
              kAudioCaptureParams,
              audio_ops::handle_audio_capture},
          {"start", "stop", "create_wav"},
          {"{\"action\":\"start\"}",
           "{\"action\":\"stop\",\"name\":\"take1\",\"format\":16}",
           "{\"action\":\"create_wav\",\"name\":\"beep\",\"samples\":[0.0,0.5,0.0]}"}),
      {
          audio_alias("start_audio_input_capture", "audio_capture", "start"),
          audio_alias("stop_audio_input_capture", "audio_capture", "stop"),
          audio_alias("create_audiostream_wav", "audio_capture",
                      "create_wav"),
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_audio_bus_effect",
      "Remove an audio effect from a bus using its zero-based index within the bus effect chain. An out-of-range 'effect_index' returns an error, and removing the last effect leaves the bus with a plain chain. Query the current chain with audio_bus action='get_layout' first. Returns 'result' set to 'ok'. This tool is classified as a destructive side effect because it destroys the effect object that already exists inside the host, and no undo path is provided.",
      "Audio", {"audio", "effect", "remove"}, SideEffect::Destructive, tool_flags::kNone,
      kRemoveAudioBusEffectParams, audio_ops::handle_effect_remove}));
  return v;
}

} // namespace audio_tools
} // namespace godot_autopilot

#endif
