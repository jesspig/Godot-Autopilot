#include "audio_ops.hpp"
#include "core/log_system.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/audio_bus_layout.hpp>
#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_player2d.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace audio_ops {

namespace {

constexpr const char *NODE_PATH_HINT =
    " — expected scene-relative path like 'Level1/Player' or absolute "
    "'/root/Level1/Player'";

godot::Node *find_node(const std::string &path_str) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return nullptr;
  return util::resolve_scene_node(path_str, editor->get_edited_scene_root());
}

struct AudioPlayerVariant {
  godot::AudioStreamPlayer *p1 = nullptr;
  godot::AudioStreamPlayer2D *p2 = nullptr;
  godot::AudioStreamPlayer3D *p3 = nullptr;

  bool is_valid() const { return p1 || p2 || p3; }

  std::string type_name() const {
    if (p1)
      return "AudioStreamPlayer";
    if (p2)
      return "AudioStreamPlayer2D";
    if (p3)
      return "AudioStreamPlayer3D";
    return "unknown";
  }

  void play(float from_pos = 0.0f) {
    if (p1)
      p1->play(from_pos);
    else if (p2)
      p2->play(from_pos);
    else if (p3)
      p3->play(from_pos);
  }

  void stop() {
    if (p1)
      p1->stop();
    else if (p2)
      p2->stop();
    else if (p3)
      p3->stop();
  }

  void seek(float to_position) {
    if (p1)
      p1->seek(to_position);
    else if (p2)
      p2->seek(to_position);
    else if (p3)
      p3->seek(to_position);
  }

  void set_volume_db(float volume_db) {
    if (p1)
      p1->set_volume_db(volume_db);
    else if (p2)
      p2->set_volume_db(volume_db);
    else if (p3)
      p3->set_volume_db(volume_db);
  }

  void set_pitch_scale(float pitch_scale) {
    if (p1)
      p1->set_pitch_scale(pitch_scale);
    else if (p2)
      p2->set_pitch_scale(pitch_scale);
    else if (p3)
      p3->set_pitch_scale(pitch_scale);
  }

  float get_playback_position() {
    if (p1)
      return p1->get_playback_position();
    if (p2)
      return p2->get_playback_position();
    if (p3)
      return p3->get_playback_position();
    return 0.0f;
  }

  void set_stream(const godot::Ref<godot::AudioStream> &stream) {
    if (p1)
      p1->set_stream(stream);
    else if (p2)
      p2->set_stream(stream);
    else if (p3)
      p3->set_stream(stream);
  }

  godot::Ref<godot::AudioStream> get_stream() {
    if (p1)
      return p1->get_stream();
    if (p2)
      return p2->get_stream();
    if (p3)
      return p3->get_stream();
    return {};
  }
};

AudioPlayerVariant resolve_audio_player(godot::Node *node) {
  AudioPlayerVariant r;
  r.p1 = godot::Object::cast_to<godot::AudioStreamPlayer>(node);
  if (!r.p1)
    r.p2 = godot::Object::cast_to<godot::AudioStreamPlayer2D>(node);
  if (!r.p1 && !r.p2)
    r.p3 = godot::Object::cast_to<godot::AudioStreamPlayer3D>(node);
  return r;
}

constexpr int kMaxCaptureFrames = 1000000;
constexpr int kDefaultWavMixRate = 44100;
constexpr int kMinWavMixRate = 1000;
constexpr int kMaxWavMixRate = 192000;

bool parse_wav_name(const mcp::JsonValue &args, std::string &out_name,
                    std::string &out_error) {
  out_name.clear();
  out_error.clear();
  auto *n = args.Find("name");
  if (!n || !n->IsString() || n->GetString().empty()) {
    out_error = "missing required parameter: name";
    return false;
  }
  out_name = n->GetString();
  return true;
}

bool parse_wav_format(const mcp::JsonValue &args, int &out_bits,
                      std::string &out_error) {
  out_bits = 16;
  out_error.clear();
  auto *f = args.Find("format");
  if (!f) {
    return true;
  }
  if (!f->IsInt() || (f->GetInt() != 8 && f->GetInt() != 16)) {
    out_error = "unsupported format: expected 8 or 16";
    return false;
  }
  out_bits = static_cast<int>(f->GetInt());
  return true;
}

