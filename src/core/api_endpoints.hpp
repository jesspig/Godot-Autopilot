#ifndef GODOT_AUTOPILOT_API_ENDPOINTS_HPP
#define GODOT_AUTOPILOT_API_ENDPOINTS_HPP

#include <functional>
#include <string>

#include <mcp/JsonValue.hpp>

#include "core/project_store.hpp"

namespace godot_autopilot {
namespace api_endpoints {

struct EndpointInfo {
  std::string instance_id;
  std::string version;
  int pid{0};
  std::string project_path;
  EndpointEntry mcp;
  EndpointEntry api;
};

using EndpointInfoProviderFn = std::function<EndpointInfo()>;

EndpointInfoProviderFn &endpoint_info_slot();

void register_endpoint_info_provider(EndpointInfoProviderFn provider);

EndpointInfo endpoint_info();

mcp::JsonValue build_endpoint_document(const EndpointInfo &info);

mcp::JsonValue endpoint_document();

std::string endpoint_document_json();

} // namespace api_endpoints
} // namespace godot_autopilot

#endif
