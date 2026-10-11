#ifndef GODOT_AUTOPILOT_GDSCRIPT_WRAP_HPP
#define GODOT_AUTOPILOT_GDSCRIPT_WRAP_HPP

#include <cctype>
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
    if (line.compare(pos, 12, "static func ") == 0)
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

inline bool has_func_definition_statement(const std::string &code) {
  std::istringstream stream(code);
  std::string line;
  while (std::getline(stream, line)) {
    size_t pos = line.find_first_not_of(" \t");
    if (pos == std::string::npos || line[pos] == '#')
      continue;
    if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/')
      continue;
    size_t head = pos;
    if (line.compare(head, 7, "static ") == 0)
      head += 7;
    if (head >= line.size())
      continue;
    if (line.compare(head, 4, "func") != 0)
      continue;
    char next = head + 4 < line.size() ? line[head + 4] : '\0';
    if (next == ' ' || next == '\t' || next == '(')
      return true;
  }
  return false;
}

inline bool is_plain_assignment(const std::string &text) {
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '=')
      continue;
    char prev = i > 0 ? text[i - 1] : '\0';
    char next = i + 1 < text.size() ? text[i + 1] : '\0';
    bool prev_is_op = prev == '=' || prev == '!' || prev == '<' || prev == '>';
    bool next_is_op = next == '=' || next == '!' || next == '<' || next == '>';
    if (!prev_is_op && !next_is_op)
      return true;
  }
  return false;
}

inline constexpr const char *SINGLE_EXPRESSION_BLOCKING_KEYWORDS[] = {
    "if",   "for",   "while", "match",  "func",  "return",
    "var",  "const", "class", "static", "break", "continue",
    "pass", "await", "try",   "assert", "super", "self",
};

inline bool is_single_expression(const std::string &text) {
  if (text.find('\n') != std::string::npos)
    return false;
  size_t start = text.find_first_not_of(" \t");
  if (start == std::string::npos || text[start] == '#')
    return false;
  if (is_plain_assignment(text))
    return false;
  size_t word_end = start;
  while (word_end < text.size()) {
    char c = text[word_end];
    if (c == ' ' || c == '\t' || c == '(' || c == ':')
      break;
    ++word_end;
  }
  std::string first_word = text.substr(start, word_end - start);
  for (const char *keyword : SINGLE_EXPRESSION_BLOCKING_KEYWORDS) {
    if (first_word == keyword)
      return false;
  }
  return true;
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

inline constexpr const char *WRAP_MODE_BARE = "bare";
inline constexpr const char *WRAP_MODE_EXPRESSION = "single_expression";
inline constexpr const char *WRAP_MODE_WHOLE_SCRIPT = "whole_script";

struct WrapResult {
  bool ok = false;
  std::string wrapped;
  int header_lines = 0;
  std::string error;
  std::string mode;
};

inline WrapResult wrap_bare_body(const std::string &source,
                                 const std::string &entry_name,
                                 const std::string &inject_line,
                                 bool single_expression_auto_return) {
  WrapResult result;
  std::string cleaned = strip_extends_lines(source);

  if (single_expression_auto_return && is_single_expression(source)) {
    result.mode = WRAP_MODE_EXPRESSION;
    result.wrapped = "@tool\nextends Node\n\n";
    result.header_lines = 3;
    if (!inject_line.empty()) {
      result.wrapped += inject_line + "\n";
      ++result.header_lines;
    }
    result.wrapped += "func " + entry_name + "():\n";
    result.wrapped += "    return " + cleaned + "\n";
    result.ok = true;
    return result;
  }

  result.mode = WRAP_MODE_BARE;
  if (has_func_definition_statement(cleaned)) {
    result.error =
        "func definition detected in single-function mode — position: "
        "source_code — expected: no func definitions while in "
        "single-function mode — action: top-level func definitions "
        "require multi-function mode; use editor script_create or wrap in "
        "a lambda";
    return result;
  }

  IndentStyle indent = scan_indent_style(cleaned);
  if (indent.uses_tabs && indent.uses_spaces) {
    result.error =
        "mixed tab/space indentation detected in source — position: "
        "source_code — expected: consistent indentation — action: reindent "
        "source with only tabs or only spaces; note the wrapper requires the "
        "same indentation style throughout";
    return result;
  }

  std::string prefix = indent_prefix(indent);
  result.wrapped = "@tool\nextends Node\n\n";
  result.header_lines = 3;
  if (!inject_line.empty()) {
    result.wrapped += inject_line + "\n";
    ++result.header_lines;
  }
  result.wrapped += "func " + entry_name + "():\n";
  result.wrapped += reindent_lines(cleaned, prefix);
  result.wrapped += "\n";
  result.ok = true;
  return result;
}

inline WrapResult wrap_whole_script(const std::string &source,
                                    const std::string &entry_name) {
  WrapResult result;
  result.mode = WRAP_MODE_WHOLE_SCRIPT;
  if (!defines_function_named(source, entry_name)) {
    result.error = "multi-function mode requires a func " + entry_name +
                   "() entry point（或将函数改为 lambda 变量）";
    return result;
  }
  result.ok = true;
  result.wrapped = source;
  result.header_lines = 0;
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

inline std::string map_error_line_numbers(const std::string &error_text,
                                          int line_offset) {
  if (line_offset <= 0)
    return error_text;
  std::string mapped;
  mapped.reserve(error_text.size());
  const std::string marker = "gdscript://";
  size_t pos = 0;
  while (true) {
    size_t mark = error_text.find(marker, pos);
    if (mark == std::string::npos) {
      mapped.append(error_text, pos, std::string::npos);
      break;
    }
    mapped.append(error_text, pos, mark - pos);
    size_t name_start = mark + marker.size();
    size_t name_end = error_text.find('.', name_start);
    bool is_gd_colon = name_end != std::string::npos &&
                       error_text.compare(name_end, 4, ".gd:") == 0;
    if (!is_gd_colon) {
      mapped.append(marker);
      pos = name_start;
      continue;
    }
    size_t num_start = name_end + 4;
    size_t num_end = num_start;
    while (num_end < error_text.size() &&
           std::isdigit(static_cast<unsigned char>(error_text[num_end]))) {
      ++num_end;
    }
    if (num_end == num_start) {
      mapped.append(marker);
      pos = name_start;
      continue;
    }
    std::string name = error_text.substr(name_start, name_end - name_start);
    int original = std::stoi(error_text.substr(num_start, num_end - num_start));
    if (original - line_offset > 0) {
      mapped += "gdscript://" + name + ".gd:" +
                std::to_string(original - line_offset) +
                " (mapped to user source line " +
                std::to_string(original - line_offset) + ")";
    } else {
      mapped += "gdscript://" + name + ".gd:" + std::to_string(original);
    }
    pos = num_end;
  }
  return mapped;
}

} // namespace gdscript_wrap
} // namespace godot_autopilot

#endif
