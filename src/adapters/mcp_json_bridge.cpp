#include "adapters/mcp_json_bridge.hpp"

#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace adapters {

JsonValue FromSdk(const SdkJson &value) {
  if (value.IsNull()) {
    return JsonValue(nullptr);
  }
  if (value.IsBool()) {
    return JsonValue(value.GetBool());
  }
  if (value.IsInt()) {
    return JsonValue(value.GetInt());
  }
  if (value.IsDouble()) {
    return JsonValue(value.GetDouble());
  }
  if (value.IsString()) {
    return JsonValue(value.GetString());
  }
  if (value.IsArray()) {
    JsonValue::Array out;
    const SdkJson::Array &items = value.GetArray();
    out.reserve(items.size());
    for (const SdkJson &item : items) {
      out.push_back(FromSdk(item));
    }
    return JsonValue(std::move(out));
  }
  JsonValue::Object out;
  const SdkJson::Object &entries = value.GetObject();
  for (const auto &entry : entries) {
    out.emplace(entry.first, FromSdk(entry.second));
  }
  return JsonValue(std::move(out));
}

SdkJson ToSdk(const JsonValue &value) {
  if (value.IsNull()) {
    return SdkJson(nullptr);
  }
  if (value.IsBool()) {
    return SdkJson(value.GetBool());
  }
  if (value.IsInt()) {
    return SdkJson(value.GetInt());
  }
  if (value.IsDouble()) {
    return SdkJson(value.GetDouble());
  }
  if (value.IsString()) {
    return SdkJson(value.GetString());
  }
  if (value.IsArray()) {
    SdkJson::Array out;
    const JsonValue::Array &items = value.GetArray();
    out.reserve(items.size());
    for (const JsonValue &item : items) {
      out.push_back(ToSdk(item));
    }
    return SdkJson(std::move(out));
  }
  SdkJson::Object out;
  const JsonValue::Object &entries = value.GetObject();
  for (const auto &entry : entries) {
    out.emplace(entry.first, ToSdk(entry.second));
  }
  return SdkJson(std::move(out));
}

std::optional<JsonValue> FromSdkOptional(const std::optional<SdkJson> &value) {
  if (!value.has_value()) {
    return std::nullopt;
  }
  return FromSdk(*value);
}

std::optional<SdkJson> ToSdkOptional(const std::optional<JsonValue> &value) {
  if (!value.has_value()) {
    return std::nullopt;
  }
  return ToSdk(*value);
}

} // namespace adapters
} // namespace godot_autopilot
