#ifndef GODOT_SELF_DRIVING_SERVER_CONTEXT_HPP
#define GODOT_SELF_DRIVING_SERVER_CONTEXT_HPP

#include <chrono>
#include <string>

#include <mcp/server/McpServer.hpp>
#include <mcp/transport/StreamableHttpServerTransport.hpp>
#include <memory>

#include "command_queue.hpp"
#include "config.hpp"

namespace godot_self_driving {

class ToolCatalog;
class Bm25Index;

class ServerContext {
public:
  ServerContext(CommandQueue &queue);
  ~ServerContext();

  bool start();
  void stop();

  int get_port() const;
  bool is_running() const;

  CommandQueue &get_queue() { return queue_; }

  const std::string &last_error() const { return last_error_; }

private:
  CommandQueue &queue_;
  std::unique_ptr<ToolCatalog> catalog_;
  std::unique_ptr<Bm25Index> bm25_index_;
  std::shared_ptr<mcp::StreamableHttpServerTransport> transport_;
  std::unique_ptr<mcp::McpServer> server_;
  int port_ = GSD_DEFAULT_PORT;
  bool running_ = false;
  std::string last_error_;
  std::chrono::steady_clock::time_point start_time_;

  void register_tools();
  int resolve_port();
};

} // namespace godot_self_driving

#endif
