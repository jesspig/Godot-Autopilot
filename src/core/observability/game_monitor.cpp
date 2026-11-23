#include "core/observability/game_monitor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "util/error_util.hpp"
#include "util/string_util.hpp"

namespace godot_autopilot {
namespace game_monitor {

namespace {

struct MonitorName {
  const char *name;
  const char *type;
};

const MonitorName kMonitors[] = {
    {"time/fps", "time"},
    {"time/process", "time"},
    {"time/physics_process", "time"},
    {"time/navigation_process", "time"},
    {"memory/static", "memory"},
    {"memory/static_max", "memory"},
    {"memory/message_buffer_max", "memory"},
    {"object/count", "quantity"},
    {"object/resource_count", "quantity"},
    {"object/node_count", "quantity"},
    {"object/orphan_node_count", "quantity"},
    {"render/total_objects_in_frame", "quantity"},
    {"render/total_primitives_in_frame", "quantity"},
    {"render/total_draw_calls_in_frame", "quantity"},
    {"render/video_mem_used", "memory"},
    {"render/texture_mem_used", "memory"},
    {"render/buffer_mem_used", "memory"},
    {"physics/2d/active_objects", "quantity"},
    {"physics/2d/collision_pairs", "quantity"},
    {"physics/2d/island_count", "quantity"},
    {"physics/3d/active_objects", "quantity"},
    {"physics/3d/collision_pairs", "quantity"},
    {"physics/3d/island_count", "quantity"},
    {"audio/output_latency", "time"},
    {"navigation/active_maps", "quantity"},
    {"navigation/region_count", "quantity"},
    {"navigation/agent_count", "quantity"},
    {"navigation/link_count", "quantity"},
    {"navigation/polygon_count", "quantity"},
    {"navigation/edge_count", "quantity"},
    {"navigation/edge_merge_count", "quantity"},
    {"navigation/edge_connection_count", "quantity"},
    {"navigation/edge_free_count", "quantity"},
    {"navigation/obstacle_count", "quantity"},
    {"pipeline/compilations_canvas", "quantity"},
    {"pipeline/compilations_mesh", "quantity"},
    {"pipeline/compilations_surface", "quantity"},
    {"pipeline/compilations_draw", "quantity"},
    {"pipeline/compilations_specialization", "quantity"},
    {"navigation/2d/active_maps", "quantity"},
    {"navigation/2d/region_count", "quantity"},
    {"navigation/2d/agent_count", "quantity"},
    {"navigation/2d/link_count", "quantity"},
    {"navigation/2d/polygon_count", "quantity"},
    {"navigation/2d/edge_count", "quantity"},
    {"navigation/2d/edge_merge_count", "quantity"},
    {"navigation/2d/edge_connection_count", "quantity"},
    {"navigation/2d/edge_free_count", "quantity"},
    {"navigation/2d/obstacle_count", "quantity"},
    {"navigation/3d/active_maps", "quantity"},
    {"navigation/3d/region_count", "quantity"},
    {"navigation/3d/agent_count", "quantity"},
    {"navigation/3d/link_count", "quantity"},
    {"navigation/3d/polygon_count", "quantity"},
    {"navigation/3d/edge_count", "quantity"},
    {"navigation/3d/edge_merge_count", "quantity"},
    {"navigation/3d/edge_connection_count", "quantity"},
    {"navigation/3d/edge_free_count", "quantity"},
    {"navigation/3d/obstacle_count", "quantity"},
};

constexpr int kMonitorCount = static_cast<int>(sizeof(kMonitors) / sizeof(kMonitors[0]));
constexpr int kFlushFrames = 120;
constexpr int64_t kFlushMicros = 1000000;

struct CustomInfo {
  std::string id;
  std::string category;
  int type = 0;
};

int64_t env_int(const char *name, int64_t fallback) {
  if (const char *value = std::getenv(name)) {
    char *end = nullptr;
    const long long parsed = std::strtoll(value, &end, 10);
    if (end != nullptr && *end == '\0' && parsed > 0) {
      return static_cast<int64_t>(parsed);
    }
  }
  return fallback;
}

int64_t max_files() { return env_int("GDA_MONITOR_MAX_FILES", 24); }

int64_t max_bytes() {
  return env_int("GDA_MONITOR_MAX_BYTES", 2LL * 1024LL * 1024LL * 1024LL);
}

struct PruneEntry {
  godot::String path;
  uint64_t modified = 0;
  uint64_t size = 0;
};

void prune_dir(const godot::String &dir_path) {
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(dir_path);
  if (dir.is_null() || dir->list_dir_begin() != godot::OK) {
    return;
  }
  std::vector<PruneEntry> entries;
  godot::String name = dir->get_next();
  while (name != godot::String()) {
    if (!dir->current_is_dir() && name.begins_with("gda-monitor-") &&
        name.ends_with(".jsonl")) {
      godot::String full = dir_path.path_join(name);
      godot::Ref<godot::FileAccess> probe =
          godot::FileAccess::open(full, godot::FileAccess::READ);
      uint64_t size = 0;
      if (probe.is_valid()) {
        size = static_cast<uint64_t>(std::max<int64_t>(0, probe->get_length()));
        probe->close();
      }
      entries.push_back({full, godot::FileAccess::get_modified_time(full), size});
    }
    name = dir->get_next();
  }
  std::sort(entries.begin(), entries.end(), [](const PruneEntry &a, const PruneEntry &b) {
    return a.modified < b.modified;
  });
  uint64_t total = 0;
  for (const PruneEntry &entry : entries) {
    total += entry.size;
  }
  const int64_t limit_files = max_files();
  const int64_t limit_bytes = max_bytes();
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const bool too_many = static_cast<int64_t>(entries.size() - i) > limit_files;
    const bool too_big = static_cast<int64_t>(total) > limit_bytes;
    if (!too_many && !too_big) {
      break;
    }
    total -= entries[i].size;
    godot::DirAccess::remove_absolute(entries[i].path);
  }
}

void append_double(std::string &out, double value) {
  if (!std::isfinite(value)) {
    out += "null";
    return;
  }
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "%.9g", value);
  out += buffer;
}

void append_quoted(std::string &out, const std::string &value) {
  out += '"';
  out += util::json_escape(value);
  out += '"';
}

std::string custom_type_name(int type) {
  switch (type) {
  case 1:
    return "memory";
  case 2:
    return "time";
  case 3:
    return "percentage";
  default:
    return "quantity";
  }
}

class Recorder {
public:
  Status status() const {
    Status out;
    out.active = active_;
    out.session_id = session_id_;
    out.path = path_;
    out.frames = frames_;
    out.custom_count = static_cast<uint64_t>(customs_.size());
    return out;
  }

