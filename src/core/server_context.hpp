#ifndef GODOT_AUTOPILOT_SERVER_CONTEXT_HPP
#define GODOT_AUTOPILOT_SERVER_CONTEXT_HPP

#include <cstdint>
#include <string>

#include <memory>

#include "command_queue.hpp"
#include "config.hpp"
#include "core/transport/ipc_waiters.hpp"
#include "core/transport/port_resolver.hpp"
#include "core/storage/project_store.hpp"

namespace gda {
namespace control {

class ControlService;
class GcpDispatcher;
class InvocationSink;

} // namespace control
} // namespace gda

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

private:
  CommandQueue &queue_;
  std::unique_ptr<ToolCatalog> catalog_;
  std::unique_ptr<Bm25Index> bm25_index_;
  std::unique_ptr<gda::control::ControlService> gcp_control_;
  std::unique_ptr<gda::control::InvocationSink> gcp_sink_;
  std::unique_ptr<gda::control::GcpDispatcher> gcp_dispatcher_;
  IpcWaiterSet ipc_waiters_;
  int port_ = 0;
  std::string host_ = "127.0.0.1";
  bool running_ = false;
  bool instance_started_ = false;
  std::string last_error_;
  std::string instance_id_;
  std::string gcp_instance_id_;
  std::string ipc_token_;
  PortAssignment port_assignment_;

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
