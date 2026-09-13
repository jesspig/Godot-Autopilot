#ifndef GODOT_AUTOPILOT_AUTHORIZATION_HPP
#define GODOT_AUTOPILOT_AUTHORIZATION_HPP

#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>

#include <mcp/JsonValue.hpp>

#include "core/log_system.hpp"

namespace godot_autopilot {
enum class SideEffect;
namespace authorization {

using AllowProvider = std::string (*)();

inline AllowProvider &allow_provider() {
  static AllowProvider provider = nullptr;
  return provider;
}

inline void set_allow_provider(AllowProvider provider) {
  allow_provider() = provider;
}

inline bool allow_list_contains(const std::string &value,
                                std::string_view capability) {
  size_t begin = 0;
  while (begin <= value.size()) {
    const size_t end = value.find(',', begin);
    const std::string entry =
        value.substr(begin, end == std::string::npos ? std::string::npos
                                                     : end - begin);
    if (entry == capability || entry == "all")
      return true;
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return false;
}

inline bool capability_enabled(std::string_view capability) {
  const char *raw = std::getenv("GODOT_AUTOPILOT_ALLOW");
  if (raw && *raw != '\0')
    return allow_list_contains(raw, capability);
  const AllowProvider provider = allow_provider();
  if (!provider)
    return false;
  return allow_list_contains(provider(), capability);
}

inline const char *capability_for_tool(std::string_view name,
                                       SideEffect effect);

inline mcp::JsonValue deny_if_unauthorized(std::string_view name,
                                           SideEffect effect) {
  const char *capability = capability_for_tool(name, effect);
  if (!capability || capability_enabled(capability))
    return mcp::JsonValue();
  LogSystem::instance().log(
      LogLevel::Warning, LogCategory::Tools,
      "authorization denied for tool '" + std::string(name) +
          "' (capability '" + capability + "')");
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(
      "authorization required for '" + std::string(name) +
      "'; set GODOT_AUTOPILOT_ALLOW to include '" + capability +
      "' for trusted local development");
  result["authorization_required"] = mcp::JsonValue(capability);
  result["enable"] = mcp::JsonValue(
      "set GODOT_AUTOPILOT_ALLOW to '" + std::string(capability) +
      "' (or 'all') and restart the engine, or tick \"Allow " + capability +
      "\" in the plugin's MCP Config dock (takes effect immediately)");
  return result;
}

} // namespace authorization
} // namespace godot_autopilot
#endif
