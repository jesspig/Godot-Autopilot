#ifndef GODOT_SELF_DRIVING_SERVER_CONTEXT_HPP
#define GODOT_SELF_DRIVING_SERVER_CONTEXT_HPP

#include <memory>
#include <chrono>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/StreamableHttpServerTransport.hpp>

namespace godot_self_driving {

class ServerContext {
public:
    ServerContext();
    ~ServerContext();

    void start();
    void stop();

    int get_port() const;
    bool is_running() const;

private:
    std::shared_ptr<mcp::StreamableHttpServerTransport> transport_;
    std::unique_ptr<mcp::McpServer> server_;
    int port_ = 9527;
    bool running_ = false;
    std::chrono::steady_clock::time_point start_time_;

    void register_tools();
    int resolve_port();
};

} // namespace godot_self_driving

#endif
