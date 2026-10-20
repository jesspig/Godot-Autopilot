#ifndef GODOT_AUTOPILOT_GDSCRIPT_WRAP_HPP
#define GODOT_AUTOPILOT_GDSCRIPT_WRAP_HPP

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

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

constexpr const char *DEADLINE_CONST_NAME = "__gda_deadline_ms";
constexpr const char *TIMEOUT_SENTINEL_NAME = "__GDA_TIMEOUT__";
constexpr const char *TIMEOUT_SENTINEL_VALUE = "__GDA_TIMEOUT__";

inline std::string deadline_declaration(int64_t deadline_ms) {
  return std::string("const ") + DEADLINE_CONST_NAME + " := " +
         std::to_string(deadline_ms);
}

inline std::string timeout_sentinel_declaration() {
  return std::string("const ") + TIMEOUT_SENTINEL_NAME + " := \"" +
         TIMEOUT_SENTINEL_VALUE + "\"";
}

inline std::string deadline_check_line() {
  return std::string("if Time.get_ticks_msec() > ") + DEADLINE_CONST_NAME +
         ": return " + TIMEOUT_SENTINEL_NAME;
}

inline std::string deadline_break_line() {
  return std::string("if Time.get_ticks_msec() > ") + DEADLINE_CONST_NAME +
         ": break";
}

inline bool declares_return_type(const std::string &signature) {
  return signature.find("->") != std::string::npos;
}

inline bool is_function_signature_line(const std::string &trimmed) {
  return trimmed.compare(0, 5, "func ") == 0 ||
         trimmed.compare(0, 11, "static func ") == 0;
}

inline std::string deadline_loop_condition() {
  return std::string("Time.get_ticks_msec() <= ") + DEADLINE_CONST_NAME;
}

inline bool is_timeout_sentinel_text(const std::string &text) {
  return text == TIMEOUT_SENTINEL_VALUE;
}

inline std::string leading_whitespace(const std::string &line) {
  size_t index = 0;
  while (index < line.size() &&
         (line[index] == ' ' || line[index] == '\t')) {
    ++index;
  }
  return line.substr(0, index);
}

inline std::string trim_whitespace(const std::string &text) {
  size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\t')) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\t')) {
    --end;
  }
  return text.substr(begin, end - begin);
}

inline bool is_comment_only(const std::string &trimmed) {
  return !trimmed.empty() && trimmed[0] == '#';
}

inline bool is_while_header(const std::string &trimmed) {
  return trimmed.compare(0, 6, "while ") == 0;
}

inline bool is_for_header(const std::string &trimmed) {
  return trimmed.compare(0, 4, "for ") == 0;
}

inline size_t find_top_level_colon(const std::string &text) {
  int depth = 0;
  char quote = 0;
  for (size_t index = 0; index < text.size(); ++index) {
    const char current = text[index];
    if (quote != 0) {
      if (current == quote) {
        quote = 0;
      }
      continue;
    }
    if (current == '"' || current == '\'') {
      quote = current;
      continue;
    }
    if (current == '(' || current == '[' || current == '{') {
      ++depth;
      continue;
    }
    if (current == ')' || current == ']' || current == '}') {
      --depth;
      continue;
    }
    if (current == ':' && depth == 0) {
      return index;
    }
  }
  return std::string::npos;
}

inline std::vector<std::string> split_lines(const std::string &text) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
  return lines;
}

inline std::string join_lines(const std::vector<std::string> &lines) {
  std::string out;
  for (size_t index = 0; index < lines.size(); ++index) {
    if (index > 0) {
      out += "\n";
    }
    out += lines[index];
  }
  return out;
}

struct DeadlineTransform {
  bool ok = false;
  std::string source;
  std::string error;
};

