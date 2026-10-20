#ifndef GODOT_AUTOPILOT_API_HOST_CONTEXT_HPP
#define GODOT_AUTOPILOT_API_HOST_CONTEXT_HPP

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>

#include <string>

namespace godot_autopilot {
namespace util {

inline int api_host_process_id() {
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return 0;
  }
  return static_cast<int>(os->get_process_id());
}

inline std::string api_host_project_root() {
  auto *settings = godot::ProjectSettings::get_singleton();
  if (!settings) {
    return std::string();
  }
  std::string root =
      std::string(settings->globalize_path("res://").utf8().get_data());
  while (root.size() > 1 && (root.back() == '/' || root.back() == '\\')) {
    root.pop_back();
  }
  return root;
}

} // namespace util
} // namespace godot_autopilot

#endif
