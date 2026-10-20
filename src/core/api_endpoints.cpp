#include "core/api_endpoints.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace godot_autopilot {
namespace api_endpoints {

namespace {

mcp::JsonValue endpoint_to_json(const char *name, const EndpointEntry &entry) {
  mcp::JsonValue doc(mcp::JsonValue::object_tag);
  doc["name"] = mcp::JsonValue(name);
  doc["host"] = mcp::JsonValue(entry.host);
  doc["port"] = mcp::JsonValue(static_cast<int64_t>(entry.port));
  doc["path"] = mcp::JsonValue(entry.path);
  doc["protocol"] = mcp::JsonValue(entry.protocol);
  doc["auth"] = mcp::JsonValue(entry.auth);
  return doc;
}

} // namespace

EndpointInfoProviderFn &endpoint_info_slot() {
  static EndpointInfoProviderFn provider;
  return provider;
}

void register_endpoint_info_provider(EndpointInfoProviderFn provider) {
  endpoint_info_slot() = std::move(provider);
}

EndpointInfo endpoint_info() {
  EndpointInfoProviderFn &provider = endpoint_info_slot();
  if (provider) {
    return provider();
  }
  return EndpointInfo();
}

mcp::JsonValue build_endpoint_document(const EndpointInfo &info) {
  mcp::JsonValue doc(mcp::JsonValue::object_tag);
  doc["instance_id"] = mcp::JsonValue(info.instance_id);
  doc["version"] = mcp::JsonValue(info.version);
  doc["pid"] = mcp::JsonValue(static_cast<int64_t>(info.pid));
  doc["project_path"] = mcp::JsonValue(info.project_path);
  mcp::JsonValue endpoints(mcp::JsonValue::array_tag);
  endpoints.PushBack(endpoint_to_json("mcp", info.mcp));
  endpoints.PushBack(endpoint_to_json("api", info.api));
  doc["endpoints"] = std::move(endpoints);
  return doc;
}

mcp::JsonValue endpoint_document() {
  return build_endpoint_document(endpoint_info());
}

std::string endpoint_document_json() {
  return endpoint_document().Dump();
}

} // namespace api_endpoints
} // namespace godot_autopilot
