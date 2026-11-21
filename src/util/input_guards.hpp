#ifndef GODOT_AUTOPILOT_INPUT_GUARDS_HPP
#define GODOT_AUTOPILOT_INPUT_GUARDS_HPP

#include <mcp/JsonValue.hpp>

#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#ifdef GetObject
#pragma push_macro("GetObject")
#undef GetObject
#define GDA_INPUT_GUARDS_POP_GETOBJECT 1
#endif

namespace godot_autopilot {
namespace util {

inline constexpr int64_t kMinPhysicsFps = 1;
inline constexpr int64_t kMaxPhysicsFps = 1000;
inline constexpr int64_t kMinMaxFps = 0;
inline constexpr int64_t kMaxMaxFps = 1000;
inline constexpr double kMaxTimeScale = 100.0;

inline bool physics_fps_is_valid(int64_t fps) {
  return fps >= kMinPhysicsFps && fps <= kMaxPhysicsFps;
}

inline bool max_fps_is_valid(int64_t fps) {
  return fps >= kMinMaxFps && fps <= kMaxMaxFps;
}

inline bool time_scale_is_valid(double scale) {
  return std::isfinite(scale) && scale >= 0.0 && scale <= kMaxTimeScale;
}

inline bool is_decimal_digits(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  for (char c : text) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

inline bool parse_decimal_int64(const std::string &text, int64_t &out) {
  if (!is_decimal_digits(text)) {
    return false;
  }
  try {
    size_t consumed = 0;
    out = std::stoll(text, &consumed);
    return consumed == text.size();
  } catch (...) {
    return false;
  }
}

inline bool parse_signed_decimal_int64(const std::string &text, int64_t &out) {
  if (text.empty()) {
    return false;
  }
  size_t pos = 0;
  if (text[0] == '-') {
    pos = 1;
    if (text.size() == 1) {
      return false;
    }
  }
  for (; pos < text.size(); ++pos) {
    if (text[pos] < '0' || text[pos] > '9') {
      return false;
    }
  }
  try {
    size_t consumed = 0;
    out = std::stoll(text, &consumed);
    return consumed == text.size();
  } catch (...) {
    return false;
  }
}

inline bool try_parse_json(const std::string &text, mcp::JsonValue &out) {
  try {
    out = mcp::JsonValue::Parse(text);
    return true;
  } catch (...) {
    return false;
  }
}

inline char ascii_tolower(char c) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

inline std::string ascii_lowercase(const std::string &text) {
  std::string out = text;
  for (char &c : out) {
    c = ascii_tolower(c);
  }
  return out;
}

enum class StringArgStatus { Ok, Missing, WrongType, Empty };

inline StringArgStatus string_arg_status(const mcp::JsonValue &args,
                                         const char *key, std::string &out,
                                         bool require_nonempty) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    return StringArgStatus::Missing;
  }
  if (!value->IsString()) {
    return StringArgStatus::WrongType;
  }
  out = value->GetString();
  if (require_nonempty && out.empty()) {
    return StringArgStatus::Empty;
  }
  return StringArgStatus::Ok;
}

inline bool require_nonempty_string_arg(const mcp::JsonValue &args,
                                        const char *key, std::string &out) {
  return string_arg_status(args, key, out, true) == StringArgStatus::Ok;
}

struct ObjectLocatorFields {
  std::string name;
  std::string path;
  bool has_object_id = false;
  int64_t object_id = 0;
};

enum class LocatorFieldStatus { Ok, UnknownField, WrongType, OutOfRange };

inline LocatorFieldStatus parse_object_locator(
    const mcp::JsonValue &value, bool allow_path, ObjectLocatorFields &out,
    const char **bad_field = nullptr) {
  for (const auto &entry : value.GetObject()) {
    const bool known =
        entry.first == "name" || entry.first == "object_id" ||
        entry.first == "object_id_str" || (allow_path && entry.first == "path");
    if (!known) {
      if (bad_field != nullptr) {
        *bad_field = entry.first.c_str();
      }
      return LocatorFieldStatus::UnknownField;
    }
  }
  const mcp::JsonValue *name_p = value.Find("name");
  if (name_p != nullptr) {
    if (!name_p->IsString()) {
      if (bad_field != nullptr) *bad_field = "name";
      return LocatorFieldStatus::WrongType;
    }
    out.name = name_p->GetString();
  }
  const mcp::JsonValue *path_p = value.Find("path");
  if (allow_path && path_p != nullptr) {
    if (!path_p->IsString()) {
      if (bad_field != nullptr) *bad_field = "path";
      return LocatorFieldStatus::WrongType;
    }
    out.path = path_p->GetString();
  }
  const mcp::JsonValue *oid_p = value.Find("object_id");
  if (oid_p != nullptr) {
    if (!oid_p->IsNumber()) {
      if (bad_field != nullptr) *bad_field = "object_id";
      return LocatorFieldStatus::WrongType;
    }
    out.has_object_id = true;
    out.object_id = oid_p->IsInt() ? oid_p->GetInt()
                                   : static_cast<int64_t>(oid_p->GetDouble());
  }
  const mcp::JsonValue *oid_str_p = value.Find("object_id_str");
  if (oid_str_p != nullptr) {
    if (!oid_str_p->IsString() ||
        !is_decimal_digits(oid_str_p->GetString())) {
      if (bad_field != nullptr) *bad_field = "object_id_str";
      return LocatorFieldStatus::WrongType;
    }
    if (!parse_decimal_int64(oid_str_p->GetString(), out.object_id)) {
      if (bad_field != nullptr) *bad_field = "object_id_str";
      return LocatorFieldStatus::OutOfRange;
    }
    out.has_object_id = true;
  }
  return LocatorFieldStatus::Ok;
}

inline std::string parse_locator_string(const std::string &text,
                                        bool allow_path,
                                        ObjectLocatorFields &out,
                                        bool &out_of_range) {
  out_of_range = false;
  if (text.rfind("memory://", 0) == 0) {
    out.name = text.substr(9);
  } else if (allow_path &&
             (text.rfind("res://", 0) == 0 || text.rfind("user://", 0) == 0)) {
    out.path = text;
  } else if (is_decimal_digits(text)) {
    if (!parse_decimal_int64(text, out.object_id)) {
      out_of_range = true;
    }
    out.has_object_id = true;
  } else if (!text.empty()) {
    out.name = text;
  }
  return text;
}

inline std::string res_uri(const std::string &path) {
  if (path.rfind("res://", 0) == 0) {
    return path;
  }
  return "res://" + path;
}

inline std::string received_keys_text(const mcp::JsonValue &args,
                                      bool leading_space) {
  const std::string prefix = leading_space ? " " : "";
  if (!args.IsObject() || args.GetObject().empty()) {
    return prefix + "(received no parameters)";
  }
  std::string text = prefix + "(received keys: ";
  bool first = true;
  for (const auto &entry : args.GetObject()) {
    if (!first) {
      text += ", ";
    }
    first = false;
    text += entry.first;
  }
  text += ")";
  return text;
}

inline std::string received_keys_suffix(const mcp::JsonValue &args) {
  return received_keys_text(args, true);
}

inline std::string escape_bbcode(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '[') {
      out += "[lb]";
    } else if (c == ']') {
      out += "[rb]";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

inline bool resource_template_value(
    const std::map<std::string, std::string> &vars, const std::string &key,
    std::string &out) {
  const auto it = vars.find(key);
  if (it == vars.end()) {
    return false;
  }
  out = it->second;
  return true;
}

inline bool normalize_resource_uri_path(const std::string &raw,
                                        std::string &out) {
  std::string path = raw;
  for (char &c : path) {
    if (c == '\\') {
      c = '/';
    }
  }
  if (path.rfind("res://", 0) == 0) {
    path = path.substr(6);
  }
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= path.size()) {
    const size_t end = path.find('/', start);
    const std::string part = path.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (part.empty() || part == ".") {
    } else if (part == "..") {
      if (parts.empty()) {
        return false;
      }
      parts.pop_back();
    } else {
      parts.push_back(part);
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  std::string clean;
  for (const std::string &part : parts) {
    if (!clean.empty()) {
      clean += "/";
    }
    clean += part;
  }
  if (clean.empty()) {
    return false;
  }
  out = "res://" + clean;
  return true;
}

} // namespace util
} // namespace godot_autopilot

#ifdef GDA_INPUT_GUARDS_POP_GETOBJECT
#pragma pop_macro("GetObject")
#undef GDA_INPUT_GUARDS_POP_GETOBJECT
#endif

#endif
