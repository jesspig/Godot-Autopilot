#ifndef GODOT_AUTOPILOT_CAPTURE_RETENTION_HPP
#define GODOT_AUTOPILOT_CAPTURE_RETENTION_HPP

#include <string>

namespace godot_autopilot {
namespace capture_retention {

inline std::string normalized_directory_key(const std::string &path) {
  std::string key = path;
  for (char &c : key) {
    if (c == '\\')
      c = '/';
  }
  while (key.size() > 1 && key.back() == '/')
    key.pop_back();
  return key;
}

inline bool prune_allowed(const std::string &directory,
                          const std::string &captures_root) {
  if (directory.empty() || captures_root.empty())
    return false;
  const std::string dir = normalized_directory_key(directory);
  const std::string root = normalized_directory_key(captures_root);
  if (dir == root)
    return true;
  return dir.size() > root.size() && dir.compare(0, root.size(), root) == 0 &&
         dir[root.size()] == '/';
}

} // namespace capture_retention
} // namespace godot_autopilot

#endif
