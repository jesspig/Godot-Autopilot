#include "server_context.hpp"
#include "log_system.hpp"
#include "resources/resource_handlers.hpp"
#include "tools/register_all.hpp"
#include <mcp/server/ServerOptions.hpp>
#include <mcp/Content.hpp>
#include <hv/hlog.h>
#include <cstdlib>

namespace godot_self_driving {

int ServerContext::resolve_port() {
    if (const char* env_port = std::getenv("GODOT_SELF_DRIVING_PORT")) {
        return std::atoi(env_port);
    }
    return 9527;
}

ServerContext::ServerContext(CommandQueue& queue)
    : queue_(queue), start_time_(std::chrono::steady_clock::now()) {
    port_ = resolve_port();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
        "Server configured on port " + std::to_string(port_));
}

ServerContext::~ServerContext() {
    if (running_) {
        stop();
    }
}

void ServerContext::start() {
    hlog_disable();

    mcp::StreamableHttpServerOptions http_opts;
    http_opts.port = static_cast<uint16_t>(port_);
    http_opts.endpoint = "/mcp";
    http_opts.stateless = true;
    http_opts.enable_legacy_sse = false;

    transport_ = std::make_shared<mcp::StreamableHttpServerTransport>(http_opts);

    mcp::ServerOptions opts;
    opts.server_info = mcp::Implementation{"godot-self-driving", "0.1.0"};
    opts.on_method_called = [this](std::string_view method) {
        LogSystem::instance().log(LogLevel::Debug, LogCategory::Transport,
            "MCP request: " + std::string(method));
    };
    opts.on_client_connected = [](const mcp::Implementation& info) {
        LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
            "Client connected: " + info.name + " " + info.version);
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

    register_tools();

    transport_->Start();

    port_ = http_opts.port;
    running_ = true;

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
        "MCP server started on 127.0.0.1:" + std::to_string(port_));
}

void ServerContext::stop() {
    if (!running_) return;
    running_ = false;

    server_->Close();
    transport_->Close();

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport, "MCP server stopped");
}

int ServerContext::get_port() const {
    return port_;
}

bool ServerContext::is_running() const {
    return running_;
}

void ServerContext::register_tools() {
    register_all_tools(*server_, queue_, catalog_, bm25_index_, port_);
    register_all_resources(*server_, queue_);
}

} // namespace godot_self_driving