  void start() {
    if (active_) {
      return;
    }
    auto *os = godot::OS::get_singleton();
    auto *time = godot::Time::get_singleton();
    auto *perf = godot::Performance::get_singleton();
    if (os == nullptr || time == nullptr || perf == nullptr) {
      return;
    }
    const godot::String base = os->get_user_data_dir();
    const godot::String dir = base.path_join("logs").path_join("monitor");
    godot::DirAccess::make_dir_recursive_absolute(dir);
    prune_dir(dir);

    const uint64_t ticks = static_cast<uint64_t>(time->get_ticks_usec());
    static uint64_t sequence = 0;
    char id_buffer[64];
    std::snprintf(id_buffer, sizeof(id_buffer), "%llu-%llu",
                  static_cast<unsigned long long>(ticks),
                  static_cast<unsigned long long>(++sequence));
    session_id_ = id_buffer;
    const std::string file_name = "gda-monitor-" + session_id_ + ".jsonl";
    path_ = util::to_std(dir.path_join(godot::String::utf8(file_name.c_str())));

    file_ = godot::FileAccess::open(godot::String::utf8(path_.c_str()),
                                   godot::FileAccess::WRITE);
    if (file_.is_null()) {
      path_.clear();
      session_id_.clear();
      return;
    }

    frames_ = 0;
    custom_count_ = 0;
    customs_.clear();
    buffer_.clear();
    last_mod_time_ = -1;
    start_wall_ms_ = static_cast<int64_t>(time->get_unix_time_from_system()) * 1000;
    start_mono_us_ = static_cast<int64_t>(time->get_ticks_usec());
    last_frame_us_ = start_mono_us_;
    last_flush_us_ = start_mono_us_;
    active_ = true;
    write_header();
    refresh_customs(perf);
    flush();
  }

