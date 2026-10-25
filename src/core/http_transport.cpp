#include "core/http_transport.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "core/config.hpp"
#include "core/http_parse.hpp"
#include "core/http_stream_registry.hpp"
#include "core/http_transport_ext.hpp"
#include "core/instance_registry.hpp"
#include "core/log_system.hpp"
#include "core/port_recovery.hpp"
#include "core/transport_lifecycle.hpp"
#include "util/api_host_context.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kTransportContentType = "application/json";
constexpr const char *kTransportEventStreamType = "text/event-stream";

struct PendingJob {
  HttpRequest request;
  HttpRouteHandler handler;
  HttpResponse preset;
  bool use_preset{false};
  bool keep_alive{true};
};

struct CompletedReply {
  HttpResponse response;
  bool keep_alive{true};
};

struct ConnectionChannel {
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<PendingJob> jobs;
  std::deque<CompletedReply> replies;
  bool closed{false};
  bool finished{false};
  bool executing{false};
};

struct TransportConnection {
  godot::Ref<godot::StreamPeerTCP> peer;
  std::string inbox;
  std::string outbox;
  bool close_after_write{false};
  bool dead{false};
  bool streaming{false};
  uint64_t id{0};
  std::shared_ptr<ConnectionChannel> channel;
  std::shared_ptr<http_stream::Sink> sink;
  std::thread worker;
};

struct TransportState {
  godot::Ref<godot::TCPServer> server;
  uint16_t bound_port{0};
  std::vector<std::shared_ptr<TransportConnection>> connections;
  HttpRouteTable routes;
  uint64_t next_connection_id{1};
};

TransportState &transport_state_never_destroyed() {
  static TransportState *state = new TransportState();
  return *state;
}

void transport_release_engine_server(TransportState &state) {
  if (!state.server.is_valid()) {
    return;
  }
  state.server->stop();
  state.server.unref();
}

bool transport_host_is_loopback(const std::string &host) {
  return host == "127.0.0.1" || host == "::1" || host == "[::1]";
}

std::string transport_json_escape(const std::string &text) {
  std::string result;
  result.reserve(text.size());
  for (char c : text) {
    if (c == '"' || c == '\\') {
      result.push_back('\\');
      result.push_back(c);
    } else if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c == '\t') {
      result += "\\t";
    } else if (static_cast<unsigned char>(c) < 0x20) {
      result += " ";
    } else {
      result.push_back(c);
    }
  }
  return result;
}

std::string transport_find_header(
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &name) {
  const std::string *found = http_parse::find_header(headers, name);
  if (found == nullptr) {
    return std::string();
  }
  return *found;
}

std::string transport_hex(uint64_t value) {
  static const char *kDigits = "0123456789abcdef";
  std::string out;
  bool started = false;
  for (int shift = 60; shift >= 0; shift -= 4) {
    const int nibble = static_cast<int>((value >> shift) & 0xF);
    if (nibble != 0 || started || shift == 0) {
      out.push_back(kDigits[nibble]);
      started = true;
    }
  }
  return out;
}

std::string transport_chunk(const std::string &data) {
  if (data.empty()) {
    return std::string();
  }
  return transport_hex(static_cast<uint64_t>(data.size())) + "\r\n" + data +
         "\r\n";
}

std::string transport_build_response(const HttpResponse &response,
                                     bool keep_alive) {
  std::string head = "HTTP/1.1 " + std::to_string(response.status_code) + " " +
                     response.status_text + "\r\n";
  std::string content_type =
      transport_find_header(response.headers, "Content-Type");
  if (content_type.empty()) {
    content_type = kTransportContentType;
  }
  head += "Content-Type: " + content_type + "\r\n";
  head += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
  for (const auto &entry : response.headers) {
    const std::string lowered = http_parse::to_lower(entry.first);
    if (lowered == "content-type" || lowered == "content-length" ||
        lowered == "connection" || lowered == kStreamIdHeader) {
      continue;
    }
    head += entry.first + ": " + entry.second + "\r\n";
  }
  head += std::string("Connection: ") + (keep_alive ? "keep-alive" : "close") +
          "\r\n\r\n";
  return head + response.body;
}

