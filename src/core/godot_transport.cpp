#include "core/godot_transport.hpp"

#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <mcp/JsonValue.hpp>
#include <mcp/ProtocolVersion.hpp>

#include "core/http_stream_registry.hpp"

namespace godot_autopilot {

namespace mcp_wire {

std::string request_id_key(const mcp::RequestId &id) {
  if (std::holds_alternative<int64_t>(id)) {
    return "i:" + std::to_string(std::get<int64_t>(id));
  }
  return "s:" + std::get<std::string>(id);
}

ParsedRequest parse_request(const std::string &body) {
  ParsedRequest parsed;
  try {
    parsed.message = mcp::DeserializeMessage(body);
  } catch (const std::exception &e) {
    parsed.error = std::string("message parse failed: ") + e.what();
    return parsed;
  } catch (...) {
    parsed.error = "message parse failed: unknown exception";
    return parsed;
  }
  if (const auto *request = mcp::AsRequest(parsed.message)) {
    parsed.ok = true;
    parsed.is_request = true;
    parsed.method = request->method;
    parsed.request_id = request_id_key(request->id);
    return parsed;
  }
  if (const auto *notification = mcp::AsNotification(parsed.message)) {
    parsed.ok = true;
    parsed.is_notification = true;
    parsed.method = notification->method;
    return parsed;
  }
  parsed.error = "message is neither a client request nor a notification";
  return parsed;
}

std::string build_response_text(const mcp::JsonRpcMessage &message) {
  return mcp::SerializeMessage(message);
}

std::string build_error_response_text(int code, const std::string &message) {
  mcp::JsonValue root(mcp::JsonValue::object_tag);
  root["jsonrpc"] = mcp::JsonValue("2.0");
  mcp::JsonValue failure(mcp::JsonValue::object_tag);
  failure["code"] = mcp::JsonValue(static_cast<int64_t>(code));
  failure["message"] = mcp::JsonValue(message);
  root["error"] = std::move(failure);
  return root.Dump();
}

std::string build_event_frame(const std::string &event,
                               const std::string &data) {
  std::string escaped;
  escaped.reserve(data.size());
  for (char c : data) {
    if (c == '\n') {
      escaped += "\ndata: ";
    } else {
      escaped.push_back(c);
    }
  }
  return "event: " + event + "\ndata: " + escaped + "\n\n";
}

std::string build_close_event_frame() {
  return "event: close\ndata: {}\n\n";
}

bool is_supported_protocol_version(const std::string &version) {
  if (version.empty()) {
    return true;
  }
  for (const auto &known : mcp::kProtocolVersions) {
    if (version == known) {
      return true;
    }
  }
  return false;
}

}

namespace {

std::mutex &transport_instance_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::shared_ptr<GodotTransport> &transport_instance_slot() {
  static std::shared_ptr<GodotTransport> instance;
  return instance;
}

} // namespace

GodotTransport::GodotTransport() = default;

GodotTransport::~GodotTransport() = default;

void GodotTransport::Close() {
  fail_pending_exchanges();
  if (GetState() == mcp::TransportState::Disconnected) {
    return;
  }
  GetMessageChannel().Close();
  SetDisconnected();
}

void GodotTransport::fail_pending_exchanges() {
  std::vector<PendingExchange> pending;
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending.reserve(pending_.size());
    for (const auto &entry : pending_) {
      pending.push_back(entry.second);
    }
    pending_.clear();
  }
  for (const PendingExchange &exchange : pending) {
    if (!exchange.promise) {
      continue;
    }
    mcp::JsonRpcErrorResponse failure;
    failure.id = exchange.request_id;
    failure.error.code = mcp::McpErrorCode::ConnectionClosed;
    failure.error.message =
        "the MCP transport closed before the request completed";
    try {
      exchange.promise->set_value(mcp::JsonRpcMessage(std::move(failure)));
    } catch (const std::exception &) {
    }
  }
}

bool GodotTransport::IsStateless() const { return true; }

void GodotTransport::SendMessageAsync(mcp::JsonRpcMessage message) {
  std::string key;
  if (const auto *response = std::get_if<mcp::JsonRpcResponse>(&message)) {
    key = mcp_wire::request_id_key(response->id);
  } else if (const auto *failure =
                 std::get_if<mcp::JsonRpcErrorResponse>(&message)) {
    if (!failure->id) {
      return;
    }
    key = mcp_wire::request_id_key(*failure->id);
  } else {
    http_stream::broadcast(
        mcp_wire::build_event_frame("message", mcp::SerializeMessage(message)));
    return;
  }
  std::shared_ptr<std::promise<mcp::JsonRpcMessage>> waiter;
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto it = pending_.find(key);
    if (it == pending_.end()) {
      return;
    }
    waiter = it->second.promise;
  }
  try {
    waiter->set_value(std::move(message));
  } catch (const std::exception &) {
  }
}

bool GodotTransport::submit(const mcp_wire::ParsedRequest &parsed,
                            std::string &error) {
  if (!parsed.ok) {
    error = parsed.error;
    return false;
  }
  if (parsed.is_request) {
    auto promise = std::make_shared<std::promise<mcp::JsonRpcMessage>>();
    std::shared_future<mcp::JsonRpcMessage> future = promise->get_future().share();
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      if (pending_.count(parsed.request_id) != 0) {
        error = "request id already in flight: " + parsed.request_id;
        return false;
      }
      PendingExchange exchange;
      exchange.promise = std::move(promise);
      exchange.future = std::move(future);
      if (const auto *request = mcp::AsRequest(parsed.message)) {
        exchange.request_id = request->id;
      }
      pending_.emplace(parsed.request_id, std::move(exchange));
    }
  }
  if (!GetMessageChannel().TrySend(parsed.message)) {
    if (parsed.is_request) {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      pending_.erase(parsed.request_id);
    }
    error = "message channel is closed";
    return false;
  }
  return true;
}

bool GodotTransport::await_response(const std::string &request_id,
                                    mcp::JsonRpcMessage &message,
                                    std::chrono::milliseconds timeout) {
  std::shared_future<mcp::JsonRpcMessage> future;
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto it = pending_.find(request_id);
    if (it == pending_.end()) {
      return false;
    }
    future = it->second.future;
  }
  if (future.wait_for(timeout) != std::future_status::ready) {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_.erase(request_id);
    return false;
  }
  message = future.get();
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_.erase(request_id);
  }
  return true;
}

bool GodotTransport::await_response(const std::string &request_id,
                                    std::string &response_text,
                                    std::chrono::milliseconds timeout) {
  mcp::JsonRpcMessage message;
  if (!await_response(request_id, message, timeout)) {
    return false;
  }
  response_text = mcp_wire::build_response_text(message);
  return true;
}

std::shared_ptr<GodotTransport> godot_transport_instance() {
  std::lock_guard<std::mutex> lock(transport_instance_mutex());
  auto &slot = transport_instance_slot();
  if (!slot) {
    slot = std::make_shared<GodotTransport>();
  }
  return slot;
}

std::shared_ptr<GodotTransport> godot_transport_reset() {
  std::shared_ptr<GodotTransport> previous;
  std::shared_ptr<GodotTransport> fresh;
  {
    std::lock_guard<std::mutex> lock(transport_instance_mutex());
    auto &slot = transport_instance_slot();
    previous = slot;
    fresh = std::make_shared<GodotTransport>();
    slot = fresh;
  }
  if (previous) {
    previous->Close();
  }
  return fresh;
}

} // namespace godot_autopilot
