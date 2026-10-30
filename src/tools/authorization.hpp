#ifndef GODOT_AUTOPILOT_AUTHORIZATION_HPP
#define GODOT_AUTOPILOT_AUTHORIZATION_HPP

#include <cstddef>
#include <cstdlib>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <mcp/JsonValue.hpp>

#include "core/command_queue.hpp"
#include "core/log_system.hpp"
#include "tools/runtime_ops.hpp"

namespace godot_autopilot {
enum class SideEffect;
namespace authorization {

using AllowProvider = std::string (*)();
using AuthModeProvider = std::string (*)();

enum class CapabilityTier { Read, Write, Danger };

enum class AuthMode { Strict, Legacy };

inline constexpr const char *kAuthModeStrict = "strict";
inline constexpr const char *kAuthModeLegacy = "legacy";
inline constexpr const char *kEnvAuthMode = "GODOT_AUTOPILOT_AUTH_MODE";
inline constexpr const char *kConfigAuthModeKey = "auth_mode";

inline constexpr const char *kCapabilityProcess = "process";
inline constexpr const char *kCapabilityCodeExecute = "code_execute";
inline constexpr const char *kCapabilityGameRuntime = "game_runtime";
inline constexpr const char *kCapabilityDestructive = "destructive";
inline constexpr const char *kCapabilityUserTools = "user_tools";
inline constexpr const char *kCapabilityRead = "read";
inline constexpr const char *kCapabilityWrite = "write";

inline constexpr const char *kKnownCapabilities[] = {
    kCapabilityProcess, kCapabilityCodeExecute, kCapabilityGameRuntime,
    kCapabilityDestructive, kCapabilityUserTools, kCapabilityRead,
    kCapabilityWrite};

inline constexpr const char *kDockToggleCapabilities[] = {
    kCapabilityCodeExecute, kCapabilityGameRuntime, kCapabilityUserTools,
    kCapabilityWrite, kCapabilityDestructive, kCapabilityProcess};

inline AllowProvider &allow_provider() {
  static AllowProvider provider = nullptr;
  return provider;
}

inline void set_allow_provider(AllowProvider provider) {
  allow_provider() = provider;
}

inline AuthModeProvider &auth_mode_provider() {
  static AuthModeProvider provider = nullptr;
  return provider;
}

inline void set_auth_mode_provider(AuthModeProvider provider) {
  auth_mode_provider() = provider;
}

inline constexpr const char *kEnvAllow = "GODOT_AUTOPILOT_ALLOW";

inline bool may_read_config_directly() {
  if (!runtime_ops::has_editor_queue())
    return true;
  return get_editor_queue().is_main_thread();
}

struct ConfigSnapshot {
  std::string allow_list;
  std::string auth_mode;
  bool allow_ready = false;
  bool auth_mode_ready = false;
};

inline std::mutex &snapshot_mutex() {
  static std::mutex mutex;
  return mutex;
}

inline ConfigSnapshot &snapshot_slot() {
  static ConfigSnapshot snapshot;
  return snapshot;
}

inline void publish_allow_snapshot(const std::string &allow_list) {
  std::lock_guard<std::mutex> lock(snapshot_mutex());
  ConfigSnapshot &snapshot = snapshot_slot();
  snapshot.allow_list = allow_list;
  snapshot.allow_ready = true;
}

inline void publish_auth_mode_snapshot(const std::string &auth_mode) {
  std::lock_guard<std::mutex> lock(snapshot_mutex());
  ConfigSnapshot &snapshot = snapshot_slot();
  snapshot.auth_mode = auth_mode;
  snapshot.auth_mode_ready = true;
}

inline bool read_allow_snapshot(std::string &out) {
  std::lock_guard<std::mutex> lock(snapshot_mutex());
  const ConfigSnapshot &snapshot = snapshot_slot();
  if (!snapshot.allow_ready)
    return false;
  out = snapshot.allow_list;
  return true;
}

inline bool read_auth_mode_snapshot(std::string &out) {
  std::lock_guard<std::mutex> lock(snapshot_mutex());
  const ConfigSnapshot &snapshot = snapshot_slot();
  if (!snapshot.auth_mode_ready)
    return false;
  out = snapshot.auth_mode;
  return true;
}

inline bool config_snapshot_ready() {
  std::lock_guard<std::mutex> lock(snapshot_mutex());
  const ConfigSnapshot &snapshot = snapshot_slot();
  return snapshot.allow_ready && snapshot.auth_mode_ready;
}

inline void reset_config_snapshot() {
  std::lock_guard<std::mutex> lock(snapshot_mutex());
  snapshot_slot() = ConfigSnapshot();
}

inline void refresh_config_snapshot() {
  const AllowProvider allow = allow_provider();
  if (allow)
    publish_allow_snapshot(allow());
  const AuthModeProvider mode = auth_mode_provider();
  if (mode)
    publish_auth_mode_snapshot(mode());
}

inline bool parse_auth_mode(std::string_view raw, AuthMode &out) {
  if (raw == kAuthModeStrict) {
    out = AuthMode::Strict;
    return true;
  }
  if (raw == kAuthModeLegacy) {
    out = AuthMode::Legacy;
    return true;
  }
  return false;
}

inline const char *auth_mode_name(AuthMode mode) {
  return mode == AuthMode::Legacy ? kAuthModeLegacy : kAuthModeStrict;
}

inline void note_invalid_auth_mode(std::string_view raw,
                                   const char *source) {
  static std::mutex mutex;
  static std::set<std::string> reported;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!reported.insert(std::string(raw)).second)
      return;
  }
  LogSystem::instance().log(
      LogLevel::Warning, LogCategory::Tools,
      "invalid authorization mode '" + std::string(raw) + "' from " +
          std::string(source) +
          "; falling back to the strict mode (write and dangerous tools stay "
          "denied)");
}