std::string transport_build_stream_response(const HttpResponse &response) {
  std::string head = "HTTP/1.1 " + std::to_string(response.status_code) + " " +
                     response.status_text + "\r\n";
  std::string content_type =
      transport_find_header(response.headers, "Content-Type");
  if (content_type.empty()) {
    content_type = kTransportEventStreamType;
  }
  head += "Content-Type: " + content_type + "\r\n";
  head += "Cache-Control: no-cache\r\n";
  head += "Transfer-Encoding: chunked\r\n";
  head += "Connection: keep-alive\r\n\r\n";
  return head + transport_chunk(response.body);
}

HttpResponse transport_error_response(int status_code,
                                      const std::string &status_text,
                                      const std::string &message) {
  HttpResponse response;
  response.status_code = status_code;
  response.status_text = status_text;
  response.headers["Content-Type"] = kTransportContentType;
  response.body = "{\"error\":\"" + transport_json_escape(message) + "\"}";
  return response;
}

void transport_execute_job(const PendingJob &job, HttpResponse &response) {
  if (job.use_preset) {
    response = job.preset;
    return;
  }
  if (!job.handler) {
    response = transport_error_response(
        404, "Not Found", "no route for " + job.request.path);
    return;
  }
  try {
    (job.handler)(job.request, response);
  } catch (const std::exception &e) {
    response = transport_error_response(500, "Internal Server Error",
                                        std::string("handler failed: ") +
                                            e.what());
  } catch (...) {
    response = transport_error_response(500, "Internal Server Error",
                                        "handler failed: unknown exception");
  }
}

bool transport_channel_finished(const std::shared_ptr<ConnectionChannel> &channel) {
  std::lock_guard<std::mutex> lock(channel->mutex);
  return channel->finished;
}

bool transport_channel_executing(
    const std::shared_ptr<ConnectionChannel> &channel) {
  std::lock_guard<std::mutex> lock(channel->mutex);
  return channel->executing;
}

void transport_worker_loop(const std::shared_ptr<ConnectionChannel> &channel) {
  while (true) {
    PendingJob job;
    {
      std::unique_lock<std::mutex> lock(channel->mutex);
      channel->cv.wait(lock, [&channel] {
        return channel->closed || !channel->jobs.empty();
      });
      if (channel->closed) {
        return;
      }
      job = std::move(channel->jobs.front());
      channel->jobs.pop_front();
    }
    {
      std::lock_guard<std::mutex> lock(channel->mutex);
      if (channel->closed) {
        return;
      }
      channel->executing = true;
    }
    HttpResponse response;
    transport_execute_job(job, response);
    {
      std::lock_guard<std::mutex> lock(channel->mutex);
      channel->executing = false;
      if (channel->closed) {
        return;
      }
      CompletedReply reply;
      reply.response = std::move(response);
      reply.keep_alive = job.keep_alive;
      channel->replies.push_back(std::move(reply));
    }
  }
}

void transport_worker_main(const std::shared_ptr<ConnectionChannel> &channel) {
  transport_worker_loop(channel);
  {
    std::lock_guard<std::mutex> lock(channel->mutex);
    channel->finished = true;
  }
  channel->cv.notify_all();
}

void transport_enqueue_job(TransportConnection &connection, PendingJob job) {
  {
    std::lock_guard<std::mutex> lock(connection.channel->mutex);
    if (connection.channel->closed) {
      return;
    }
    connection.channel->jobs.push_back(std::move(job));
  }
  connection.channel->cv.notify_one();
}

void transport_wait_worker(const std::shared_ptr<ConnectionChannel> &channel,
                           std::chrono::milliseconds budget) {
  std::unique_lock<std::mutex> lock(channel->mutex);
  channel->cv.wait_for(lock, budget,
                       [&channel] { return channel->finished; });
}

struct ParkedWorker {
  std::thread thread;
  std::shared_ptr<ConnectionChannel> channel;
};

std::vector<ParkedWorker> &parked_workers_never_destroyed() {
  static auto *parked = new std::vector<ParkedWorker>();
  return *parked;
}

