#ifndef GODOT_AUTOPILOT_GODOT_TRANSPORT_HPP
#define GODOT_AUTOPILOT_GODOT_TRANSPORT_HPP

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <mcp/JsonRpc.hpp>
#include <mcp/Transport.hpp>

namespace godot_autopilot {

namespace mcp_wire {

struct ParsedRequest {
  bool ok{false};
  bool is_request{false};
  bool is_notification{false};
  std::string request_id;
  std::string method;
  std::string error;
  mcp::JsonRpcMessage message;
};

ParsedRequest parse_request(const std::string &body);
std::string build_response_text(const mcp::JsonRpcMessage &message);
std::string build_error_response_text(int code, const std::string &message);
std::string build_event_frame(const std::string &event, const std::string &data);
std::string build_close_event_frame();
bool is_supported_protocol_version(const std::string &version);
std::string request_id_key(const mcp::RequestId &id);
std::string make_pending_key(const std::string &connection_id,
                             const std::string &request_id);

}

class GodotTransport : public mcp::TransportBase {
public:
  GodotTransport();
  ~GodotTransport() override;

  void Close() override;
  bool IsStateless() const override;
  void SendMessageAsync(mcp::JsonRpcMessage message) override;

  bool submit(const mcp_wire::ParsedRequest &parsed,
              const std::string &connection_id, std::string &error);
  bool await_response(const std::string &connection_id,
                      const std::string &request_id,
                      std::string &response_text,
                      std::chrono::milliseconds timeout);
  bool await_response(const std::string &connection_id,
                      const std::string &request_id,
                      mcp::JsonRpcMessage &message,
                      std::chrono::milliseconds timeout);

private:
  struct PendingExchange {
    std::shared_ptr<std::promise<mcp::JsonRpcMessage>> promise;
    std::shared_future<mcp::JsonRpcMessage> future;
    mcp::RequestId request_id;
    std::string connection_id;
    bool expired{false};
    std::chrono::steady_clock::time_point expired_at{};
  };

  std::mutex pending_mutex_;
  std::unordered_map<std::string, PendingExchange> pending_;
  std::unordered_map<std::string, std::string> id_index_;

  void expire_stale_pending();
  void fail_pending_exchanges();
};

std::shared_ptr<GodotTransport> godot_transport_instance();

std::shared_ptr<GodotTransport> godot_transport_reset();

} // namespace godot_autopilot

#endif
