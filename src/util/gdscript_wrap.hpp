#ifndef GODOT_AUTOPILOT_GDSCRIPT_WRAP_HPP
#define GODOT_AUTOPILOT_GDSCRIPT_WRAP_HPP

#include <cstddef>
#include <sstream>
#include <string>

namespace godot_autopilot {
namespace gdscript_wrap {

constexpr size_t MAX_CAPTURE_BYTES = 8192;

constexpr const char *NODE_NOT_FOUND_HINT =
    "\n[hint] Node path resolution: the execution node lives under /root, NOT "
    "inside the edited scene. Use SceneRoot.get_node(\"Child\") to reach "
    "edited-scene nodes (SceneRoot is the scene root node itself — no "
    "root-name prefix, e.g. SceneRoot.get_node(\"Player\") or "
    "SceneRoot.get_node(\"Player/CollisionShape2D\")).";

inline std::string truncate_capture_text(const std::string &text) {
  if (text.size() > MAX_CAPTURE_BYTES) {
    return text.substr(0, MAX_CAPTURE_BYTES) + "\n...(truncated, total " +
           std::to_string(text.size()) + " bytes)";
  }
  return text;
}

inline std::string strip_extends_lines(const std::string &code) {
  std::string result;
  std::istringstream stream(code);
  std::string line;
  bool first = true;
  while (std::getline(stream, line)) {
    if (!first)
      result += "\n";
    first = false;
    size_t pos = line.find_first_not_of(" \t");
    if (pos != std::string::npos && line.compare(pos, 8, "extends ") == 0) {
      result += "# " + line;
    } else {
      result += line;
    }
  }
  return result;
}

inline bool has_top_level_func_def(const std::string &code) {
  std::istringstream stream(code);
  std::string line;
  while (std::getline(stream, line)) {
    size_t pos = line.find_first_not_of(" \t");
    if (pos == std::string::npos || line[pos] == '#')
      continue;
    if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/')
      continue;
    if (line.compare(pos, 5, "func ") == 0)
      return true;
  }
  return false;
}

inline bool defines_function_named(const std::string &code,
                                   const std::string &target) {
  std::istringstream stream(code);
  std::string line;
  while (std::getline(stream, line)) {
    size_t pos = line.find_first_not_of(" \t");
    if (pos == std::string::npos || line[pos] == '#')
      continue;
    if (line.compare(pos, 5, "func ") != 0)
      continue;
    size_t name_start = line.find_first_not_of(" \t", pos + 5);
    if (name_start == std::string::npos)
      continue;
    size_t name_end = line.find('(', name_start);
    if (name_end == std::string::npos)
      continue;
    std::string fname = line.substr(name_start, name_end - name_start);
    size_t last = fname.find_last_not_of(" \t");
    if (last != std::string::npos)
      fname = fname.substr(0, last + 1);
    if (fname == target)
      return true;
  }
  return false;
}

struct IndentStyle {
  bool uses_tabs = false;
  bool uses_spaces = false;
};

inline IndentStyle scan_indent_style(const std::string &code) {
  IndentStyle style;
  std::istringstream stream(code);
  std::string line;
  while (std::getline(stream, line)) {
    size_t pos = line.find_first_not_of(" \t");
    if (pos == std::string::npos || pos == 0)
      continue;
    std::string indent = line.substr(0, pos);
    if (indent.find('\t') != std::string::npos)
      style.uses_tabs = true;
    if (indent.find("    ") != std::string::npos)
      style.uses_spaces = true;
  }
  return style;
}

inline std::string indent_prefix(const IndentStyle &style) {
  return (style.uses_tabs && !style.uses_spaces) ? "\t" : "    ";
}

inline std::string reindent_lines(const std::string &code,
                                  const std::string &prefix) {
  std::string result;
  if (code.empty())
    return result;
  std::istringstream stream(code);
  std::string line;
  bool first_line = true;
  while (std::getline(stream, line)) {
    if (!first_line)
      result += "\n";
    first_line = false;
    size_t content_start = line.find_first_not_of(" \t");
    if (content_start == std::string::npos) {
      result += prefix;
    } else {
      result += prefix + line;
    }
  }
  return result;
}

template <typename FormatCaptured>
inline std::string
compose_compile_failure_message(const std::string &header,
                                const std::string &captured_errors,
                                const std::string &wrapped_source,
                                FormatCaptured format_captured) {
  std::string message = header;
  if (!captured_errors.empty()) {
    message += "\n" + format_captured(captured_errors);
  }
  message += "\nwrapped source:\n" + wrapped_source;
  return message;
}

} // namespace gdscript_wrap
} // namespace godot_autopilot

#endif