void transport_park_worker(std::thread worker,
                            const std::shared_ptr<ConnectionChannel> &channel) {
  ParkedWorker entry;
  entry.thread = std::move(worker);
  entry.channel = channel;
  parked_workers_never_destroyed().push_back(std::move(entry));
}

void transport_reap_parked_workers() {
  std::vector<ParkedWorker> &parked = parked_workers_never_destroyed();
  if (parked.empty()) {
    return;
  }
  std::vector<ParkedWorker> pending;
  pending.reserve(parked.size());
  for (ParkedWorker &entry : parked) {
    const bool finished = transport_channel_finished(entry.channel);
    if (transport_lifecycle::should_reap(finished)) {
      if (entry.thread.joinable()) {
        entry.thread.join();
      }
      continue;
    }
    pending.push_back(std::move(entry));
  }
  parked.swap(pending);
}

void transport_join_worker(TransportConnection &connection) {
  if (!connection.worker.joinable()) {
    return;
  }
  if (transport_channel_finished(connection.channel)) {
    connection.worker.join();
    return;
  }
  transport_park_worker(std::move(connection.worker), connection.channel);
}

std::chrono::steady_clock::time_point transport_worker_deadline() {
  return std::chrono::steady_clock::now() +
         std::chrono::milliseconds(GDA_RESPONSE_GRACE_MS);
}

void transport_wait_parked_workers(
    std::chrono::steady_clock::time_point deadline) {
  std::vector<ParkedWorker> &parked = parked_workers_never_destroyed();
  for (ParkedWorker &entry : parked) {
    const std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    if (now >= deadline) {
      break;
    }
    transport_wait_worker(
        entry.channel,
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
  }
  transport_reap_parked_workers();
}

size_t transport_drain_parked_workers(
    std::chrono::steady_clock::time_point deadline) {
  std::vector<ParkedWorker> &parked = parked_workers_never_destroyed();
  if (parked.empty()) {
    return 0;
  }
  transport_wait_parked_workers(deadline);
  size_t joined = 0;
  size_t forced = 0;
  size_t executing = 0;
  for (ParkedWorker &entry : parked) {
    if (!entry.thread.joinable()) {
      continue;
    }
    if (!transport_channel_finished(entry.channel)) {
      ++forced;
      if (transport_channel_executing(entry.channel)) {
        ++executing;
      }
    }
    entry.thread.join();
    ++joined;
  }
  parked.clear();
  if (forced > 0) {
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Transport,
        "joined " + std::to_string(forced) +
            " transport worker thread(s) that exceeded the shutdown budget (" +
            std::to_string(executing) + " still inside a route handler)");
  }
  return joined;
}

void transport_retire_connections(
    std::vector<std::shared_ptr<TransportConnection>> &connections,
    bool wait_for_workers,
    std::chrono::steady_clock::time_point deadline) {
  for (const std::shared_ptr<TransportConnection> &connection : connections) {
    if (!connection) {
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(connection->channel->mutex);
      connection->channel->closed = true;
    }
    connection->channel->cv.notify_all();
  }
  for (const std::shared_ptr<TransportConnection> &connection : connections) {
    if (!connection) {
      continue;
    }
    if (wait_for_workers) {
      const std::chrono::steady_clock::time_point now =
          std::chrono::steady_clock::now();
      if (now < deadline) {
        transport_wait_worker(
            connection->channel,
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                  now));
      }
    }
    transport_join_worker(*connection);
    http_stream::release(std::to_string(connection->id));
    if (connection->peer.is_valid()) {
      connection->peer->disconnect_from_host();
    }
  }
  connections.clear();
  transport_reap_parked_workers();
}

void transport_read(TransportConnection &connection) {
  const HttpPumpLimits &limits = http_pump_limits();
  const int32_t available = connection.peer->get_available_bytes();
  if (available <= 0) {
    return;
  }
  size_t wanted = static_cast<size_t>(available);
  if (wanted > limits.max_read_bytes_per_connection) {
    wanted = limits.max_read_bytes_per_connection;
  }
  godot::Array result =
      connection.peer->get_partial_data(static_cast<int32_t>(wanted));
  if (result.size() < 2) {
    return;
  }
  const int64_t error = static_cast<int64_t>(result[0]);
  if (error != static_cast<int64_t>(godot::OK)) {
    connection.dead = true;
    return;
  }
  const godot::PackedByteArray chunk =
      static_cast<godot::PackedByteArray>(result[1]);
  const int64_t chunk_size = chunk.size();
  if (chunk_size <= 0) {
    return;
  }
  connection.inbox.append(reinterpret_cast<const char *>(chunk.ptr()),
                           static_cast<size_t>(chunk_size));
}

