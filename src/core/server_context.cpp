#include "server_context.hpp"
#include "config.hpp"
#include "log_system.hpp"
#include "plugin_config.hpp"
#include <version.hpp>
#include "prompts/debugger_prompts.hpp"
#include "prompts/prompt_handlers.hpp"
#include "resources/debugger_resources.hpp"
#include "resources/resource_handlers.hpp"
#include "tools/register_all.hpp"
#include "tools/dispatch.hpp"
#include "tools/tool_catalog.hpp"
#include "util/bm25_index.hpp"
#include <cstdlib>
#include <exception>
#include <string_view>
#include <typeinfo>
#include <mcp/Content.hpp>
#include <mcp/server/ServerOptions.hpp>

namespace godot_autopilot {

namespace {
bool is_loopback_host(std::string_view host) {
  return host == "127.0.0.1" || host == "::1" || host == "[::1]";
}
} // namespace

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

std::string ServerContext::resolve_host() {
  const char *env = std::getenv("GODOT_AUTOPILOT_HOST");
  if (env && *env)
    return std::string(env);
  return "127.0.0.1";
}

ServerContext::ServerContext(CommandQueue &queue)
    : queue_(queue),
      catalog_(std::make_unique<ToolCatalog>()),
      bm25_index_(std::make_unique<Bm25Index>()) {
  port_ = resolve_port();
  host_ = resolve_host();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "Server configured on " + host_ + ":" +
                                std::to_string(port_));
}

ServerContext::~ServerContext() {
  if (running_ || server_ || transport_) {
    stop();
  }
}

bool ServerContext::start() {
  try {
    if (!is_loopback_host(host_)) {
      last_error_ = "refusing non-loopback listen address '" + host_ +
                    "': remote listening requires application authentication";
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "MCP server start rejected: " + last_error_);
      return false;
    }

    mcp::StreamableHttpServerOptions http_opts;
    http_opts.port = static_cast<uint16_t>(port_);
    http_opts.endpoint = "/mcp";
    http_opts.stateless = true;
    http_opts.enable_legacy_sse = false;
    http_opts.host = host_;

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "MCP server initializing: host=" +
                                  http_opts.host + " port=" +
                                  std::to_string(port_));

    transport_ =
        std::make_shared<mcp::StreamableHttpServerTransport>(http_opts);

    mcp::ServerOptions opts;
    opts.server_info = mcp::Implementation{"godot-autopilot", GDA_VERSION};
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
      transport_.reset();
      dispatch::clear_handlers();
      clear_active_registry();
      return false;
    }

    LogSystem::instance().log(LogLevel::Debug, LogCategory::Transport,
                              "Registering tools/catalog/resources/prompts...");

    register_tools();

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "Tool registration complete");

    LogSystem::instance().log(LogLevel::Debug, LogCategory::Transport,
                              "Starting HTTP transport on " +
                                  http_opts.host + ":" +
                                  std::to_string(port_));

    transport_->Start();

    port_ = http_opts.port;
    running_ = true;

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "MCP server started on " + http_opts.host +
                                  ":" + std::to_string(port_));
    return true;
  } catch (const std::exception &e) {
    last_error_ = "transport start failed: " + std::string(typeid(e).name()) +
                  ": " + std::string(e.what());
    LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                              "MCP server start failed: " + last_error_);
    if (server_) { try { server_->Close(); } catch (...) {} server_.reset(); }
    if (transport_) { try { transport_->Close(); } catch (...) {} transport_.reset(); }
    dispatch::clear_handlers();
    clear_active_registry();
    return false;
  } catch (...) {
    std::string detail = "non-std exception";
    try {
      std::rethrow_exception(std::current_exception());
    } catch (const std::exception &e) {
      detail = std::string(typeid(e).name()) + ": " + std::string(e.what());
    } catch (...) {
    }
    last_error_ = "transport start failed: " + detail;
    LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                              "MCP server start failed: " + last_error_);
    if (server_) { try { server_->Close(); } catch (...) {} server_.reset(); }
    if (transport_) { try { transport_->Close(); } catch (...) {} transport_.reset(); }
    dispatch::clear_handlers();
    clear_active_registry();
    return false;
  }
}

void ServerContext::stop() {
  if (!running_ && !server_ && !transport_)
    return;
  running_ = false;

  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "MCP server stopping");

  if (server_)
    server_->Close();
  if (transport_)
    transport_->Close();
  server_.reset();
  transport_.reset();
  dispatch::clear_handlers();
  clear_active_registry();

  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "MCP server stopped");
}

bool ServerContext::restart(uint16_t port) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "MCP server restart requested on port " +
                                std::to_string(port));
  stop();
  port_ = port;
  return start();
}

int ServerContext::get_port() const { return port_; }

const std::string &ServerContext::get_host() const { return host_; }

bool ServerContext::is_running() const { return running_; }

void ServerContext::register_tools() {
  register_all_tools(*server_, queue_, *catalog_, *bm25_index_, port_);
  register_all_resources(*server_, queue_);
  register_all_prompts(*server_, queue_);
  register_debugger_resources(*server_, queue_);
  register_debugger_prompts(*server_);
}

} // namespace godot_autopilot
