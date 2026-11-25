#ifndef GODOT_AUTOPILOT_SERVER_CONTEXT_HPP
#define GODOT_AUTOPILOT_SERVER_CONTEXT_HPP

#include <cstdint>
#include <string>

#include <mcp/server/McpServer.hpp>
#include <memory>

#include "command_queue.hpp"
#include "config.hpp"
#include "core/transport/godot_transport.hpp"
#include "core/transport/ipc_waiters.hpp"
#include "core/transport/port_resolver.hpp"
#include "core/storage/project_store.hpp"

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

  int get_port() const;
  const std::string &get_host() const;
  bool is_running() const;

  const std::string &last_error() const { return last_error_; }

protected:
  virtual bool bind_transport();
  virtual std::unique_ptr<mcp::McpServer>
  create_server(const std::shared_ptr<GodotTransport> &transport,
                const mcp::ServerOptions &options);

private:
  CommandQueue &queue_;
  std::unique_ptr<ToolCatalog> catalog_;
  std::unique_ptr<Bm25Index> bm25_index_;
  std::shared_ptr<GodotTransport> transport_;
  std::unique_ptr<mcp::McpServer> server_;
  IpcWaiterSet ipc_waiters_;
  int port_ = 0;
  int requested_port_ = 0;
  std::string host_ = "127.0.0.1";
  bool running_ = false;
  bool instance_started_ = false;
  std::string last_error_;
  std::string instance_id_;
  std::string ipc_token_;
  PortAssignment port_assignment_;
  EndpointTable endpoints_;

  void register_tools();
  void rollback_start();
  void write_instance_record();
  void remove_instance_record();
  static int resolve_explicit_port();
  static int resolve_preferred_port();
  static std::string resolve_host();
};

} // namespace godot_autopilot

#endif
