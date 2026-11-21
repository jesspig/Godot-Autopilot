#ifndef GODOT_AUTOPILOT_STRING_UTIL_HPP
#define GODOT_AUTOPILOT_STRING_UTIL_HPP

#include <cstdio>
#include <string>

namespace godot_autopilot {
namespace util {

inline std::string json_escape(const std::string &text) {
  std::string out;
  out.reserve(text.size() + 2);
  for (unsigned char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
        break;
    }
  }
  return out;
}

inline void append_json_escaped(std::string &out, const std::string &text) {
  out += json_escape(text);
}

} // namespace util
} // namespace godot_autopilot

#endif