bool parse_wav_mix_rate(const mcp::JsonValue &args, int fallback_rate,
                        int &out_rate, std::string &out_error) {
  out_rate = fallback_rate;
  out_error.clear();
  auto *m = args.Find("mix_rate");
  if (!m) {
    return true;
  }
  if (!m->IsInt() || m->GetInt() < kMinWavMixRate ||
      m->GetInt() > kMaxWavMixRate) {
    out_error = "invalid mix_rate: expected an integer in 1000..192000";
    return false;
  }
  out_rate = static_cast<int>(m->GetInt());
  return true;
}

bool parse_wav_stereo(const mcp::JsonValue &args, bool &out_stereo,
                      std::string &out_error) {
  out_stereo = false;
  out_error.clear();
  auto *s = args.Find("stereo");
  if (!s) {
    return true;
  }
  if (!s->IsBool()) {
    out_error = "invalid stereo: expected a boolean";
    return false;
  }
  out_stereo = s->GetBool();
  return true;
}

void encode_sample(godot::PackedByteArray &bytes, int64_t &cursor,
                   float sample, int format_bits) {
  float c = sample;
  if (c > 1.0f) {
    c = 1.0f;
  }
  if (c < -1.0f) {
    c = -1.0f;
  }
  if (format_bits == 8) {
    int v = static_cast<int>(c * 127.0f + 128.0f);
    if (v < 0) {
      v = 0;
    }
    if (v > 255) {
      v = 255;
    }
    bytes[cursor++] = static_cast<uint8_t>(v);
  } else {
    int32_t v = static_cast<int32_t>(c * 32767.0f);
    if (v < -32768) {
      v = -32768;
    }
    if (v > 32767) {
      v = 32767;
    }
    uint16_t u = static_cast<uint16_t>(v);
    bytes[cursor++] = static_cast<uint8_t>(u & 0xFF);
    bytes[cursor++] = static_cast<uint8_t>((u >> 8) & 0xFF);
  }
}

godot::Ref<godot::AudioStreamWAV> instantiate_wav(std::string &out_error) {
  out_error.clear();
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    out_error = "ClassDB singleton not available";
    return godot::Ref<godot::AudioStreamWAV>();
  }
  godot::Variant wav_var =
      cdbs->instantiate(godot::StringName("AudioStreamWAV"));
  if (wav_var.get_type() == godot::Variant::NIL) {
    out_error = "failed to instantiate AudioStreamWAV";
    return godot::Ref<godot::AudioStreamWAV>();
  }
  godot::Ref<godot::AudioStreamWAV> wav = wav_var;
  if (wav.is_null()) {
    out_error = "instantiated object is not an AudioStreamWAV";
    return godot::Ref<godot::AudioStreamWAV>();
  }
  return wav;
}

mcp::JsonValue wav_locator(const std::string &name,
                           const godot::Ref<godot::AudioStreamWAV> &wav,
                           int format_bits, int mix_rate, bool stereo,
                           int64_t frames) {
  mcp::JsonValue info(mcp::JsonValue::object_tag);
  info["class"] = mcp::JsonValue("AudioStreamWAV");
  info["name"] = mcp::JsonValue(name);
  info["path"] = mcp::JsonValue("memory://" + name);
  info["object_id"] =
      mcp::JsonValue(static_cast<int64_t>(wav->get_instance_id()));
  info["object_id_str"] = mcp::JsonValue(
      std::to_string(static_cast<int64_t>(wav->get_instance_id())));
  info["format"] = mcp::JsonValue(static_cast<int64_t>(format_bits));
  info["mix_rate"] = mcp::JsonValue(static_cast<int64_t>(mix_rate));
  info["stereo"] = mcp::JsonValue(stereo);
  info["frames"] = mcp::JsonValue(frames);
  return info;
}

} // namespace

mcp::JsonValue handle_bus_get_layout(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_bus_layout called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto layout = server->generate_bus_layout();
  auto serialized = VariantJson::serialize(layout);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(serialized);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_bus_layout completed");
  return r;
}