void transport_enqueue_error_job(TransportConnection &connection,
                                  int status_code,
                                  const std::string &status_text,
                                  const std::string &message) {
  PendingJob job;
  job.use_preset = true;
  job.preset = transport_error_response(status_code, status_text, message);
  job.keep_alive = false;
  transport_enqueue_job(connection, std::move(job));
}

void transport_dispatch(TransportState &state, TransportConnection &connection) {
  while (true) {
    const size_t header_end = connection.inbox.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      if (connection.inbox.size() > http_parse::kMaxHeaderBlockBytes) {
        transport_enqueue_error_job(connection, 400, "Bad Request",
                                    "header block exceeds the length limit");
        connection.inbox.clear();
      }
      return;
    }
    const std::string head = connection.inbox.substr(0, header_end);
    const size_t body_start = header_end + 4;
    const size_t line_end = head.find("\r\n");
    const std::string request_line_text =
        line_end == std::string::npos ? head : head.substr(0, line_end);
    const std::string header_text =
        line_end == std::string::npos ? std::string() : head.substr(line_end + 2);

    http_parse::RequestLine request_line;
    if (!http_parse::parse_request_line(request_line_text, request_line)) {
      transport_enqueue_error_job(connection, 400, "Bad Request",
                                  request_line.error);
      connection.inbox.clear();
      return;
    }
    http_parse::HeaderBlock headers;
    if (!http_parse::parse_headers(header_text, headers)) {
      transport_enqueue_error_job(connection, 400, "Bad Request", headers.error);
      connection.inbox.clear();
      return;
    }
    size_t content_length = 0;
    if (!http_parse::parse_content_length(headers, content_length)) {
      transport_enqueue_error_job(connection, 400, "Bad Request",
                                  "content-length is not a valid length");
      connection.inbox.clear();
      return;
    }
    if (http_parse::decide_body(content_length) ==
        http_parse::BodyDecision::kTooLarge) {
      transport_enqueue_error_job(connection, 413, "Payload Too Large",
                                  "request body exceeds the accepted limit");
      connection.inbox.clear();
      return;
    }
    if (connection.inbox.size() - body_start < content_length) {
      return;
    }

    HttpRequest request;
    request.method = request_line.method;
    request.path = request_line.path;
    request.headers = headers.headers;
    request.body = connection.inbox.substr(body_start, content_length);
    connection.inbox.erase(0, body_start + content_length);
    request.headers[http_parse::kQueryRawHeader] = request_line.query;
    for (const auto &entry : request_line.query_params) {
      request.headers[std::string(http_parse::kQueryHeaderPrefix) + entry.first] =
          entry.second;
    }
    request.headers[kConnectionIdHeader] = std::to_string(connection.id);

    PendingJob job;
    job.request = std::move(request);
    job.keep_alive =
        http_parse::request_keeps_alive(request_line.version, headers);
    const bool keep_alive = job.keep_alive;
    const HttpRouteHandler *handler =
        state.routes.find(job.request.method, job.request.path);
    if (handler != nullptr) {
      job.handler = *handler;
    }
    transport_enqueue_job(connection, std::move(job));
    if (!keep_alive) {
      return;
    }
  }
}

void transport_collect_replies(TransportConnection &connection) {
  std::deque<CompletedReply> replies;
  {
    std::lock_guard<std::mutex> lock(connection.channel->mutex);
    replies.swap(connection.channel->replies);
  }
  for (CompletedReply &reply : replies) {
    const std::string stream_id =
        transport_find_header(reply.response.headers, kStreamIdHeader);
    if (!stream_id.empty()) {
      connection.streaming = true;
      connection.sink = http_stream::acquire(stream_id);
      connection.outbox += transport_build_stream_response(reply.response);
      continue;
    }
    connection.outbox += transport_build_response(reply.response, reply.keep_alive);
    if (!reply.keep_alive) {
      connection.close_after_write = true;
      break;
    }
  }
}

