#ifndef GODOT_AUTOPILOT_TEST_PROJECT_PATH_PROVIDER_HPP
#define GODOT_AUTOPILOT_TEST_PROJECT_PATH_PROVIDER_HPP

#include "util/project_path.hpp"

#include <filesystem>
#include <string>

namespace godot_autopilot_test {

class ProjectPathProviderGuard {
public:
  ProjectPathProviderGuard() {
    const std::filesystem::path root = std::filesystem::current_path();
    const std::filesystem::path user =
        std::filesystem::temp_directory_path() / "gda_user_path_unit";
    godot_autopilot::util::set_scheme_globalize_provider(
        [root, user](const std::string &path) -> std::string {
          if (path.rfind("res://", 0) == 0) {
            return (root / path.substr(6)).generic_string();
          }
          if (path.rfind("user://", 0) == 0) {
            return (user / path.substr(7)).generic_string();
          }
          return {};
        });
  }

  ~ProjectPathProviderGuard() {
    godot_autopilot::util::set_scheme_globalize_provider({});
  }

  ProjectPathProviderGuard(const ProjectPathProviderGuard &) = delete;
  ProjectPathProviderGuard &
  operator=(const ProjectPathProviderGuard &) = delete;
};

} // namespace godot_autopilot_test

#endif
