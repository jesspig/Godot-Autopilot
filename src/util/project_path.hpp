#ifndef GODOT_AUTOPILOT_PROJECT_PATH_HPP
#define GODOT_AUTOPILOT_PROJECT_PATH_HPP

#include <godot_cpp/classes/project_settings.hpp>
#include "error_util.hpp"
#include <string>
#include <vector>

namespace godot_autopilot {
namespace util {

struct ProjectPath {
  std::string value;
  std::string error;

  bool valid() const { return error.empty(); }
};

inline std::string project_path_slashes(std::string path) {
  for (char &c : path) {
    if (c == '\\')
      c = '/';
  }
  return path;
}

inline std::string project_path_lexical(const std::string &path) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= path.size()) {
    const size_t end = path.find('/', start);
    const std::string part = path.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (part.empty() || part == ".") {
      // 忽略重复分隔符和当前目录片段。
    } else if (part == "..") {
      if (!parts.empty() && parts.back() != "..")
        parts.pop_back();
      else
        return {};
    } else {
      parts.push_back(part);
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }

  std::string result;
  for (const std::string &part : parts) {
    if (!result.empty())
      result += "/";
    result += part;
  }
  return result;
}

inline bool project_path_is_absolute(const std::string &path) {
  return (!path.empty() && path[0] == '/') ||
         (path.size() >= 3 &&
          ((path[0] >= 'A' && path[0] <= 'Z') ||
           (path[0] >= 'a' && path[0] <= 'z')) &&
          path[1] == ':' && path[2] == '/');
}

inline bool project_path_equal_prefix(const std::string &path,
                                      const std::string &root) {
  if (path.size() < root.size())
    return false;
  for (size_t i = 0; i < root.size(); ++i) {
    char left = path[i];
    char right = root[i];
    if (left >= 'A' && left <= 'Z')
      left = static_cast<char>(left + ('a' - 'A'));
    if (right >= 'A' && right <= 'Z')
      right = static_cast<char>(right + ('a' - 'A'));
    if (left != right)
      return false;
  }
  return path.size() == root.size() || path[root.size()] == '/';
}

inline ProjectPath normalize_project_path(const std::string &raw,
                                          bool allow_user,
                                          bool allow_root = true) {
  const std::string input = project_path_slashes(raw);
  if (input.empty())
    return {{}, "path is empty"};

  if (project_path_is_absolute(input)) {
    auto *settings = godot::ProjectSettings::get_singleton();
    if (!settings)
      return {{}, "ProjectSettings is not available for absolute path validation"};
    std::string absolute_root =
        project_path_slashes(util::to_std(settings->globalize_path("res://")));
    while (absolute_root.size() > 1 && absolute_root.back() == '/')
      absolute_root.pop_back();
    std::string absolute = project_path_lexical(input);
    if (absolute.empty() || !project_path_equal_prefix(absolute, absolute_root))
      return {{}, "absolute path is outside the project root"};
    std::string relative = absolute.substr(absolute_root.size());
    if (!relative.empty() && relative[0] == '/')
      relative.erase(0, 1);
    if (relative.empty() && !allow_root)
      return {{}, "path must name an item below the project root"};
    return {"res://" + relative, {}};
  }

  std::string scheme;
  std::string body = input;
  const size_t scheme_end = input.find("://");
  if (scheme_end != std::string::npos) {
    scheme = input.substr(0, scheme_end);
    body = input.substr(scheme_end + 3);
    if (scheme != "res" && scheme != "user")
      return {{}, "unsupported path scheme"};
    if (scheme == "user" && !allow_user)
      return {{}, "user:// paths are not allowed for this operation"};
  } else if (input.find(':') != std::string::npos) {
    return {{}, "malformed or unsupported path scheme"};
  } else {
    scheme = "res";
  }

  const std::string clean = project_path_lexical(body);
  if (clean.empty() && !allow_root)
    return {{}, "path must name an item below the namespace root"};
  return {scheme + "://" + clean, {}};
}

} // namespace util
} // namespace godot_autopilot

#endif
