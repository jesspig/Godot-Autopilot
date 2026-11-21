#ifndef GODOT_AUTOPILOT_INPUT_GUARDS_HPP
#define GODOT_AUTOPILOT_INPUT_GUARDS_HPP

#include <mcp/JsonValue.hpp>

#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

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

inline bool parse_decimal_int64(const std::string &text, int64_t &out) {
  if (text.empty()) {
    return false;
  }
  for (char c : text) {
    if (c < '0' || c > '9') {
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

#endif
