#include "audio_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_bus_layout.hpp>
#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <string>

namespace godot_self_driving {
namespace audio_ops {

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

godot::Node* find_node(const std::string& path_str) {
    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) return nullptr;
    auto* root = editor->get_edited_scene_root();
    if (!root) return nullptr;
    std::string clean = path_str;
    if (!clean.empty() && clean[0] == '/') {
        clean = clean.substr(1);
    }
    if (clean.empty() || clean == to_std(root->get_name())) {
        return root;
    }
    godot::NodePath np(godot::String(clean.c_str()));
    return root->get_node_or_null(np);
}

mcp::JsonValue error_json(const std::string& msg) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(msg);
    return e;
}

mcp::JsonValue ok_json() {
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    return r;
}

} // namespace

mcp::JsonValue handle_bus_get_layout(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_get_layout called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto layout = server->generate_bus_layout();
    auto serialized = VariantJson::serialize(layout);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(serialized);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_get_layout completed");
    return r;
}

mcp::JsonValue handle_bus_set_layout(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_layout called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* layout_val = args.Find("layout");
    if (!layout_val || !layout_val->IsObject()) {
        return error_json("missing required parameter: layout");
    }
    auto variant = VariantJson::deserialize(*layout_val, "object");
    auto layout = godot::Ref<godot::AudioBusLayout>(variant);
    if (layout.is_null()) {
        return error_json("failed to deserialize AudioBusLayout");
    }
    server->set_bus_layout(layout);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_layout completed");
    return ok_json();
}

mcp::JsonValue handle_bus_get_count(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_get_count called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    int count = server->get_bus_count();
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(count));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_get_count completed");
    return r;
}

mcp::JsonValue handle_bus_get_name(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_get_name called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* bi = args.Find("bus_index");
    if (!bi || !bi->IsInt()) {
        return error_json("missing required parameter: bus_index");
    }
    int idx = bi->GetInt();
    int count = server->get_bus_count();
    if (idx < 0 || idx >= count) {
        return error_json("audio bus not found at index: " + std::to_string(idx));
    }
    godot::String name = server->get_bus_name(idx);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(to_std(name));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_get_name completed");
    return r;
}

mcp::JsonValue handle_bus_set_volume(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_volume called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* bi = args.Find("bus_index");
    if (!bi || !bi->IsInt()) {
        return error_json("missing required parameter: bus_index");
    }
    auto* vd = args.Find("volume_db");
    if (!vd || !vd->IsNumber()) {
        return error_json("missing required parameter: volume_db");
    }
    int idx = bi->GetInt();
    int count = server->get_bus_count();
    if (idx < 0 || idx >= count) {
        return error_json("audio bus not found at index: " + std::to_string(idx));
    }
    float vol = static_cast<float>(vd->IsDouble() ? vd->GetDouble() : static_cast<double>(vd->GetInt()));
    server->set_bus_volume_db(idx, vol);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_volume completed");
    return ok_json();
}

mcp::JsonValue handle_bus_set_mute(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_mute called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* bi = args.Find("bus_index");
    if (!bi || !bi->IsInt()) {
        return error_json("missing required parameter: bus_index");
    }
    auto* mu = args.Find("muted");
    if (!mu || !mu->IsBool()) {
        return error_json("missing required parameter: muted");
    }
    int idx = bi->GetInt();
    int count = server->get_bus_count();
    if (idx < 0 || idx >= count) {
        return error_json("audio bus not found at index: " + std::to_string(idx));
    }
    server->set_bus_mute(idx, mu->GetBool());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_mute completed");
    return ok_json();
}

mcp::JsonValue handle_bus_set_bypass(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_bypass called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* bi = args.Find("bus_index");
    if (!bi || !bi->IsInt()) {
        return error_json("missing required parameter: bus_index");
    }
    auto* bp = args.Find("bypass");
    if (!bp || !bp->IsBool()) {
        return error_json("missing required parameter: bypass");
    }
    int idx = bi->GetInt();
    int count = server->get_bus_count();
    if (idx < 0 || idx >= count) {
        return error_json("audio bus not found at index: " + std::to_string(idx));
    }
    server->set_bus_bypass_effects(idx, bp->GetBool());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_bypass completed");
    return ok_json();
}

