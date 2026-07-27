#ifndef GODOT_SELF_DRIVING_SERVER_CONTEXT_HPP
#define GODOT_SELF_DRIVING_SERVER_CONTEXT_HPP

#include <memory>
#include <chrono>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/StreamableHttpServerTransport.hpp>

#include "command_queue.hpp"
#include "tools/tool_catalog.hpp"
#include "util/bm25_index.hpp"

namespace godot_self_driving {

class ServerContext {
public:
    ServerContext(CommandQueue& queue);
    ~ServerContext();

    void start();
    void stop();

    int get_port() const;
    bool is_running() const;

    CommandQueue& get_queue() { return queue_; }

private:
    CommandQueue& queue_;
    ToolCatalog catalog_;
    Bm25Index bm25_index_;
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
