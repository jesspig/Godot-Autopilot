#ifndef GODOT_AUTOPILOT_SERVER_CONTEXT_HPP
#define GODOT_AUTOPILOT_SERVER_CONTEXT_HPP

#include <cstdint>
#include <string>

#include <mcp/server/McpServer.hpp>
#include <mcp/transport/StreamableHttpServerTransport.hpp>
#include <memory>

#include "command_queue.hpp"
#include "config.hpp"

namespace godot_autopilot {

class ToolCatalog;
class Bm25Index;

class ServerContext {
public:
  ServerContext(CommandQueue &queue);
  ~ServerContext();

  bool start();
  void stop();
  bool restart(uint16_t port);

  int get_port() const;
  const std::string &get_host() const;
  bool is_running() const;

  const std::string &last_error() const { return last_error_; }

private:
  CommandQueue &queue_;
  std::unique_ptr<ToolCatalog> catalog_;
  std::unique_ptr<Bm25Index> bm25_index_;
  std::shared_ptr<mcp::StreamableHttpServerTransport> transport_;
  std::unique_ptr<mcp::McpServer> server_;
  int port_ = GDA_DEFAULT_PORT;
  std::string host_ = "127.0.0.1";
  bool running_ = false;
  std::string last_error_;

  void register_tools();
  int resolve_port();
  static std::string resolve_host();
};

} // namespace godot_autopilot

#endif
