#ifndef GODOT_AUTOPILOT_AUTHORIZATION_HPP
#define GODOT_AUTOPILOT_AUTHORIZATION_HPP

#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
enum class SideEffect;
namespace authorization {

inline bool capability_enabled(std::string_view capability) {
  const char *raw = std::getenv("GODOT_AUTOPILOT_ALLOW");
  if (!raw || *raw == '\0')
    return false;
  std::string value(raw);
  size_t begin = 0;
  while (begin <= value.size()) {
    size_t end = value.find(',', begin);
    if (value.substr(begin, end == std::string::npos ? std::string::npos
                                                       : end - begin) == capability ||
        value.substr(begin, end == std::string::npos ? std::string::npos
                                                       : end - begin) == "all")
      return true;
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return false;
}

inline const char *capability_for_tool(std::string_view name,
                                       SideEffect effect);

inline mcp::JsonValue deny_if_unauthorized(std::string_view name,
                                           SideEffect effect) {
  const char *capability = capability_for_tool(name, effect);
  if (!capability || capability_enabled(capability))
    return mcp::JsonValue();
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(
      "authorization required for '" + std::string(name) +
      "'; set GODOT_AUTOPILOT_ALLOW to include '" + capability +
      "' for trusted local development");
  result["authorization_required"] = mcp::JsonValue(capability);
  return result;
}

} // namespace authorization
} // namespace godot_autopilot
#endif
