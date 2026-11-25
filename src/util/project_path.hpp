#ifndef GODOT_AUTOPILOT_PROJECT_PATH_HPP
#define GODOT_AUTOPILOT_PROJECT_PATH_HPP

#include <godot_cpp/classes/project_settings.hpp>
#include "error_util.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
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

inline std::string project_path_root_prefix(std::string path) {
  path = project_path_slashes(std::move(path));
  if (path.size() == 3 && path[1] == ':' && path[2] == '/') {
    return path;
  }
  while (path.size() > 1 && path.back() == '/') {
    path.pop_back();
  }
  return path;
}

inline std::string project_path_lexical(const std::string &path,
                                         bool &escaped) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= path.size()) {
    const size_t end = path.find('/', start);
    const std::string part = path.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (part.empty() || part == ".") {
    } else if (part == "..") {
      if (!parts.empty() && parts.back() != "..")
        parts.pop_back();
      else {
        escaped = true;
        return {};
      }
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
  if (result.size() == 2 && result[1] == ':') {
    return result + "/";
  }
  if (!path.empty() && path[0] == '/') {
    return result.empty() ? "/" : "/" + result;
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

inline char project_path_prefix_char(char value) {
#ifdef _WIN32
  if (value >= 'A' && value <= 'Z') {
    return static_cast<char>(value + ('a' - 'A'));
  }
#endif
  return value;
}

inline bool project_path_equal_prefix(const std::string &path,
                                      const std::string &root) {
  const std::string normalized_root = project_path_root_prefix(root);
  if (normalized_root.empty() || path.size() < normalized_root.size())
    return false;
  for (size_t i = 0; i < normalized_root.size(); ++i) {
    char left = project_path_prefix_char(path[i]);
    char right = project_path_prefix_char(normalized_root[i]);
    if (left != right)
      return false;
  }
  if (normalized_root == "/" ||
      (normalized_root.size() == 3 && normalized_root[2] == '/'))
    return true;
  return path.size() == normalized_root.size() ||
         path[normalized_root.size()] == '/';
}

enum class ProjectPathResolveStatus { Inside, Outside, Unresolved };

inline bool containment_status_allows(ProjectPathResolveStatus status) {
  return status == ProjectPathResolveStatus::Inside;
}

inline std::function<std::string(const std::string &)> &scheme_globalize_provider() {
  static std::function<std::string(const std::string &)> provider;
  return provider;
}

inline void set_scheme_globalize_provider(std::function<std::string(const std::string &)> provider) {
  scheme_globalize_provider() = std::move(provider);
}

inline bool project_path_globalize(const std::string &scheme_path,
                                   std::string &absolute_path) {
  absolute_path.clear();
  const auto &provider = scheme_globalize_provider();
  if (!provider) {
    return false;
  }
  try {
    absolute_path = project_path_slashes(provider(scheme_path));
  } catch (...) {
    return false;
  }
  return !absolute_path.empty() && project_path_is_absolute(absolute_path);
}

inline bool project_path_components_resolve(
    const std::filesystem::path &path, bool allow_file_final) {
  std::filesystem::path component = path;
  bool first = true;
  while (!component.empty()) {
    std::error_code ec;
    const std::filesystem::file_status status =
        std::filesystem::status(component, ec);
    if (ec) {
      if (ec != std::errc::no_such_file_or_directory) {
        return false;
      }
    } else if (std::filesystem::exists(status) &&
               (!std::filesystem::is_directory(status) &&
                (!first || !allow_file_final))) {
      return false;
    }
    const std::filesystem::path parent = component.parent_path();
    if (parent == component) {
      break;
    }
    component = parent;
    first = false;
  }
  return true;
}

inline ProjectPathResolveStatus project_path_resolve_status(
    const std::string &absolute_path, const std::string &absolute_root) {
  if (!project_path_is_absolute(absolute_path) ||
      !project_path_is_absolute(absolute_root)) {
    return ProjectPathResolveStatus::Unresolved;
  }
  const std::filesystem::path path(absolute_path);
  const std::filesystem::path root(absolute_root);
  if (!project_path_components_resolve(path, true) ||
      !project_path_components_resolve(root, false)) {
    return ProjectPathResolveStatus::Unresolved;
  }
  std::error_code ec;
  const std::filesystem::path canonical_root =
      std::filesystem::weakly_canonical(root, ec);
  if (ec)
    return ProjectPathResolveStatus::Unresolved;
  const std::filesystem::path canonical_path =
      std::filesystem::weakly_canonical(path, ec);
  if (ec)
    return ProjectPathResolveStatus::Unresolved;
  const std::string root_str =
      project_path_root_prefix(canonical_root.generic_string());
  const std::string path_str =
      project_path_slashes(canonical_path.generic_string());
  return project_path_equal_prefix(path_str, root_str)
             ? ProjectPathResolveStatus::Inside
             : ProjectPathResolveStatus::Outside;
}

inline ProjectPath normalize_project_path(const std::string &raw,
                                           bool allow_user, bool allow_root) {
  const std::string input = project_path_slashes(raw);
  if (input.empty())
    return {{}, "path is empty"};

  if (project_path_is_absolute(input)) {
    std::string absolute_root;
    if (!project_path_globalize("res://", absolute_root)) {
      return {{}, "project root could not be resolved for absolute path validation"};
    }
    const std::string root_prefix = project_path_root_prefix(absolute_root);
    bool absolute_escaped = false;
    const std::string absolute = project_path_lexical(input, absolute_escaped);
    if (absolute_escaped || absolute.empty() ||
        !project_path_equal_prefix(absolute, root_prefix))
      return {{}, "absolute path is outside the project root"};
    const ProjectPathResolveStatus status =
        project_path_resolve_status(absolute, absolute_root);
    if (status == ProjectPathResolveStatus::Outside)
      return {{}, "absolute path resolves outside the project root"};
    if (status == ProjectPathResolveStatus::Unresolved)
      return {{}, "absolute path containment could not be resolved"};
    std::string relative = absolute.substr(root_prefix.size());
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

  bool escaped = false;
  const std::string clean = project_path_lexical(body, escaped);
  if (escaped)
    return {{}, "path escapes the namespace root"};
  if (clean.empty() && !allow_root)
    return {{}, "path must name an item below the namespace root"};

  std::string absolute_root;
  std::string absolute;
  const std::string namespace_root = scheme + "://";
  const std::string scheme_path = scheme + "://" + clean;
  if (!project_path_globalize(namespace_root, absolute_root) ||
      !project_path_globalize(scheme_path, absolute)) {
    return {{}, "path containment could not be resolved"};
  }
  const ProjectPathResolveStatus status =
      project_path_resolve_status(absolute, absolute_root);
  if (status == ProjectPathResolveStatus::Outside)
    return {{}, "path resolves outside the namespace root"};
  if (status == ProjectPathResolveStatus::Unresolved)
    return {{}, "path containment could not be resolved"};
  return {scheme_path, {}};
}

inline std::string current_project_root_path() {
  auto *settings = godot::ProjectSettings::get_singleton();
  if (!settings)
    return std::string();
  return project_path_root_prefix(
      util::to_std(settings->globalize_path("res://")));
}

} // namespace util
} // namespace godot_autopilot

#endif
