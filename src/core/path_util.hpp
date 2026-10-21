#ifndef GODOT_AUTOPILOT_PATH_UTIL_HPP
#define GODOT_AUTOPILOT_PATH_UTIL_HPP

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <string>

namespace godot_autopilot {

inline std::filesystem::path to_fs_path(const std::string &value) {
  return std::filesystem::u8path(value);
}

inline std::string normalize_path_separators(const std::string &path) {
  std::string out = path;
  for (char &ch : out) {
    if (ch == '\\') {
      ch = '/';
    }
  }
  return out;
}

inline std::string trim_path_trailing_separators(const std::string &path) {
  std::string out = normalize_path_separators(path);
  while (out.size() > 1 && out.back() == '/') {
    out.pop_back();
  }
  return out;
}

inline bool is_filesystem_root(const std::string &path) {
  const std::string trimmed = trim_path_trailing_separators(path);
  if (trimmed.empty()) {
    return false;
  }
  if (trimmed == "/") {
    return true;
  }
  if (trimmed.size() == 2 && trimmed[1] == ':' &&
      std::isalpha(static_cast<unsigned char>(trimmed[0])) != 0) {
    return true;
  }
  return false;
}

inline std::string join_path(const std::string &base, const std::string &leaf) {
  if (base.empty()) {
    return leaf;
  }
  if (leaf.empty()) {
    return base;
  }
  std::string out = base;
  if (out.back() != '/') {
    out += '/';
  }
  out += leaf;
  return out;
}

inline std::string path_basename(const std::string &path) {
  const std::string trimmed = trim_path_trailing_separators(path);
  const std::size_t pos = trimmed.find_last_of('/');
  if (pos == std::string::npos) {
    return trimmed;
  }
  return trimmed.substr(pos + 1);
}

inline std::string parent_dir(const std::string &path, int levels) {
  std::string current = trim_path_trailing_separators(path);
  for (int level = 0; level < levels; ++level) {
    const std::size_t pos = current.find_last_of('/');
    if (pos == std::string::npos) {
      return std::string();
    }
    if (pos == 0) {
      return "/";
    }
    current = current.substr(0, pos);
  }
  return current;
}

inline std::string sanitize_path_component(const std::string &value) {
  static const char kIllegal[] = "\\/:*?\"<>|";
  std::string out = value;
  for (char &ch : out) {
    for (const char *cursor = kIllegal; *cursor != '\0'; ++cursor) {
      if (ch == *cursor) {
        ch = '_';
        break;
      }
    }
  }
  return out;
}

} // namespace godot_autopilot

#endif
