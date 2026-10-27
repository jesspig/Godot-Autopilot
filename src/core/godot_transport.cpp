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
#include "core/log_system.hpp"

namespace godot_autopilot {

namespace mcp_wire {

std::string request_id_key(const mcp::RequestId &id) {
  if (std::holds_alternative<int64_t>(id)) {
    return "i:" + std::to_string(std::get<int64_t>(id));
  }
  return "s:" + std::get<std::string>(id);
}

std::string make_pending_key(const std::string &connection_id,
                             const std::string &request_id) {
  return connection_id + '\x1f' + request_id;
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

constexpr std::chrono::milliseconds kPendingExpiredGraceMs{60000};

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

void GodotTransport::expire_stale_pending() {
  std::lock_guard<std::mutex> lock(pending_mutex_);
  const auto now = std::chrono::steady_clock::now();
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (!it->second.expired ||
        now - it->second.expired_at <= kPendingExpiredGraceMs) {
      ++it;
      continue;
    }
    id_index_.erase(mcp_wire::request_id_key(it->second.request_id));
    it = pending_.erase(it);
  }
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
    id_index_.clear();
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
  bool drop_expired = false;
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto index = id_index_.find(key);
    if (index == id_index_.end()) {
      return;
    }
    auto it = pending_.find(index->second);
    if (it == pending_.end()) {
      return;
    }
    if (it->second.expired) {
      id_index_.erase(index);
      pending_.erase(it);
      drop_expired = true;
    } else {
      waiter = it->second.promise;
    }
  }
  if (drop_expired) {
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Transport,
        "dropped late mcp response for expired request " + key);
    return;
  }
  try {
    waiter->set_value(std::move(message));
  } catch (const std::exception &) {
  }
}

bool GodotTransport::submit(const mcp_wire::ParsedRequest &parsed,
                            const std::string &connection_id,
                            std::string &error) {
  if (!parsed.ok) {
    error = parsed.error;
    return false;
  }
  expire_stale_pending();
  if (parsed.is_request) {
    auto promise = std::make_shared<std::promise<mcp::JsonRpcMessage>>();
    std::shared_future<mcp::JsonRpcMessage> future = promise->get_future().share();
    const std::string key =
        mcp_wire::make_pending_key(connection_id, parsed.request_id);
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      auto index = id_index_.find(parsed.request_id);
      if (index != id_index_.end()) {
        std::string owner = connection_id;
        const auto entry = pending_.find(index->second);
        if (entry != pending_.end()) {
          owner = entry->second.connection_id;
        }
        error = "request id already in flight (connection " + owner + ")";
        return false;
      }
      PendingExchange exchange;
      exchange.promise = std::move(promise);
      exchange.future = std::move(future);
      if (const auto *request = mcp::AsRequest(parsed.message)) {
        exchange.request_id = request->id;
      }
      exchange.connection_id = connection_id;
      pending_.emplace(key, std::move(exchange));
      id_index_.emplace(parsed.request_id, key);
    }
  }
  if (!GetMessageChannel().TrySend(parsed.message)) {
    if (parsed.is_request) {
      const std::string key =
          mcp_wire::make_pending_key(connection_id, parsed.request_id);
      std::lock_guard<std::mutex> lock(pending_mutex_);
      pending_.erase(key);
      id_index_.erase(parsed.request_id);
    }
    error = "message channel is closed";
    return false;
  }
  return true;
}

bool GodotTransport::await_response(const std::string &connection_id,
                                    const std::string &request_id,
                                    mcp::JsonRpcMessage &message,
                                    std::chrono::milliseconds timeout) {
  expire_stale_pending();
  const std::string key =
      mcp_wire::make_pending_key(connection_id, request_id);
  std::shared_future<mcp::JsonRpcMessage> future;
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto it = pending_.find(key);
    if (it == pending_.end()) {
      return false;
    }
    future = it->second.future;
  }
  if (future.wait_for(timeout) != std::future_status::ready) {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto it = pending_.find(key);
    if (it != pending_.end()) {
      it->second.expired = true;
      it->second.expired_at = std::chrono::steady_clock::now();
    }
    return false;
  }
  message = future.get();
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    auto it = pending_.find(key);
    if (it != pending_.end()) {
      id_index_.erase(mcp_wire::request_id_key(it->second.request_id));
      pending_.erase(it);
    }
  }
  return true;
}

bool GodotTransport::await_response(const std::string &connection_id,
                                    const std::string &request_id,
                                    std::string &response_text,
                                    std::chrono::milliseconds timeout) {
  mcp::JsonRpcMessage message;
  if (!await_response(connection_id, request_id, message, timeout)) {
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