  void tick() {
    if (!active_) {
      return;
    }
    auto *perf = godot::Performance::get_singleton();
    auto *time = godot::Time::get_singleton();
    if (perf == nullptr || time == nullptr || file_.is_null()) {
      return;
    }
    const int64_t now_us = static_cast<int64_t>(time->get_ticks_usec());
    const double delta_ms = static_cast<double>(now_us - last_frame_us_) / 1000.0;
    last_frame_us_ = now_us;
    const double elapsed_ms = static_cast<double>(now_us - start_mono_us_) / 1000.0;
    auto *engine = godot::Engine::get_singleton();
    const int64_t process_frame = engine != nullptr
                                      ? static_cast<int64_t>(engine->get_process_frames())
                                      : 0;
    const int64_t physics_frame = engine != nullptr
                                      ? static_cast<int64_t>(engine->get_physics_frames())
                                      : 0;

    refresh_customs(perf);

    std::string line;
    line.reserve(1024);
    line += "{\"type\":\"frame\",\"n\":";
    line += std::to_string(frames_ + 1);
    line += ",\"f\":";
    line += std::to_string(process_frame);
    line += ",\"pf\":";
    line += std::to_string(physics_frame);
    line += ",\"d\":";
    append_double(line, delta_ms);
    line += ",\"t\":";
    append_double(line, elapsed_ms);
    line += ",\"w\":";
    line += std::to_string(start_wall_ms_ + static_cast<int64_t>(elapsed_ms));
    line += ",\"v\":[";
    for (int i = 0; i < kMonitorCount; ++i) {
      if (i != 0) {
        line += ',';
      }
      append_double(line, perf->get_monitor(
                              static_cast<godot::Performance::Monitor>(i)));
    }
    line += "],\"c\":[";
    for (std::size_t i = 0; i < customs_.size(); ++i) {
      if (i != 0) {
        line += ',';
      }
      const godot::Variant value = perf->get_custom_monitor(
          godot::StringName(godot::String(customs_[i].id.c_str())));
      if (value.get_type() == godot::Variant::INT) {
        line += std::to_string(static_cast<int64_t>(value));
      } else if (value.get_type() == godot::Variant::FLOAT) {
        append_double(line, static_cast<double>(value));
      } else {
        line += "null";
      }
    }
    line += "]}\n";
    buffer_ += line;
    ++frames_;
    custom_count_ = static_cast<uint64_t>(customs_.size());
    if (frames_ % kFlushFrames == 0 || now_us - last_flush_us_ >= kFlushMicros) {
      flush();
      last_flush_us_ = now_us;
    }
  }

  void stop() {
    if (!active_) {
      return;
    }
    flush();
    if (file_.is_valid()) {
      file_->close();
      file_.unref();
    }
    active_ = false;
    customs_.clear();
  }

private:
  void write_header() {
    std::string line;
    line.reserve(4096);
    line += "{\"type\":\"session\",\"schema\":1,\"id\":";
    append_quoted(line, session_id_);
    line += ",\"started_wall_ms\":";
    line += std::to_string(start_wall_ms_);
    line += ",\"builtin_monitors\":[";
    for (int i = 0; i < kMonitorCount; ++i) {
      if (i != 0) {
        line += ',';
      }
      line += "{\"i\":";
      line += std::to_string(i);
      line += ",\"n\":";
      append_quoted(line, kMonitors[i].name);
      line += ",\"t\":";
      append_quoted(line, kMonitors[i].type);
      line += '}';
    }
    line += "],\"custom_monitors\":[]}\n";
    buffer_ += line;
  }