inline AuthMode current_auth_mode() {
  const char *raw = std::getenv(kEnvAuthMode);
  if (raw && *raw != '\0') {
    AuthMode mode = AuthMode::Strict;
    if (parse_auth_mode(raw, mode))
      return mode;
    note_invalid_auth_mode(raw, kEnvAuthMode);
    return AuthMode::Strict;
  }
  std::string value;
  if (may_read_config_directly()) {
    const AuthModeProvider provider = auth_mode_provider();
    if (!provider)
      return AuthMode::Strict;
    value = provider();
    publish_auth_mode_snapshot(value);
  } else if (!read_auth_mode_snapshot(value)) {
    return AuthMode::Strict;
  }
  if (value.empty())
    return AuthMode::Strict;
  AuthMode mode = AuthMode::Strict;
  if (parse_auth_mode(value, mode))
    return mode;
  note_invalid_auth_mode(value, kConfigAuthModeKey);
  return AuthMode::Strict;
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

inline bool capability_has_dock_toggle(std::string_view capability) {
  for (const char *toggle : kDockToggleCapabilities) {
    if (capability == toggle)
      return true;
  }
  return false;
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

inline std::string current_allow_list() {
  const char *raw = std::getenv(kEnvAllow);
  if (raw && *raw != '\0')
    return std::string(raw);
  if (may_read_config_directly()) {
    const AllowProvider provider = allow_provider();
    if (!provider)
      return std::string();
    const std::string value = provider();
    publish_allow_snapshot(value);
    return value;
  }
  std::string cached;
  if (read_allow_snapshot(cached))
    return cached;
  return std::string();
}

inline bool capability_enabled(std::string_view capability) {
  return allow_list_contains(current_allow_list(), capability);
}

inline bool is_read_capability(std::string_view capability) {
  return capability == kCapabilityRead;
}

inline bool tier_permits(std::string_view capability,
                         std::string_view allow_list, AuthMode mode) {
  if (capability.empty())
    return true;
  if (is_read_capability(capability))
    return true;
  if (mode == AuthMode::Legacy && capability == kCapabilityWrite)
    return true;
  return allow_list_contains(std::string(allow_list), capability);
}

inline bool tier_permits(std::string_view capability,
                         std::string_view allow_list) {
  return tier_permits(capability, allow_list, current_auth_mode());
}

inline const char *capability_for_tool(std::string_view name,
                                       SideEffect effect);

inline mcp::JsonValue deny_if_unauthorized(std::string_view name,
                                           SideEffect effect) {
  const char *capability = capability_for_tool(name, effect);
  if (!capability)
    return mcp::JsonValue();
  const AuthMode mode = current_auth_mode();
  if (tier_permits(capability, current_allow_list(), mode))
    return mcp::JsonValue();
  LogSystem::instance().log(
      LogLevel::Warning, LogCategory::Tools,
      "authorization denied for tool '" + std::string(name) +
          "' (capability '" + capability + "', mode '" +
          auth_mode_name(mode) + "')");
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(
      "authorization required for '" + std::string(name) +
      "'; set GODOT_AUTOPILOT_ALLOW to include '" + capability +
      "' for trusted local development");
  result["code"] = mcp::JsonValue("authorization_required");
  result["authorization_required"] = mcp::JsonValue(capability);
  result["authorization_mode"] = mcp::JsonValue(auth_mode_name(mode));
  std::string enable =
      "set GODOT_AUTOPILOT_ALLOW to '" + std::string(capability) +
      "' (or 'all') and restart the engine";
  if (capability_has_dock_toggle(capability)) {
    enable += ", or tick \"Allow " + std::string(capability) +
              "\" in the plugin's MCP Config dock (takes effect immediately)";
  }
  if (mode == AuthMode::Strict && capability == kCapabilityWrite) {
    enable +=
        "; or switch the authorization mode to 'legacy' "
        "(GODOT_AUTOPILOT_AUTH_MODE=legacy, or pick \"Compatibility\" in the "
        "MCP Config dock) to restore the pre-0.3.0 behavior where write "
        "tools need no authorization";
  }
  result["enable"] = mcp::JsonValue(enable);
  return result;
}

} // namespace authorization
} // namespace godot_autopilot
#endif