inline DeadlineTransform inject_deadline_checks(const std::string &source) {
  DeadlineTransform result;
  result.ok = true;
  const std::vector<std::string> lines = split_lines(source);
  const std::string check = deadline_check_line();
  const std::string break_check = deadline_break_line();
  std::vector<std::string> out;
  out.reserve(lines.size());
  bool enclosing_function_typed = false;
  int64_t enclosing_function_indent = -1;
  for (size_t index = 0; index < lines.size(); ++index) {
    const std::string &current = lines[index];
    const std::string indent = leading_whitespace(current);
    const std::string trimmed = current.substr(indent.size());
    if (!trimmed.empty() && is_function_signature_line(trimmed)) {
      enclosing_function_typed = declares_return_type(trimmed);
      enclosing_function_indent = static_cast<int64_t>(indent.size());
    } else if (!trimmed.empty() && enclosing_function_indent >= 0 &&
               static_cast<int64_t>(indent.size()) <=
                   enclosing_function_indent) {
      enclosing_function_typed = false;
      enclosing_function_indent = -1;
    }
    const bool loop_header =
        !trimmed.empty() && !is_comment_only(trimmed) &&
        (is_while_header(trimmed) || is_for_header(trimmed));
    if (!loop_header) {
      out.push_back(current);
      continue;
    }
    const size_t colon = find_top_level_colon(trimmed);
    if (colon == std::string::npos) {
      out.push_back(current);
      continue;
    }
    const std::string head = trimmed.substr(0, colon);
    const std::string body = trim_whitespace(trimmed.substr(colon + 1));
    const std::string &loop_check =
        enclosing_function_typed ? break_check : check;
    if (body.empty()) {
      out.push_back(current);
      size_t scan = index + 1;
      while (scan < lines.size()) {
        const std::string next_indent = leading_whitespace(lines[scan]);
        const std::string next_trimmed =
            lines[scan].substr(next_indent.size());
        if (next_trimmed.empty() || is_comment_only(next_trimmed)) {
          ++scan;
          continue;
        }
        if (next_indent.size() > indent.size()) {
          out.push_back(next_indent + loop_check);
        }
        break;
      }
      continue;
    }
    if (is_while_header(trimmed)) {
      out.push_back(indent + "while (" + head.substr(6) + ") and " +
                    deadline_loop_condition() + ": " + body);
      continue;
    }
    out.push_back(current);
  }
  result.source = join_lines(out);
  return result;
}

inline std::string insert_after_header(const std::string &source,
                                       const std::string &declarations) {
  const std::vector<std::string> lines = split_lines(source);
  std::vector<std::string> out;
  out.reserve(lines.size() + 2);
  size_t index = 0;
  while (index < lines.size()) {
    const std::string trimmed = trim_whitespace(lines[index]);
    const bool blank = trimmed.empty();
    const bool header = !blank && (trimmed[0] == '@' ||
                                   trimmed.compare(0, 8, "extends ") == 0 ||
                                   trimmed.compare(0, 11, "class_name ") == 0);
    if (!blank && !header) {
      break;
    }
    out.push_back(lines[index]);
    ++index;
  }
  for (const std::string &declaration : split_lines(declarations)) {
    out.push_back(declaration);
  }
  for (; index < lines.size(); ++index) {
    out.push_back(lines[index]);
  }
  return join_lines(out);
}

inline std::string insert_entry_check(const std::string &source,
                                      const std::string &entry_name,
                                      const std::string &check) {
  const std::vector<std::string> lines = split_lines(source);
  const std::string signature = "func " + entry_name + "(";
  std::vector<std::string> out;
  out.reserve(lines.size() + 1);
  bool inserted = false;
  for (size_t index = 0; index < lines.size(); ++index) {
    const std::string &current = lines[index];
    out.push_back(current);
    if (inserted) {
      continue;
    }
    const std::string indent = leading_whitespace(current);
    const std::string trimmed = current.substr(indent.size());
    if (trimmed.compare(0, signature.size(), signature) != 0) {
      continue;
    }
    if (declares_return_type(trimmed)) {
      continue;
    }
    size_t scan = index + 1;
    std::string body_indent;
    while (scan < lines.size()) {
      const std::string next_indent = leading_whitespace(lines[scan]);
      const std::string next_trimmed = lines[scan].substr(next_indent.size());
      if (next_trimmed.empty() || is_comment_only(next_trimmed)) {
        ++scan;
        continue;
      }
      if (next_indent.size() > indent.size()) {
        body_indent = next_indent;
      }
      break;
    }
    if (!body_indent.empty()) {
      out.push_back(body_indent + check);
      inserted = true;
    }
  }
  return join_lines(out);
}

} // namespace gdscript_wrap
} // namespace godot_autopilot

#endif
