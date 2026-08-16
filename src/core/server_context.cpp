#include "server_context.hpp"
#include "config.hpp"
#include "log_system.hpp"
#include "plugin_config.hpp"
#include "prompts/debugger_prompts.hpp"
#include "prompts/prompt_handlers.hpp"
#include "resources/debugger_resources.hpp"
#include "resources/resource_handlers.hpp"
#include "tools/register_all.hpp"
#include "tools/tool_catalog.hpp"
#include "util/bm25_index.hpp"
#include <cstdlib>
#include <mcp/Content.hpp>
#include <mcp/server/ServerOptions.hpp>

namespace godot_autopilot {

int ServerContext::resolve_port() {
  if (const char *env_port = std::getenv("GODOT_AUTOPILOT_PORT")) {
    return std::atoi(env_port);
  }
  int saved_port = PluginConfig::load_port();
  if (saved_port > 0) {
    return saved_port;
  }
  return GDA_DEFAULT_PORT;
}

ServerContext::ServerContext(CommandQueue &queue)
    : queue_(queue),
      catalog_(std::make_unique<ToolCatalog>()),
      bm25_index_(std::make_unique<Bm25Index>()),
      start_time_(std::chrono::steady_clock::now()) {
  port_ = resolve_port();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "Server configured on port " +
                                std::to_string(port_));
}

ServerContext::~ServerContext() {
  if (running_) {
    stop();
  }
}

bool ServerContext::start() {
  try {
    mcp::StreamableHttpServerOptions http_opts;
    http_opts.port = static_cast<uint16_t>(port_);
    http_opts.endpoint = "/mcp";
    http_opts.stateless = true;
    http_opts.enable_legacy_sse = false;

    transport_ =
        std::make_shared<mcp::StreamableHttpServerTransport>(http_opts);

    mcp::ServerOptions opts;
    opts.server_info = mcp::Implementation{"godot-autopilot", "0.1.0"};
    opts.on_method_called = [this](std::string_view method) {
      LogSystem::instance().log(LogLevel::Debug, LogCategory::Transport,
                                "MCP request: " + std::string(method));
    };
    opts.on_client_connected = [](const mcp::Implementation &info) {
      LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                                "Client connected: " + info.name + " " +
                                    info.version);
    };
    opts.on_initialized = [] {
      LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                                "Client initialization complete");
    };
    opts.on_protocol_error = [](std::string_view error) {
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "Protocol error: " + std::string(error));
    };
    opts.on_transport_close = [] {
      LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                                "Client disconnected");
    };
    opts.on_transport_error = [](std::string_view msg) {
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "Transport error: " + std::string(msg));
    };
    server_ = mcp::McpServer::Create(transport_, opts);
    if (!server_) {
      last_error_ = "McpServer::Create returned null";
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "MCP server start failed: " + last_error_);
      return false;
    }

    register_tools();

    transport_->Start();

    port_ = http_opts.port;
    running_ = true;

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "MCP server started on 0.0.0.0:" +
                                  std::to_string(port_));
    return true;
  } catch (const std::exception &e) {
    last_error_ = "transport start failed: " + std::string(e.what());
    LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                              "MCP server start failed: " + last_error_);
    return false;
  } catch (...) {
    last_error_ = "transport start failed: unknown exception";
    LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                              "MCP server start failed: " + last_error_);
    return false;
  }
}

void ServerContext::stop() {
  if (!running_)
    return;
  running_ = false;

  server_->Close();
  transport_->Close();

  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "MCP server stopped");
}

bool ServerContext::restart(uint16_t port) {
  stop();
  port_ = port;
  return start();
}

int ServerContext::get_port() const { return port_; }

bool ServerContext::is_running() const { return running_; }

void ServerContext::register_tools() {
  register_all_tools(*server_, queue_, *catalog_, *bm25_index_, port_);
  register_all_resources(*server_, queue_);
  register_all_prompts(*server_, queue_);
  register_debugger_resources(*server_);
  register_debugger_prompts(*server_);
}

} // namespace godot_autopilot
