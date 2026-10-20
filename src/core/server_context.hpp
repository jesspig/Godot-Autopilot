#ifndef GODOT_AUTOPILOT_SERVER_CONTEXT_HPP
#define GODOT_AUTOPILOT_SERVER_CONTEXT_HPP

#include <cstdint>
#include <string>

#include <mcp/server/McpServer.hpp>
#include <memory>

#include "command_queue.hpp"
#include "config.hpp"
#include "core/godot_transport.hpp"
#include "port_resolver.hpp"
#include "project_store.hpp"

namespace godot_autopilot {

class ToolCatalog;
class Bm25Index;

class ServerContext {
public:
  ServerContext(CommandQueue &queue);
  ~ServerContext();

  static ServerContext *instance();

  bool start();
  void stop();
  bool restart(uint16_t port);

  int get_port() const;
  const std::string &get_host() const;
  bool is_running() const;

  const std::string &last_error() const { return last_error_; }

  const PortAssignment &port_assignment() const;
  void set_port_assignment(const PortAssignment &assignment);
  const EndpointTable &endpoints() const;
  void set_endpoints(const EndpointTable &endpoints);

private:
  CommandQueue &queue_;
  std::unique_ptr<ToolCatalog> catalog_;
  std::unique_ptr<Bm25Index> bm25_index_;
  std::shared_ptr<GodotTransport> transport_;
  std::unique_ptr<mcp::McpServer> server_;
  int port_ = GDA_DEFAULT_PORT;
  int requested_port_ = 0;
  std::string host_ = "127.0.0.1";
  bool running_ = false;
  std::string last_error_;
  std::string instance_id_;
  PortAssignment port_assignment_;
  EndpointTable endpoints_;

  void register_tools();
  bool bind_transport();
  void write_instance_record();
  void remove_instance_record();
  static int resolve_explicit_port();
  static int resolve_preferred_port();
  static std::string resolve_host();
};

} // namespace godot_autopilot

#endif
