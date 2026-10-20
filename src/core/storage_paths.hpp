#ifndef GODOT_AUTOPILOT_STORAGE_PATHS_HPP
#define GODOT_AUTOPILOT_STORAGE_PATHS_HPP

#include <string>

namespace godot_autopilot {

class StoragePaths {
public:
  static std::string global_config_path();
  static std::string project_dir(const std::string &project_path);
  static std::string project_config_path(const std::string &project_path);
  static std::string log_dir(const std::string &project_path);
  static std::string trace_dir(const std::string &project_path);
  static std::string captures_dir(const std::string &project_path);
};

} // namespace godot_autopilot

#endif