  void refresh_customs(godot::Performance *perf) {
    const int64_t modification = static_cast<int64_t>(
        perf->get_monitor_modification_time());
    if (modification == last_mod_time_) {
      return;
    }
    last_mod_time_ = modification;
    const godot::TypedArray<godot::StringName> names =
        perf->get_custom_monitor_names();
    const godot::PackedInt32Array types = perf->get_custom_monitor_types();
    customs_.clear();
    for (int i = 0; i < names.size(); ++i) {
      CustomInfo info;
      info.id = util::to_std(godot::String(names[i]));
      if (i < types.size()) {
        info.type = types[i];
      }
      const std::size_t slash = info.id.find('/');
      info.category = slash == std::string::npos ? "Custom" : info.id.substr(0, slash);
      customs_.push_back(std::move(info));
    }
    custom_count_ = static_cast<uint64_t>(customs_.size());
    std::string line;
    line += "{\"type\":\"custom_change\",\"n\":";
    line += std::to_string(frames_);
    line += ",\"custom_monitors\":[";
    for (std::size_t i = 0; i < customs_.size(); ++i) {
      if (i != 0) {
        line += ',';
      }
      line += "{\"i\":";
      line += std::to_string(i);
      line += ",\"n\":";
      append_quoted(line, customs_[i].id);
      line += ",\"t\":";
      append_quoted(line, custom_type_name(customs_[i].type));
      line += ",\"cat\":";
      append_quoted(line, customs_[i].category);
      line += '}';
    }
    line += "]}\n";
    buffer_ += line;
  }

  void flush() {
    if (buffer_.empty() || file_.is_null()) {
      return;
    }
    file_->store_string(godot::String::utf8(buffer_.c_str()));
    buffer_.clear();
  }

  bool active_ = false;
  std::string session_id_;
  std::string path_;
  uint64_t frames_ = 0;
  uint64_t custom_count_ = 0;
  int64_t start_wall_ms_ = 0;
  int64_t start_mono_us_ = 0;
  int64_t last_frame_us_ = 0;
  int64_t last_flush_us_ = 0;
  int64_t last_mod_time_ = -1;
  std::string buffer_;
  std::vector<CustomInfo> customs_;
  godot::Ref<godot::FileAccess> file_;
};

Recorder &recorder() {
  static Recorder instance;
  return instance;
}

godot::Ref<godot::RefCounted> g_listener;
godot::SceneTree *g_tree = nullptr;
bool g_connected = false;
bool g_class_registered = false;

class GameMonitorListener : public godot::RefCounted {
  GDCLASS(GameMonitorListener, godot::RefCounted)

protected:
  static void _bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("on_process_frame"),
                                &GameMonitorListener::on_process_frame);
  }

public:
  void on_process_frame() { recorder().tick(); }
};

godot::Callable listener_callable() {
  return godot::Callable(g_listener.ptr(),
                         godot::StringName("on_process_frame"));
}

void connect_listener(godot::SceneTree *tree) {
  if (g_connected || tree == nullptr) {
    return;
  }
  if (!g_class_registered) {
    godot::ClassDB::register_class<GameMonitorListener>();
    g_class_registered = true;
  }
  if (g_listener.is_null()) {
    g_listener.instantiate();
  }
  if (g_listener.is_null()) {
    return;
  }
  tree->connect("process_frame", listener_callable());
  g_tree = tree;
  g_connected = true;
  recorder().start();
}

} // namespace

Status status() {
  Status out = recorder().status();
  out.connected = g_connected;
  return out;
}

void ensure_connected() {
  if (g_connected) {
    return;
  }
  auto *engine = godot::Engine::get_singleton();
  if (engine == nullptr) {
    return;
  }
  auto *tree = godot::Object::cast_to<godot::SceneTree>(engine->get_main_loop());
  connect_listener(tree);
}

void stop() {
  if (g_connected && g_tree != nullptr && g_listener.is_valid()) {
    g_tree->disconnect("process_frame", listener_callable());
  }
  g_connected = false;
  g_tree = nullptr;
  recorder().stop();
  g_listener.unref();
}

std::string sessions_dir() {
  auto *os = godot::OS::get_singleton();
  if (os == nullptr) {
    return std::string();
  }
  return util::to_std(os->get_user_data_dir().path_join("logs").path_join("monitor"));
}

} // namespace game_monitor
} // namespace godot_autopilot
