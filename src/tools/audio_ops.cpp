#include "audio_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/audio_bus_layout.hpp>
#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_player2d.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
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
  auto *root = editor->get_edited_scene_root();
  if (!root)
    return nullptr;
  std::string clean = path_str;
  if (!clean.empty() && clean[0] == '/') {
    clean = clean.substr(1);
  }
  if (clean.size() > 5 && clean.compare(0, 5, "root/") == 0) {
    clean = clean.substr(5);
  }
  if (clean.empty() || clean == util::to_std(root->get_name())) {
    return root;
  }
  godot::NodePath np(godot::String(clean.c_str()));
  auto *node = root->get_node_or_null(np);
  if (!node) {
    std::string root_name = util::to_std(root->get_name());
    if (clean.size() > root_name.size() + 1 &&
        clean.compare(0, root_name.size(), root_name) == 0 &&
        clean[root_name.size()] == '/') {
      std::string sub = clean.substr(root_name.size() + 1);
      if (!sub.empty()) {
        node =
            root->get_node_or_null(godot::NodePath(godot::String(sub.c_str())));
      }
    }
  }
  if (!node)
    return nullptr;
  auto *p = node->get_parent();
  while (p) {
    if (p == root)
      return node;
    p = p->get_parent();
  }
  return nullptr;
}

mcp::JsonValue ok_json() {
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
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
  return ok_json();
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
  float vol = static_cast<float>(
      vd->IsDouble() ? vd->GetDouble() : static_cast<double>(vd->GetInt()));
  server->set_bus_volume_db(idx, vol);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_bus_volume_db completed");
  return ok_json();
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
  return ok_json();
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
  return ok_json();
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
  return ok_json();
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
  return ok_json();
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
  float from_pos = 0.0f;
  auto *fp = args.Find("from_position");
  if (fp && fp->IsNumber()) {
    from_pos = static_cast<float>(
        fp->IsDouble() ? fp->GetDouble() : static_cast<double>(fp->GetInt()));
  }
  if (ap.get_stream().is_null()) {
    return util::error_json("node has no audio stream set: " + path +
                      " — set the stream first (e.g. resource_set_property "
                      "with a loaded AudioStream resource)");
  }
  ap.play(from_pos);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "play_audio_player completed");
  return ok_json();
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
  return ok_json();
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
  float vol = static_cast<float>(
      vd->IsDouble() ? vd->GetDouble() : static_cast<double>(vd->GetInt()));
  ap.set_volume_db(vol);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_player_volume_db completed");
  return ok_json();
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
  float pitch = static_cast<float>(
      ps->IsDouble() ? ps->GetDouble() : static_cast<double>(ps->GetInt()));
  ap.set_pitch_scale(pitch);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_audio_player_pitch_scale completed");
  return ok_json();
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
  float to_pos = static_cast<float>(
      tp->IsDouble() ? tp->GetDouble() : static_cast<double>(tp->GetInt()));
  ap.seek(to_pos);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "seek_audio_player completed");
  return ok_json();
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
  return ok_json();
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
  return ok_json();
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
  return ok_json();
}

} // namespace audio_ops
} // namespace godot_autopilot
