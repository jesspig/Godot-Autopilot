#include "monitor.hpp"

#include <string>
#include <utility>

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <version.hpp>

#include "log_persist.hpp"
#include "mode_detector.hpp"
#include "plugin_config.hpp"
#include "sanitize_policy.hpp"

namespace godot_autopilot {
namespace monitor {

namespace {

std::string to_std(const godot::String &value) {
  return std::string(value.utf8().get_data());
}

std::string version_field(const godot::Dictionary &info, const char *key) {
  if (!info.has(key)) {
    return std::string();
  }
  return to_std(godot::String(info[key]));
}

} // namespace

void environment_snapshot() {
  std::string godot_version;
  if (auto *engine = godot::Engine::get_singleton()) {
    godot_version = version_field(engine->get_version_info(), "string");
  }

  double fps = 0.0;
  int64_t object_count = 0;
  if (auto *performance = godot::Performance::get_singleton()) {
    fps = performance->get_monitor(godot::Performance::TIME_FPS);
    object_count = static_cast<int64_t>(
        performance->get_monitor(godot::Performance::OBJECT_COUNT));
  }

  std::string os_name;
  std::string os_version;
  int64_t processors = 0;
  if (auto *os = godot::OS::get_singleton()) {
    os_name = to_std(os->get_name());
    os_version = to_std(os->get_version());
    processors = os->get_processor_count();
  }

  std::string scene_path;
  std::string scene_root;
  if (auto *editor = godot::EditorInterface::get_singleton()) {
    godot::Node *root = editor->get_edited_scene_root();
    if (root) {
      scene_path = to_std(root->get_scene_file_path());
      scene_root = to_std(root->get_name());
    }
  }

  TraceEvent event;
  event.kind = TraceKind::Snapshot;
  event.name = "session_start";
  event.ok = true;
  event.state = "succeeded";
  event.attrs = build_attrs({
      {"gda_version", GDA_VERSION},
      {"godot_version", godot_version},
      {"os_name", os_name},
      {"os_version", os_version},
      {"processors", std::to_string(processors)},
      {"object_count", std::to_string(object_count)},
      {"editor_mode", ModeDetector::is_editor() ? "true" : "false"},
      {"scene_path", scene_path},
      {"scene_root", scene_root},
      {"port", std::to_string(PluginConfig::load_port())},
      {"desensitize", sanitize_policy::enabled() ? "true" : "false"},
      {"show_time", PluginConfig::load_show_time() ? "true" : "false"},
      {"allow", PluginConfig::load_allow()},
      {"session_nonce", LogPersist::instance().session_id()},
  });
  emit(std::move(event));
}

} // namespace monitor
} // namespace godot_autopilot
