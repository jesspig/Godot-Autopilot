#ifndef GODOT_AUTOPILOT_SANITIZE_POLICY_HPP
#define GODOT_AUTOPILOT_SANITIZE_POLICY_HPP

#include <atomic>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace sanitize_policy {

constexpr const char *kEnvKey = "GODOT_AUTOPILOT_DESENSITIZE";
inline constexpr const char *kRedactedValue = "<REDACTED>";

namespace detail {

inline std::string normalized_key(std::string_view raw) {
  std::string key;
  key.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    const unsigned char current = static_cast<unsigned char>(raw[i]);
    if (std::isalnum(current)) {
      if (i > 0 && std::isupper(current)) {
        const unsigned char previous = static_cast<unsigned char>(raw[i - 1]);
        const unsigned char next = i + 1 < raw.size()
                                       ? static_cast<unsigned char>(raw[i + 1])
                                       : 0;
        if (std::islower(previous) || std::isdigit(previous) ||
            (std::isupper(previous) && std::islower(next))) {
          if (!key.empty() && key.back() != '_') {
            key += '_';
          }
        }
      }
      key += static_cast<char>(std::tolower(current));
    } else if (!key.empty() && key.back() != '_') {
      key += '_';
    }
  }
  while (!key.empty() && key.back() == '_') {
    key.pop_back();
  }
  return key;
}

inline bool has_key_token(const std::string &key, std::string_view token) {
  size_t start = 0;
  while ((start = key.find(token, start)) != std::string::npos) {
    const size_t end = start + token.size();
    const bool left = start == 0 || key[start - 1] == '_';
    const bool right = end == key.size() || key[end] == '_';
    if (left && right) {
      return true;
    }
    start = end;
  }
  return false;
}

inline std::string lowercase_trimmed(std::string_view raw) {
  std::string value;
  value.reserve(raw.size());
  for (unsigned char c : raw) {
    if (!std::isspace(c) && c != '"' && c != '\'') {
      value += static_cast<char>(std::tolower(c));
    }
  }
  return value;
}

inline bool metadata_value_is_truthy(std::string_view raw) {
  const std::string value = lowercase_trimmed(raw);
  return !value.empty() && value != "false" && value != "0" &&
         value != "off" && value != "no" && value != "null";
}

inline std::atomic<bool> &cached_enabled() {
  static std::atomic<bool> value{true};
  return value;
}

inline std::optional<bool> env_declared(const char *raw) {
  if (raw == nullptr || *raw == '\0') {
    return std::nullopt;
  }
  const std::string value(raw);
  return !(value == "0" || value == "false" || value == "off");
}

} // namespace detail

inline bool is_sensitive_key(std::string_view raw_key) {
  const std::string key = detail::normalized_key(raw_key);
  if (key.empty()) {
    return false;
  }
  static constexpr std::string_view kExact[] = {
      "data",     "base64",  "script_content", "apikey", "passwd",
      "password", "pass_word", "passphrase", "secret", "token", "key", "auth",
      "authorization", "credential", "credentials", "cookie"};
  for (std::string_view candidate : kExact) {
    if (key == candidate) {
      return true;
    }
  }
  static constexpr std::string_view kTokens[] = {
      "token", "password", "passwd", "secret", "credential",
      "credentials", "authorization", "cookie", "key"};
  for (std::string_view token : kTokens) {
    if (detail::has_key_token(key, token)) {
      return true;
    }
  }
  return false;
}

inline bool sensitive_key_is_allowed(
    std::string_view raw_key, const std::vector<std::string> &allowed_keys) {
  const std::string key = detail::normalized_key(raw_key);
  for (const std::string &allowed : allowed_keys) {
    if (key == detail::normalized_key(allowed)) {
      return true;
    }
  }
  return false;
}

inline bool metadata_marks_sensitive(std::string_view metadata_key,
                                     std::string_view metadata_value) {
  const std::string key = detail::normalized_key(metadata_key);
  if (key == "hint" || key == "hint_string") {
    const std::string value = detail::lowercase_trimmed(metadata_value);
    return value.find("password") != std::string::npos ||
           value.find("passwd") != std::string::npos ||
           value.find("secret") != std::string::npos ||
           value.find("credential") != std::string::npos ||
           value.find("authorization") != std::string::npos ||
           value.find("token") != std::string::npos ||
           value.find("api_key") != std::string::npos ||
           value.find("api key") != std::string::npos ||
           value.find("access_key") != std::string::npos ||
           value.find("access key") != std::string::npos ||
           value.find("private_key") != std::string::npos ||
           value.find("private key") != std::string::npos ||
           value.find("apikey") != std::string::npos;
  }
  static constexpr std::string_view kMetadataTokens[] = {
      "sensitive", "secret", "password", "passwd", "credential",
      "credentials", "restricted", "private"};
  for (std::string_view token : kMetadataTokens) {
    if (detail::has_key_token(key, token)) {
      return detail::metadata_value_is_truthy(metadata_value);
    }
  }
  return false;
}

struct RedactionResult {
  mcp::JsonValue value;
  bool redacted = false;
};

namespace detail {

inline RedactionResult redact_json_impl(
    const mcp::JsonValue &value, const std::vector<std::string> &allowed_keys) {
  if (value.IsObject()) {
    RedactionResult result{mcp::JsonValue(mcp::JsonValue::object_tag), false};
    for (const auto &entry : value.GetObject()) {
      if (sensitive_key_is_allowed(entry.first, allowed_keys)) {
        result.value[entry.first] = entry.second;
        continue;
      }
      if (is_sensitive_key(entry.first)) {
        result.value[entry.first] = mcp::JsonValue(kRedactedValue);
        result.redacted = true;
        continue;
      }
      RedactionResult child = redact_json_impl(entry.second, allowed_keys);
      result.value[entry.first] = std::move(child.value);
      result.redacted = result.redacted || child.redacted;
    }
    return result;
  }
  if (value.IsArray()) {
    RedactionResult result{mcp::JsonValue(mcp::JsonValue::array_tag), false};
    for (const mcp::JsonValue &item : value.GetArray()) {
      RedactionResult child = redact_json_impl(item, allowed_keys);
      result.value.PushBack(std::move(child.value));
      result.redacted = result.redacted || child.redacted;
    }
    return result;
  }
  return {value, false};
}

} // namespace detail

inline RedactionResult redact_sensitive_values(
    const mcp::JsonValue &value,
    const std::vector<std::string> &allowed_keys = {}) {
  return detail::redact_json_impl(value, allowed_keys);
}

inline bool resolve_enabled(const char *env_raw, bool config_value) {
  return detail::env_declared(env_raw).value_or(config_value);
}

inline bool enabled() { return detail::cached_enabled().load(); }

inline void set_enabled(bool value) {
  detail::cached_enabled().store(value);
}

void initialize();

} // namespace sanitize_policy
} // namespace godot_autopilot

#endif
