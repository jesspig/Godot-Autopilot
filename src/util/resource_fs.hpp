#ifndef GODOT_AUTOPILOT_RESOURCE_FS_HPP
#define GODOT_AUTOPILOT_RESOURCE_FS_HPP

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>

namespace godot_autopilot {
namespace resource_fs {

inline bool is_resource_root_dir(const std::string &dir) {
  return dir.empty() || dir == "res://" || dir == "res:/" || dir == "res:" ||
         dir == "user://" || dir == "user:/" || dir == "user:";
}

inline std::string parent_directory_of(const std::string &resource_path) {
  const godot::String path = godot::String::utf8(resource_path.c_str());
  return std::string(path.get_base_dir().utf8().get_data());
}

inline bool ensure_parent_directory(const std::string &resource_path,
                                    std::string &dir_created,
                                    std::string &error_out) {
  dir_created.clear();
  error_out.clear();
  const std::string parent = parent_directory_of(resource_path);
  if (is_resource_root_dir(parent)) {
    return true;
  }
  const godot::String parent_gs = godot::String::utf8(parent.c_str());
  if (godot::DirAccess::dir_exists_absolute(parent_gs)) {
    return true;
  }
  const godot::Error mk_err =
      godot::DirAccess::make_dir_recursive_absolute(parent_gs);
  if (mk_err != godot::Error::OK) {
    error_out = "failed to create directory: " + parent + " (error " +
                std::to_string(static_cast<int>(mk_err)) + ")";
    return false;
  }
  dir_created = parent;
  return true;
}

} // namespace resource_fs
} // namespace godot_autopilot

#endif
