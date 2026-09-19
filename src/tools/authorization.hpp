#ifndef GODOT_AUTOPILOT_AUTHORIZATION_HPP
#define GODOT_AUTOPILOT_AUTHORIZATION_HPP

#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

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

inline constexpr const char *kKnownCapabilities[] = {"process", "code_execute",
                                                     "game_runtime",
                                                     "user_tools"};

inline bool capability_has_dock_toggle(std::string_view capability) {
  return capability == "code_execute" || capability == "game_runtime" ||
         capability == "user_tools";
}

inline std::string allow_list_add(std::string allow,
                                  std::string_view capability) {
  if (allow_list_contains(allow, capability))
    return allow;
  if (allow.empty())
    return std::string(capability);
  return allow + "," + std::string(capability);
}

inline std::string allow_list_remove(std::string allow,
                                     std::string_view capability) {
  std::vector<std::string> entries;
  size_t begin = 0;
  while (begin <= allow.size()) {
    const size_t end = allow.find(',', begin);
    const std::string entry = allow.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin);
    if (!entry.empty()) {
      if (entry == "all") {
        for (const char *known : kKnownCapabilities)
          entries.push_back(known);
      } else {
        entries.push_back(entry);
      }
    }
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  std::string out;
  for (const std::string &entry : entries) {
    if (entry == capability || allow_list_contains(out, entry))
      continue;
    if (!out.empty())
      out += ',';
    out += entry;
  }
  return out;
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
  std::string enable =
      "set GODOT_AUTOPILOT_ALLOW to '" + std::string(capability) +
      "' (or 'all') and restart the engine";
  if (capability_has_dock_toggle(capability)) {
    enable += ", or tick \"Allow " + std::string(capability) +
              "\" in the plugin's MCP Config dock (takes effect immediately)";
  }
  result["enable"] = mcp::JsonValue(enable);
  return result;
}

} // namespace authorization
} // namespace godot_autopilot
#endif
