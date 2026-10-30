#ifndef GODOT_AUTOPILOT_INSTANCE_REGISTRY_HPP
#define GODOT_AUTOPILOT_INSTANCE_REGISTRY_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <mcp/JsonValue.hpp>

#include "core/project_store.hpp"

namespace godot_autopilot {

struct InstanceRecord {
  std::string instance_id;
  int port{0};
  int pid{0};
  std::string project_path;
  std::string api_token;
  EndpointTable endpoints;
  std::string gda_home;
  int64_t started_at{0};
  std::string version;
};

mcp::JsonValue record_to_json(const InstanceRecord &record);
bool record_from_json(const mcp::JsonValue &doc, InstanceRecord &out);
bool record_from_json_text(const std::string &json_text, InstanceRecord &out);

class InstanceRegistry {
public:
  static bool write(const InstanceRecord &record);
  static bool remove(const std::string &instance_id);
  static std::vector<InstanceRecord> list_all();
  static bool update_global_index();
};

} // namespace godot_autopilot

#endif