void transport_drain_stream(TransportConnection &connection) {
  if (!connection.streaming || !connection.sink) {
    return;
  }
  const std::vector<std::string> frames = connection.sink->drain();
  for (const std::string &frame : frames) {
    connection.outbox += transport_chunk(frame);
  }
}

void transport_flush(TransportConnection &connection, size_t &write_budget) {
  while (!connection.outbox.empty() && write_budget > 0) {
    size_t chunk_size = connection.outbox.size();
    if (chunk_size > write_budget) {
      chunk_size = write_budget;
    }
    godot::PackedByteArray payload;
    payload.resize(static_cast<int64_t>(chunk_size));
    std::memcpy(payload.ptrw(), connection.outbox.data(), chunk_size);
    godot::Array result = connection.peer->put_partial_data(payload);
    if (result.size() < 2) {
      break;
    }
    const int64_t error = static_cast<int64_t>(result[0]);
    const int64_t written = static_cast<int64_t>(result[1]);
    if (error == static_cast<int64_t>(godot::ERR_BUSY)) {
      break;
    }
    if (error != static_cast<int64_t>(godot::OK)) {
      connection.dead = true;
      return;
    }
    if (written <= 0) {
      break;
    }
    size_t consumed = static_cast<size_t>(written);
    if (consumed > connection.outbox.size()) {
      consumed = connection.outbox.size();
    }
    connection.outbox.erase(0, consumed);
    write_budget -= consumed;
  }
}

void transport_process(TransportState &state, TransportConnection &connection,
                       size_t &write_budget) {
  if (connection.peer.is_null()) {
    connection.dead = true;
    return;
  }
  connection.peer->poll();
  if (connection.peer->get_status() !=
      godot::StreamPeerSocket::STATUS_CONNECTED) {
    connection.dead = true;
    return;
  }
  transport_read(connection);
  if (connection.dead) {
    return;
  }
  if (!connection.streaming) {
    transport_dispatch(state, connection);
  }
  transport_collect_replies(connection);
  transport_drain_stream(connection);
  transport_flush(connection, write_budget);
  if (connection.outbox.empty() && connection.close_after_write) {
    connection.dead = true;
  }
}

port_recovery::RecoveryPlan &transport_residual_plan() {
  static port_recovery::RecoveryPlan plan;
  static bool analyzed = false;
  if (analyzed) {
    return plan;
  }
  analyzed = true;
  const std::string project_path = util::api_host_project_root();
  if (project_path.empty()) {
    return plan;
  }
  const int current_pid = util::api_host_process_id();
  plan = port_recovery::analyze(
      InstanceRegistry::list_all(), project_path, current_pid,
      [](int pid) { return util::api_host_process_alive(pid); });
  for (const port_recovery::ResidualInstance &residual : plan.stale) {
    if (!InstanceRegistry::remove(residual.instance_id)) {
      continue;
    }
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Transport,
        "removed the stale instance registration " + residual.instance_id +
            " (pid " + std::to_string(residual.pid) + ", port " +
            std::to_string(residual.port) + ")");
  }
  for (const InstanceRecord &record : InstanceRegistry::list_all()) {
    if (record.pid == current_pid)
      continue;
    if (port_recovery::same_project_path(record.project_path, project_path))
      continue;
    if (util::api_host_process_alive(record.pid))
      continue;
    if (!InstanceRegistry::remove(record.instance_id))
      continue;
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Transport,
        "removed the dead cross-project instance registration " +
            record.instance_id + " (pid " + std::to_string(record.pid) +
            ", port " + std::to_string(record.port) + ")");
  }
  return plan;
}

std::string transport_residual_hint(int port) {
  const port_recovery::RecoveryPlan &plan = transport_residual_plan();
  return port_recovery::build_hint(plan, port);
}

} // namespace

HttpTransport &http_transport_instance() {
  static HttpTransport transport;
  return transport;
}