mcp::JsonValue handle_effect_add(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_effect_add called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* bi = args.Find("bus_index");
    if (!bi || !bi->IsInt()) {
        return error_json("missing required parameter: bus_index");
    }
    auto* et = args.Find("effect_type");
    if (!et || !et->IsString()) {
        return error_json("missing required parameter: effect_type");
    }
    int idx = bi->GetInt();
    int count = server->get_bus_count();
    if (idx < 0 || idx >= count) {
        return error_json("audio bus not found at index: " + std::to_string(idx));
    }
    std::string effect_type = et->GetString();
    auto* cdbs = godot::ClassDBSingleton::get_singleton();
    if (!cdbs) {
        return error_json("ClassDB singleton not available");
    }
    godot::StringName sn(effect_type.c_str());
    godot::Variant effect_var = cdbs->instantiate(sn);
    if (effect_var.get_type() == godot::Variant::NIL) {
        return error_json("failed to instantiate effect type: " + effect_type);
    }
    auto effect_ref = godot::Ref<godot::AudioEffect>(effect_var);
    if (effect_ref.is_null()) {
        return error_json("instantiated object is not an AudioEffect: " + effect_type);
    }
    auto* pos_val = args.Find("at_position");
    int position = -1;
    if (pos_val && pos_val->IsInt()) {
        position = pos_val->GetInt();
    }
    server->add_bus_effect(idx, effect_ref, position);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_effect_add completed");
    return ok_json();
}

mcp::JsonValue handle_effect_remove(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_effect_remove called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* bi = args.Find("bus_index");
    if (!bi || !bi->IsInt()) {
        return error_json("missing required parameter: bus_index");
    }
    auto* ei = args.Find("effect_index");
    if (!ei || !ei->IsInt()) {
        return error_json("missing required parameter: effect_index");
    }
    int bus_idx = bi->GetInt();
    int count = server->get_bus_count();
    if (bus_idx < 0 || bus_idx >= count) {
        return error_json("audio bus not found at index: " + std::to_string(bus_idx));
    }
    int effect_idx = ei->GetInt();
    server->remove_bus_effect(bus_idx, effect_idx);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_effect_remove completed");
    return ok_json();
}

mcp::JsonValue handle_stream_play(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_play called");
    auto* np = args.Find("node_path");
    if (!np || !np->IsString()) {
        return error_json("missing required parameter: node_path");
    }
    std::string path = np->GetString();
    auto* node = find_node(path);
    if (!node) {
        return error_json("AudioStreamPlayer node not found: " + path);
    }
    auto* player = godot::Object::cast_to<godot::AudioStreamPlayer>(node);
    if (!player) {
        return error_json("node is not an AudioStreamPlayer: " + path);
    }
    auto* sp = args.Find("stream_path");
    if (sp && sp->IsString()) {
        auto* loader = godot::ResourceLoader::get_singleton();
        if (!loader) {
            return error_json("ResourceLoader not available");
        }
        auto stream = loader->load(godot::String(sp->GetString().c_str()));
        if (stream.is_null()) {
            return error_json("failed to load audio stream: " + sp->GetString());
        }
        auto audio_stream = godot::Ref<godot::AudioStream>(stream);
        if (audio_stream.is_null()) {
            return error_json("loaded resource is not an AudioStream: " + sp->GetString());
        }
        player->set_stream(audio_stream);
    }
    float from_pos = 0.0f;
    auto* fp = args.Find("from_position");
    if (fp && fp->IsNumber()) {
        from_pos = static_cast<float>(fp->IsDouble() ? fp->GetDouble() : static_cast<double>(fp->GetInt()));
    }
    player->play(from_pos);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_play completed");
    return ok_json();
}

mcp::JsonValue handle_stream_stop(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_stop called");
    auto* np = args.Find("node_path");
    if (!np || !np->IsString()) {
        return error_json("missing required parameter: node_path");
    }
    std::string path = np->GetString();
    auto* node = find_node(path);
    if (!node) {
        return error_json("AudioStreamPlayer node not found: " + path);
    }
    auto* player = godot::Object::cast_to<godot::AudioStreamPlayer>(node);
    if (!player) {
        return error_json("node is not an AudioStreamPlayer: " + path);
    }
    player->stop();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_stop completed");
    return ok_json();
}

mcp::JsonValue handle_stream_set_volume(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_set_volume called");
    auto* np = args.Find("node_path");
    if (!np || !np->IsString()) {
        return error_json("missing required parameter: node_path");
    }
    auto* vd = args.Find("volume_db");
    if (!vd || !vd->IsNumber()) {
        return error_json("missing required parameter: volume_db");
    }
    std::string path = np->GetString();
    auto* node = find_node(path);
    if (!node) {
        return error_json("AudioStreamPlayer node not found: " + path);
    }
    auto* player = godot::Object::cast_to<godot::AudioStreamPlayer>(node);
    if (!player) {
        return error_json("node is not an AudioStreamPlayer: " + path);
    }
    float vol = static_cast<float>(vd->IsDouble() ? vd->GetDouble() : static_cast<double>(vd->GetInt()));
    player->set_volume_db(vol);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_set_volume completed");
    return ok_json();
}

