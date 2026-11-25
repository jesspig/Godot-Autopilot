#ifndef GODOT_AUTOPILOT_RESOURCE_SANITIZATION_HPP
#define GODOT_AUTOPILOT_RESOURCE_SANITIZATION_HPP

#include "core/observability/sanitize_policy.hpp"

#include <godot_cpp/classes/global_constants.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace resources {

struct ResourceValueRedaction {
  godot_autopilot::util::JsonValue value;
  bool redacted = false;
};

inline bool node_property_is_exported(uint64_t usage) {
  const uint64_t flags = usage;
  return (flags & godot::PROPERTY_USAGE_STORAGE) != 0 &&
         (flags & (godot::PROPERTY_USAGE_INTERNAL |
                   godot::PROPERTY_USAGE_GROUP |
                   godot::PROPERTY_USAGE_CATEGORY |
                   godot::PROPERTY_USAGE_SUBGROUP)) == 0;
}

inline bool node_property_metadata_is_sensitive(
    const std::map<std::string, std::string> &metadata) {
  for (const auto &entry : metadata) {
    if (sanitize_policy::metadata_marks_sensitive(entry.first, entry.second)) {
      return true;
    }
  }
  return false;
}

inline bool resource_value_is_sensitive(
    const std::string &key,
    const std::map<std::string, std::string> &metadata = {}) {
  return sanitize_policy::is_sensitive_key(key) ||
         node_property_metadata_is_sensitive(metadata);
}

inline ResourceValueRedaction redact_resource_value(
    const std::string &key, const godot_autopilot::util::JsonValue &value,
    const std::map<std::string, std::string> &metadata = {},
    const std::vector<std::string> &allowed_keys = {}) {
  if (sanitize_policy::sensitive_key_is_allowed(key, allowed_keys)) {
    return {value, false};
  }
  if (resource_value_is_sensitive(key, metadata)) {
    return {godot_autopilot::util::JsonValue(sanitize_policy::kRedactedValue), true};
  }
  sanitize_policy::RedactionResult redacted =
      sanitize_policy::redact_sensitive_values(value, allowed_keys);
  return {std::move(redacted.value), redacted.redacted};
}

inline godot_autopilot::util::JsonValue resource_value_response(
    const ResourceValueRedaction &redaction) {
  if (!redaction.redacted) {
    return redaction.value;
  }
  godot_autopilot::util::JsonValue response(godot_autopilot::util::JsonValue::object_tag);
  response["value"] = redaction.value;
  response["redacted"] = godot_autopilot::util::JsonValue(true);
  return response;
}

inline godot_autopilot::util::JsonValue redacted_resource_response() {
  return resource_value_response(
      {godot_autopilot::util::JsonValue(sanitize_policy::kRedactedValue), true});
}

} // namespace resources
} // namespace godot_autopilot

#endif
