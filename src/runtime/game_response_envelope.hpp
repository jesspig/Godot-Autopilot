#ifndef GODOT_AUTOPILOT_RUNTIME_GAME_RESPONSE_ENVELOPE_HPP
#define GODOT_AUTOPILOT_RUNTIME_GAME_RESPONSE_ENVELOPE_HPP

#include <mcp/JsonValue.hpp>

#include <utility>

namespace godot_autopilot {
namespace runtime {
namespace response_envelope {

inline constexpr const char *kTransportFields[] = {"wrapped", "next_seq"};

inline bool has_transport_field(const mcp::JsonValue &game_body) {
  for (const char *key : kTransportFields) {
    if (game_body.Find(key) != nullptr)
      return true;
  }
  return false;
}

inline mcp::JsonValue carry_transport_fields(const mcp::JsonValue &game_body,
                                             mcp::JsonValue payload) {
  if (!game_body.IsObject() || !has_transport_field(game_body))
    return payload;
  if (!payload.IsObject()) {
    mcp::JsonValue envelope(mcp::JsonValue::object_tag);
    envelope["result"] = std::move(payload);
    payload = std::move(envelope);
  }
  for (const char *key : kTransportFields) {
    const mcp::JsonValue *value = game_body.Find(key);
    if (value != nullptr && !payload.Contains(key))
      payload[key] = *value;
  }
  return payload;
}

} // namespace response_envelope
} // namespace runtime
} // namespace godot_autopilot

#endif