HttpRouteTable &http_transport_route_table() { return transport_state_never_destroyed().routes; }

bool http_transport_is_listening() {
  return transport_state_never_destroyed().server.is_valid();
}

size_t http_transport_reap_parked_workers() {
  const std::chrono::steady_clock::time_point deadline =
      transport_worker_deadline();
  return transport_drain_parked_workers(deadline);
}

bool http_transport_bind(uint16_t port, const std::string &host,
                         std::string &error) {
  error.clear();
  if (!transport_host_is_loopback(host)) {
    error = "listen address '" + host + "' is not a loopback address";
    return false;
  }
  TransportState &state = transport_state_never_destroyed();
  transport_release_engine_server(state);
  transport_retire_connections(state.connections, true,
                               transport_worker_deadline());
  state.bound_port = 0;

  transport_residual_plan();

  godot::Ref<godot::TCPServer> server;
  server.instantiate();
  const godot::Error listen_error = server->listen(port, godot::String(host.c_str()));
  if (listen_error != godot::OK) {
    error = "listen failed on " + host + ":" + std::to_string(port) +
            " (error " + std::to_string(static_cast<int>(listen_error)) + ")";
    const std::string hint = transport_residual_hint(static_cast<int>(port));
    if (!hint.empty()) {
      error += "; " + hint;
    }
    return false;
  }
  state.server = server;
  state.bound_port = static_cast<uint16_t>(server->get_local_port());
  return true;
}

bool HttpTransport::listen(uint16_t port, const std::string &host) {
  std::string error;
  if (!http_transport_bind(port, host, error)) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                              "HTTP transport listen failed: " + error);
    return false;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "HTTP transport listening on " + host + ":" +
                                std::to_string(transport_state_never_destroyed().bound_port));
  return true;
}

void HttpTransport::stop() {
  TransportState &state = transport_state_never_destroyed();
  const std::chrono::steady_clock::time_point deadline =
      transport_worker_deadline();
  transport_retire_connections(state.connections, true, deadline);
  transport_wait_parked_workers(deadline);
  transport_release_engine_server(state);
  state.bound_port = 0;
}

uint16_t HttpTransport::bound_port() const { return transport_state_never_destroyed().bound_port; }

void HttpTransport::register_route(const std::string &method,
                                   const std::string &prefix,
                                   HttpRouteHandler handler) {
  transport_state_never_destroyed().routes.add(method, prefix, std::move(handler));
}

void HttpTransport::pump() {
  TransportState &state = transport_state_never_destroyed();
  if (state.server.is_null()) {
    return;
  }
  const HttpPumpLimits &limits = http_pump_limits();
  size_t accepted = 0;
  while (accepted < limits.max_accepts_per_frame &&
         state.server->is_connection_available()) {
    godot::Ref<godot::StreamPeerTCP> peer = state.server->take_connection();
    if (peer.is_null()) {
      break;
    }
    peer->set_no_delay(true);
    if (state.connections.size() >= limits.max_open_connections) {
      peer->disconnect_from_host();
      continue;
    }
    auto connection = std::make_shared<TransportConnection>();
    connection->peer = peer;
    connection->id = state.next_connection_id++;
    connection->channel = std::make_shared<ConnectionChannel>();
    connection->worker = std::thread(transport_worker_main, connection->channel);
    state.connections.push_back(std::move(connection));
    ++accepted;
  }
  size_t serviced = 0;
  size_t write_budget = limits.max_write_bytes_per_frame;
  for (size_t index = 0;
       index < state.connections.size() &&
       serviced < limits.max_connections_per_frame;
       ++index) {
    transport_process(state, *state.connections[index], write_budget);
    ++serviced;
  }
  std::vector<std::shared_ptr<TransportConnection>> survivors;
  std::vector<std::shared_ptr<TransportConnection>> retired;
  survivors.reserve(state.connections.size());
  for (const std::shared_ptr<TransportConnection> &connection :
       state.connections) {
    if (connection->dead) {
      retired.push_back(connection);
    } else {
      survivors.push_back(connection);
    }
  }
  state.connections.swap(survivors);
  transport_retire_connections(retired, false, transport_worker_deadline());
}

} // namespace godot_autopilot