mcp::JsonValue handle_stream_set_pitch(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_set_pitch called");
    auto* np = args.Find("node_path");
    if (!np || !np->IsString()) {
        return error_json("missing required parameter: node_path");
    }
    auto* ps = args.Find("pitch_scale");
    if (!ps || !ps->IsNumber()) {
        return error_json("missing required parameter: pitch_scale");
    }
    std::string path = np->GetString();
    auto* node = find_node(path);
    if (!node) {
        return error_json("AudioStreamPlayer node not found: " + path);
    }
    auto* player = godot::Object::cast_to<godot::AudioStreamPlayer>(node);
    if (!player) {
        return error_json("node is not an AudioStreamPlayer: " + path);
    }
    float pitch = static_cast<float>(ps->IsDouble() ? ps->GetDouble() : static_cast<double>(ps->GetInt()));
    player->set_pitch_scale(pitch);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_set_pitch completed");
    return ok_json();
}

mcp::JsonValue handle_stream_get_playback_position(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_get_playback_position called");
    auto* np = args.Find("node_path");
    if (!np || !np->IsString()) {
        return error_json("missing required parameter: node_path");
    }
    std::string path = np->GetString();
    auto* node = find_node(path);
    if (!node) {
        return error_json("AudioStreamPlayer node not found: " + path);
    }
    auto* player = godot::Object::cast_to<godot::AudioStreamPlayer>(node);
    if (!player) {
        return error_json("node is not an AudioStreamPlayer: " + path);
    }
    float pos = player->get_playback_position();
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<double>(pos));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_get_playback_position completed");
    return r;
}

mcp::JsonValue handle_stream_seek(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_seek called");
    auto* np = args.Find("node_path");
    if (!np || !np->IsString()) {
        return error_json("missing required parameter: node_path");
    }
    auto* tp = args.Find("to_position");
    if (!tp || !tp->IsNumber()) {
        return error_json("missing required parameter: to_position");
    }
    std::string path = np->GetString();
    auto* node = find_node(path);
    if (!node) {
        return error_json("AudioStreamPlayer node not found: " + path);
    }
    auto* player = godot::Object::cast_to<godot::AudioStreamPlayer>(node);
    if (!player) {
        return error_json("node is not an AudioStreamPlayer: " + path);
    }
    float to_pos = static_cast<float>(tp->IsDouble() ? tp->GetDouble() : static_cast<double>(tp->GetInt()));
    player->seek(to_pos);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_stream_seek completed");
    return ok_json();
}

mcp::JsonValue handle_bus_set_solo(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_solo called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* bi = args.Find("bus_index");
    if (!bi || !bi->IsInt()) {
        return error_json("missing required parameter: bus_index");
    }
    auto* sl = args.Find("solo");
    if (!sl || !sl->IsBool()) {
        return error_json("missing required parameter: solo");
    }
    int idx = bi->GetInt();
    int count = server->get_bus_count();
    if (idx < 0 || idx >= count) {
        return error_json("audio bus not found at index: " + std::to_string(idx));
    }
    server->set_bus_solo(idx, sl->GetBool());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_bus_set_solo completed");
    return ok_json();
}

mcp::JsonValue handle_get_output_device_list(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_get_output_device_list called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto devices = server->get_output_device_list();
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    for (int i = 0; i < devices.size(); i++) {
        arr.PushBack(mcp::JsonValue(to_std(devices[i])));
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_get_output_device_list completed");
    return r;
}

mcp::JsonValue handle_set_output_device(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_set_output_device called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* dv = args.Find("device");
    if (!dv || !dv->IsString()) {
        return error_json("missing required parameter: device");
    }
    server->set_output_device(godot::String(dv->GetString().c_str()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_set_output_device completed");
    return ok_json();
}

mcp::JsonValue handle_get_input_device_list(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_get_input_device_list called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto devices = server->get_input_device_list();
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    for (int i = 0; i < devices.size(); i++) {
        arr.PushBack(mcp::JsonValue(to_std(devices[i])));
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_get_input_device_list completed");
    return r;
}

mcp::JsonValue handle_set_input_device(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_set_input_device called");
    auto* server = godot::AudioServer::get_singleton();
    if (!server) {
        return error_json("AudioServer not available");
    }
    auto* dv = args.Find("device");
    if (!dv || !dv->IsString()) {
        return error_json("missing required parameter: device");
    }
    server->set_input_device(godot::String(dv->GetString().c_str()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "audio_set_input_device completed");
    return ok_json();
}

} // namespace audio_ops
} // namespace godot_self_driving