mcp::JsonValue handle_bus_set_layout(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_layout called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *layout_val = args.Find("layout");
  if (!layout_val || !layout_val->IsObject()) {
    return util::error_json("missing required parameter: layout");
  }
  auto variant = VariantJson::deserialize(*layout_val, "object");
  auto layout = godot::Ref<godot::AudioBusLayout>(variant);
  if (layout.is_null()) {
    return util::error_json("failed to deserialize AudioBusLayout");
  }
  server->set_bus_layout(layout);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_layout completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_bus_get_count(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_bus_count called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  int count = server->get_bus_count();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(count));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_bus_count completed");
  return r;
}

mcp::JsonValue handle_bus_get_name(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_bus_name called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *bi = args.Find("bus_index");
  if (!bi || !bi->IsInt()) {
    return util::error_json("missing required parameter: bus_index");
  }
  int idx = bi->GetInt();
  int count = server->get_bus_count();
  if (idx < 0 || idx >= count) {
    return util::error_json("audio bus not found at index: " + std::to_string(idx));
  }
  godot::String name = server->get_bus_name(idx);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(util::to_std(name));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_bus_name completed");
  return r;
}

mcp::JsonValue handle_bus_set_volume(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_volume_db called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *bi = args.Find("bus_index");
  if (!bi || !bi->IsInt()) {
    return util::error_json("missing required parameter: bus_index");
  }
  auto *vd = args.Find("volume_db");
  if (!vd || !vd->IsNumber()) {
    return util::error_json("missing required parameter: volume_db");
  }
  int idx = bi->GetInt();
  int count = server->get_bus_count();
  if (idx < 0 || idx >= count) {
    return util::error_json("audio bus not found at index: " + std::to_string(idx));
  }
  float vol = static_cast<float>(util::json_number(vd, 0.0));
  server->set_bus_volume_db(idx, vol);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_volume_db completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_bus_set_mute(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_mute called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *bi = args.Find("bus_index");
  if (!bi || !bi->IsInt()) {
    return util::error_json("missing required parameter: bus_index");
  }
  auto *mu = args.Find("muted");
  if (!mu || !mu->IsBool()) {
    return util::error_json("missing required parameter: muted");
  }
  int idx = bi->GetInt();
  int count = server->get_bus_count();
  if (idx < 0 || idx >= count) {
    return util::error_json("audio bus not found at index: " + std::to_string(idx));
  }
  server->set_bus_mute(idx, mu->GetBool());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_mute completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_bus_set_bypass(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_bypass_effects called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *bi = args.Find("bus_index");
  if (!bi || !bi->IsInt()) {
    return util::error_json("missing required parameter: bus_index");
  }
  auto *bp = args.Find("bypass");
  if (!bp || !bp->IsBool()) {
    return util::error_json("missing required parameter: bypass");
  }
  int idx = bi->GetInt();
  int count = server->get_bus_count();
  if (idx < 0 || idx >= count) {
    return util::error_json("audio bus not found at index: " + std::to_string(idx));
  }
  server->set_bus_bypass_effects(idx, bp->GetBool());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_bypass_effects completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_effect_add(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_audio_bus_effect called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *bi = args.Find("bus_index");
  if (!bi || !bi->IsInt()) {
    return util::error_json("missing required parameter: bus_index");
  }
  auto *et = args.Find("effect_type");
  if (!et || !et->IsString()) {
    return util::error_json("missing required parameter: effect_type");
  }
  int idx = bi->GetInt();
  int count = server->get_bus_count();
  if (idx < 0 || idx >= count) {
    return util::error_json("audio bus not found at index: " + std::to_string(idx));
  }
  std::string effect_type = et->GetString();
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    return util::error_json("ClassDB singleton not available");
  }
  godot::StringName sn(effect_type.c_str());
  godot::Variant effect_var = cdbs->instantiate(sn);
  if (effect_var.get_type() == godot::Variant::NIL) {
    return util::error_json("failed to instantiate effect type: " + effect_type);
  }
  auto effect_ref = godot::Ref<godot::AudioEffect>(effect_var);
  if (effect_ref.is_null()) {
    return util::error_json("instantiated object is not an AudioEffect: " +
                      effect_type);
  }
  auto *pos_val = args.Find("at_position");
  int position = -1;
  if (pos_val && pos_val->IsInt()) {
    position = pos_val->GetInt();
  }
  server->add_bus_effect(idx, effect_ref, position);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_audio_bus_effect completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_effect_remove(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "remove_audio_bus_effect called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *bi = args.Find("bus_index");
  if (!bi || !bi->IsInt()) {
    return util::error_json("missing required parameter: bus_index");
  }
  auto *ei = args.Find("effect_index");
  if (!ei || !ei->IsInt()) {
    return util::error_json("missing required parameter: effect_index");
  }
  int bus_idx = bi->GetInt();
  int count = server->get_bus_count();
  if (bus_idx < 0 || bus_idx >= count) {
    return util::error_json("audio bus not found at index: " +
                      std::to_string(bus_idx));
  }
  int effect_idx = ei->GetInt();
  server->remove_bus_effect(bus_idx, effect_idx);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "remove_audio_bus_effect completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_stream_play(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "play_audio_player called");
  auto *np = args.Find("node_path");
  if (!np || !np->IsString()) {
    return util::error_json("missing required parameter: node_path");
  }
  std::string path = np->GetString();
  auto *node = find_node(path);
  if (!node) {
    return util::error_json("AudioStreamPlayer node not found: " + path +
                      NODE_PATH_HINT);
  }
  auto ap = resolve_audio_player(node);
  if (!ap.is_valid()) {
    return util::error_json(
        "node is not an "
        "AudioStreamPlayer/AudioStreamPlayer2D/AudioStreamPlayer3D: " +
        path + " (actual class: " + util::to_std(node->get_class()) + ")");
  }
  auto *sp = args.Find("stream_path");
  if (sp && sp->IsString()) {
    godot::String stream_path(sp->GetString().c_str());
    if (!godot::FileAccess::file_exists(stream_path))
      return util::error_detail("file does not exist", sp->GetString(),
                                "an existing file path",
                                "check the disk directory structure; docs "
                                "paths may be relative to the wrong folder");
    auto *loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
      return util::error_json("ResourceLoader not available");
    }
    auto stream = loader->load(stream_path);
    if (stream.is_null()) {
      return util::error_detail(
          "file exists but failed to load (not imported or wrong type)",
          sp->GetString(), "an importable resource of type AudioStream",
          "reimport the file or check the file format");
    }
    auto audio_stream = godot::Ref<godot::AudioStream>(stream);
    if (audio_stream.is_null()) {
      return util::error_json("loaded resource is not an AudioStream: " +
                        sp->GetString());
    }
    ap.set_stream(audio_stream);
  }
  float from_pos =
      static_cast<float>(util::json_number(args.Find("from_position"), 0.0));
  if (ap.get_stream().is_null()) {
    return util::error_json("node has no audio stream set: " + path +
                      " — set the stream first (e.g. resource_set_property "
                      "with a loaded AudioStream resource)");
  }
  ap.play(from_pos);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "play_audio_player completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_stream_stop(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_audio_player called");
  auto *np = args.Find("node_path");
  if (!np || !np->IsString()) {
    return util::error_json("missing required parameter: node_path");
  }
  std::string path = np->GetString();
  auto *node = find_node(path);
  if (!node) {
    return util::error_json("AudioStreamPlayer node not found: " + path +
                      NODE_PATH_HINT);
  }
  auto ap = resolve_audio_player(node);
  if (!ap.is_valid()) {
    return util::error_json(
        "node is not an "
        "AudioStreamPlayer/AudioStreamPlayer2D/AudioStreamPlayer3D: " +
        path + " (actual class: " + util::to_std(node->get_class()) + ")");
  }
  ap.stop();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_audio_player completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_stream_set_volume(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_player_volume_db called");
  auto *np = args.Find("node_path");
  if (!np || !np->IsString()) {
    return util::error_json("missing required parameter: node_path");
  }
  auto *vd = args.Find("volume_db");
  if (!vd || !vd->IsNumber()) {
    return util::error_json("missing required parameter: volume_db");
  }
  std::string path = np->GetString();
  auto *node = find_node(path);
  if (!node) {
    return util::error_json("AudioStreamPlayer node not found: " + path +
                      NODE_PATH_HINT);
  }
  auto ap = resolve_audio_player(node);
  if (!ap.is_valid()) {
    return util::error_json(
        "node is not an "
        "AudioStreamPlayer/AudioStreamPlayer2D/AudioStreamPlayer3D: " +
        path + " (actual class: " + util::to_std(node->get_class()) + ")");
  }
  float vol = static_cast<float>(util::json_number(vd, 0.0));
  ap.set_volume_db(vol);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_player_volume_db completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_stream_set_pitch(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_player_pitch_scale called");
  auto *np = args.Find("node_path");
  if (!np || !np->IsString()) {
    return util::error_json("missing required parameter: node_path");
  }
  auto *ps = args.Find("pitch_scale");
  if (!ps || !ps->IsNumber()) {
    return util::error_json("missing required parameter: pitch_scale");
  }
  std::string path = np->GetString();
  auto *node = find_node(path);
  if (!node) {
    return util::error_json("AudioStreamPlayer node not found: " + path +
                      NODE_PATH_HINT);
  }
  auto ap = resolve_audio_player(node);
  if (!ap.is_valid()) {
    return util::error_json(
        "node is not an "
        "AudioStreamPlayer/AudioStreamPlayer2D/AudioStreamPlayer3D: " +
        path + " (actual class: " + util::to_std(node->get_class()) + ")");
  }
  float pitch = static_cast<float>(util::json_number(ps, 0.0));
  ap.set_pitch_scale(pitch);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_player_pitch_scale completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_stream_get_playback_position(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_player_playback_position called");
  auto *np = args.Find("node_path");
  if (!np || !np->IsString()) {
    return util::error_json("missing required parameter: node_path");
  }
  std::string path = np->GetString();
  auto *node = find_node(path);
  if (!node) {
    return util::error_json("AudioStreamPlayer node not found: " + path +
                      NODE_PATH_HINT);
  }
  auto ap = resolve_audio_player(node);
  if (!ap.is_valid()) {
    return util::error_json(
        "node is not an "
        "AudioStreamPlayer/AudioStreamPlayer2D/AudioStreamPlayer3D: " +
        path + " (actual class: " + util::to_std(node->get_class()) + ")");
  }
  float pos = ap.get_playback_position();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<double>(pos));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_player_playback_position completed");
  return r;
}

mcp::JsonValue handle_stream_seek(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "seek_audio_player called");
  auto *np = args.Find("node_path");
  if (!np || !np->IsString()) {
    return util::error_json("missing required parameter: node_path");
  }
  auto *tp = args.Find("to_position");
  if (!tp || !tp->IsNumber()) {
    return util::error_json("missing required parameter: to_position");
  }
  std::string path = np->GetString();
  auto *node = find_node(path);
  if (!node) {
    return util::error_json("AudioStreamPlayer node not found: " + path +
                      NODE_PATH_HINT);
  }
  auto ap = resolve_audio_player(node);
  if (!ap.is_valid()) {
    return util::error_json(
        "node is not an "
        "AudioStreamPlayer/AudioStreamPlayer2D/AudioStreamPlayer3D: " +
        path + " (actual class: " + util::to_std(node->get_class()) + ")");
  }
  float to_pos = static_cast<float>(util::json_number(tp, 0.0));
  ap.seek(to_pos);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "seek_audio_player completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_bus_set_solo(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_solo called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *bi = args.Find("bus_index");
  if (!bi || !bi->IsInt()) {
    return util::error_json("missing required parameter: bus_index");
  }
  auto *sl = args.Find("solo");
  if (!sl || !sl->IsBool()) {
    return util::error_json("missing required parameter: solo");
  }
  int idx = bi->GetInt();
  int count = server->get_bus_count();
  if (idx < 0 || idx >= count) {
    return util::error_json("audio bus not found at index: " + std::to_string(idx));
  }
  server->set_bus_solo(idx, sl->GetBool());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_solo completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_get_output_device_list(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_device_outputs called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto devices = server->get_output_device_list();
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < devices.size(); i++) {
    arr.PushBack(mcp::JsonValue(util::to_std(devices[i])));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_device_outputs completed");
  return r;
}

mcp::JsonValue handle_set_output_device(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_device_output called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *dv = args.Find("device");
  if (!dv || !dv->IsString()) {
    return util::error_json("missing required parameter: device");
  }
  server->set_output_device(godot::String(dv->GetString().c_str()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_device_output completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_get_input_device_list(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_device_inputs called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto devices = server->get_input_device_list();
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < devices.size(); i++) {
    arr.PushBack(mcp::JsonValue(util::to_std(devices[i])));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_device_inputs completed");
  return r;
}

mcp::JsonValue handle_set_input_device(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_device_input called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *dv = args.Find("device");
  if (!dv || !dv->IsString()) {
    return util::error_json("missing required parameter: device");
  }
  server->set_input_device(godot::String(dv->GetString().c_str()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_device_input completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_start_input_capture(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "start_audio_input_capture called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto *dv = args.Find("device");
  if (dv) {
    if (!dv->IsString() || dv->GetString().empty()) {
      return util::error_json("invalid device: expected a non-empty device "
                              "name from get_audio_input_devices");
    }
    server->set_input_device(godot::String(dv->GetString().c_str()));
  }
  godot::Error err = server->set_input_device_active(true);
  if (err != godot::Error::OK) {
    return util::error_json("failed to start audio input capture (error " +
                            std::to_string(static_cast<int>(err)) + ")");
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "start_audio_input_capture completed");
  return util::ok_result(mcp::JsonValue("ok"));
}

mcp::JsonValue handle_stop_input_capture(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_audio_input_capture called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  std::string name;
  std::string param_error;
  if (!parse_wav_name(args, name, param_error)) {
    return util::error_json(param_error);
  }
  int format_bits = 16;
  if (!parse_wav_format(args, format_bits, param_error)) {
    return util::error_json(param_error);
  }
  bool stereo = false;
  if (!parse_wav_stereo(args, stereo, param_error)) {
    return util::error_json(param_error);
  }
  int fallback_rate = static_cast<int>(server->get_input_mix_rate());
  if (fallback_rate <= 0) {
    fallback_rate = kDefaultWavMixRate;
  }
  int mix_rate = fallback_rate;
  if (!parse_wav_mix_rate(args, fallback_rate, mix_rate, param_error)) {
    return util::error_json(param_error);
  }
  int64_t max_frames = -1;
  auto *mf = args.Find("max_frames");
  if (mf) {
    if (!mf->IsInt() || mf->GetInt() < 1) {
      return util::error_json(
          "invalid max_frames: expected an integer of at least 1");
    }
    max_frames = mf->GetInt();
  }
  int64_t available =
      static_cast<int64_t>(server->get_input_frames_available());
  if (available < 0) {
    available = 0;
  }
  int64_t want = available;
  if (max_frames >= 0 && max_frames < want) {
    want = max_frames;
  }
  bool truncated = false;
  if (want > kMaxCaptureFrames) {
    want = kMaxCaptureFrames;
    truncated = true;
  }
  godot::PackedVector2Array captured;
  if (want > 0) {
    captured = server->get_input_frames(static_cast<int>(want));
  }
  int64_t frames = static_cast<int64_t>(captured.size());
  int channels = stereo ? 2 : 1;
  int bytes_per_sample = (format_bits == 8) ? 1 : 2;
  godot::PackedByteArray data;
  data.resize(frames * channels * bytes_per_sample);
  int64_t cursor = 0;
  for (int64_t i = 0; i < frames; i++) {
    godot::Vector2 frame = captured[i];
    encode_sample(data, cursor, frame.x, format_bits);
    if (stereo) {
      encode_sample(data, cursor, frame.y, format_bits);
    }
  }
  std::string wav_error;
  godot::Ref<godot::AudioStreamWAV> wav = instantiate_wav(wav_error);
  if (wav.is_null()) {
    server->set_input_device_active(false);
    return util::error_json(wav_error);
  }
  wav->set_format(
      static_cast<godot::AudioStreamWAV::Format>(format_bits == 8 ? 0 : 1));
  wav->set_mix_rate(mix_rate);
  wav->set_stereo(stereo);
  wav->set_data(data);
  resource_ops::register_memory_resource(wav, name);
  wav->set_path(godot::String(("memory://" + name).c_str()));
  godot::Error stop_err = server->set_input_device_active(false);
  if (stop_err != godot::Error::OK) {
    return util::error_json("failed to stop audio input capture (error " +
                            std::to_string(static_cast<int>(stop_err)) + ")");
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  mcp::JsonValue info = wav_locator(name, wav, format_bits, mix_rate, stereo,
                                    frames);
  info["truncated"] = mcp::JsonValue(truncated);
  r["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_audio_input_capture completed");
  return r;
}

mcp::JsonValue handle_create_wav(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_audiostream_wav called");
  std::string name;
  std::string param_error;
  if (!parse_wav_name(args, name, param_error)) {
    return util::error_json(param_error);
  }
  int format_bits = 16;
  if (!parse_wav_format(args, format_bits, param_error)) {
    return util::error_json(param_error);
  }
  int mix_rate = kDefaultWavMixRate;
  if (!parse_wav_mix_rate(args, kDefaultWavMixRate, mix_rate, param_error)) {
    return util::error_json(param_error);
  }
  bool stereo = false;
  if (!parse_wav_stereo(args, stereo, param_error)) {
    return util::error_json(param_error);
  }
  auto *samples_val = args.Find("samples");
  if (!samples_val || !samples_val->IsArray()) {
    return util::error_json("missing required parameter: samples");
  }
  const auto &samples = samples_val->GetArray();
  if (samples.empty()) {
    return util::error_json("samples must not be empty");
  }
  int channels = stereo ? 2 : 1;
  if (stereo && samples.size() % 2 != 0) {
    return util::error_json(
        "samples has an odd length " + std::to_string(samples.size()) +
        " for stereo: expected flat interleaved L/R pairs");
  }
  int64_t frames =
      static_cast<int64_t>(samples.size() / static_cast<size_t>(channels));
  if (frames > kMaxCaptureFrames) {
    return util::error_json(
        "samples exceed the capture limit of 1000000 frames (got " +
        std::to_string(frames) + ")");
  }
  int bytes_per_sample = (format_bits == 8) ? 1 : 2;
  godot::PackedByteArray data;
  data.resize(frames * channels * bytes_per_sample);
  int64_t cursor = 0;
  for (size_t i = 0; i < samples.size(); i++) {
    if (!samples[i].IsNumber()) {
      return util::error_json("invalid sample at index " +
                              std::to_string(i) + ": expected a number");
    }
    encode_sample(data, cursor,
                  static_cast<float>(util::json_number(&samples[i], 0.0)),
                  format_bits);
  }
  std::string wav_error;
  godot::Ref<godot::AudioStreamWAV> wav = instantiate_wav(wav_error);
  if (wav.is_null()) {
    return util::error_json(wav_error);
  }
  wav->set_format(
      static_cast<godot::AudioStreamWAV::Format>(format_bits == 8 ? 0 : 1));
  wav->set_mix_rate(mix_rate);
  wav->set_stereo(stereo);
  wav->set_data(data);
  resource_ops::register_memory_resource(wav, name);
  wav->set_path(godot::String(("memory://" + name).c_str()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] =
      wav_locator(name, wav, format_bits, mix_rate, stereo, frames);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_audiostream_wav completed");
  return r;
}

mcp::JsonValue handle_get_input_devices(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_input_devices called");
  auto *server = godot::AudioServer::get_singleton();
  if (!server) {
    return util::error_json("AudioServer not available");
  }
  auto devices = server->get_input_device_list();
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < devices.size(); i++) {
    arr.PushBack(mcp::JsonValue(util::to_std(devices[i])));
  }
  mcp::JsonValue info(mcp::JsonValue::object_tag);
  info["devices"] = std::move(arr);
  info["current"] =
      mcp::JsonValue(util::to_std(server->get_input_device()));
  info["input_mix_rate"] =
      mcp::JsonValue(static_cast<double>(server->get_input_mix_rate()));
  info["frames_available"] = mcp::JsonValue(
      static_cast<int64_t>(server->get_input_frames_available()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_audio_input_devices completed");
  return r;
}

} // namespace audio_ops
} // namespace godot_autopilot
