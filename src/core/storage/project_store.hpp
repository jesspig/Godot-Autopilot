#ifndef GODOT_AUTOPILOT_PROJECT_STORE_HPP
#define GODOT_AUTOPILOT_PROJECT_STORE_HPP

#include <string>

namespace godot_autopilot {

struct EndpointEntry {
  std::string host;
  int port{0};
  std::string path;
  std::string protocol;
  std::string auth;
};

struct IpcEndpointEntry {
  std::string kind{"unix"};
  std::string path;
  std::string name;
  std::string host;
  int port{0};
  std::string token;
  std::string protocol{"mcp-jsonrpc"};
  std::string framing{"length-prefixed-json"};
  std::string auth{"token"};
  int version{1};
  bool present{false};
};

struct EndpointTable {
  EndpointEntry mcp;
  EndpointEntry api;
  IpcEndpointEntry ipc;
};

class ProjectStore {
public:
  static std::string root_dir();
  static std::string project_dir(const std::string &project_name,
                                 const std::string &project_path);
  static std::string global_config_path();
  static std::string project_config_path(const std::string &project_path);
  static std::string instances_index_path();
  static std::string instance_dir(const std::string &project_path);

  static std::string project_dir_name(const std::string &project_name,
                                      const std::string &project_path);
  static std::string project_path_hash(const std::string &project_path);
  static std::string sanitize_project_name(const std::string &project_name);

private:
  static std::string project_dir_for_path(const std::string &project_path);
};

} // namespace godot_autopilot

#endif
