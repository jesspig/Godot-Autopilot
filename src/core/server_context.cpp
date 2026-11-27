#include "server_context.hpp"
#include "config.hpp"
#include "core/observability/log_system.hpp"
#include "plugin_config.hpp"
#include <version.hpp>
#include "tools/base/register_all.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/tool_catalog.hpp"
#include "util/bm25_index.hpp"
#include "core/observability/monitor.hpp"
#include "core/control/native_call.hpp"
#include <cstdlib>
#include <chrono>
#include <cctype>
#include <functional>
#include <exception>
#include <map>
#include <string_view>
#include <utility>
#include <vector>
#include <filesystem>
#include <memory>

#include "core/control/control_service.hpp"
#include "core/control/gcp_dispatcher.hpp"
#include "core/control/invocation_service.hpp"
#include "core/control/trace_service.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "util/json_value.hpp"
#include "core/transport/api_token.hpp"
#include "core/path_util.hpp"
#include "core/storage/gda_home.hpp"
#include "core/transport/ipc_endpoint.hpp"
#include "core/transport/ipc_transport.hpp"
#include "core/transport/ipc_waiters.hpp"
#include "core/storage/project_store.hpp"
#include "core/storage/storage_paths.hpp"
#include "core/storage/instance_guard.hpp"
#include "core/storage/instance_registry.hpp"
#include "util/api_host_context.hpp"

namespace godot_autopilot {

namespace {

using JsonValue = godot_autopilot::util::JsonValue;

bool is_loopback_host(std::string_view host) {
  return host == "127.0.0.1" || host == "::1" || host == "[::1]";
}

ServerContext *g_server_context_instance = nullptr;

int64_t now_epoch_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

bool is_sensitive_param_key(const std::string &key) {
  std::string lowered;
  for (char character : key) {
    lowered.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }
  return lowered.find("token") != std::string::npos ||
         lowered.find("secret") != std::string::npos ||
         lowered.find("password") != std::string::npos ||
         lowered.find("credential") != std::string::npos ||
         lowered.find("api_key") != std::string::npos ||
         lowered.find("apikey") != std::string::npos;
}

class GcpInvocationSink final : public gda::control::InvocationSink {
public:
  GcpInvocationSink(IpcWaiterSet *waiters, CommandQueue *queue,
                    gda::control::JobService *jobs,
                    std::function<void()> release_in_flight,
                    std::string instance_id)
      : waiters_(waiters),
        queue_(queue),
        jobs_(jobs),
        release_in_flight_(std::move(release_in_flight)),
        instance_id_(std::move(instance_id)) {}

  gda::control::InvocationResult
  dispatch(const gda::control::InvocationRequest &request) override {
    gda::control::InvocationResult result;
    if (waiters_ == nullptr || queue_ == nullptr || jobs_ == nullptr) {
      result.status = gda::gcp::Status::error;
      result.error = gda::gcp::ErrorCode::result_unknown;
      result.error_message = "GCP dispatch sink unavailable";
      return result;
    }
    JsonValue arguments(JsonValue::object_tag);
    for (const gda::gcp::Param &param : request.params) {
      if (param.key == "tool" || param.key == "generation" ||
          param.key == "wait" || param.key == "deferred" ||
          param.key == "await_async" || is_sensitive_param_key(param.key)) {
        continue;
      }
      if (param.key == "confirm") {
        arguments[param.key] =
            JsonValue(param.value == "true" || param.value == "1" ||
                      param.value == "yes");
      } else {
        arguments[param.key] = JsonValue(param.value);
      }
    }
    std::string traceparent = request.traceparent;
    if (!gda::gcp::valid_traceparent(traceparent)) {
      traceparent = gda::control::TraceService::make_traceparent(
          gda::control::TraceService::new_trace_id(),
          gda::control::TraceService::new_span_id());
    }
    const std::string task_id = jobs_->make_task_id();
    jobs_->create(request.instance_id, request.generation, task_id, 0,
                  now_epoch_ms());
    const std::string instance_id = request.instance_id;
    const std::int64_t generation = request.generation;
    const std::string tool = request.tool;
    const std::int64_t deadline_ms = request.deadline_ms > 0
                                         ? request.deadline_ms
                                         : gda::gcp::kDefaultDeadlineMs;
    bool spawned = false;
    try {
      spawned = waiters_->spawn(
          [tool, arguments, traceparent, deadline_ms, queue = queue_,
           jobs = jobs_, task_id, instance_id, generation,
           release = release_in_flight_](const auto &cancelled) {
            gda::control::native::NativeCallResult call =
                gda::control::native::call_tool(tool, arguments, traceparent);
            if (call.pending) {
              call = gda::control::native::await_pending(call, deadline_ms);
            }
            if (cancelled->load(std::memory_order_acquire)) {
              if (release) {
                release();
              }
              return;
            }
            gda::control::InvocationResult final;
            if (godot_autopilot::tool_result_is_error(call.payload)) {
              final.status = gda::gcp::Status::error;
              final.error = gda::gcp::ErrorCode::result_unknown;
              final.error_message = call.payload.Dump();
            } else {
              final.status = gda::gcp::Status::ok;
              final.payload = call.result.Dump();
            }
            final.task_id = task_id;
            queue->submit([jobs, task_id, instance_id, generation,
                           final = std::move(final), release]() {
              const gda::control::TaskKey key{instance_id, generation, task_id};
              const std::int64_t now = now_epoch_ms();
              if (final.status == gda::gcp::Status::ok) {
                jobs->set_result(key, final.payload, now);
                jobs->advance(key, gda::gcp::TaskState::succeeded, now);
              } else {
                jobs->fail(key, final.error, final.error_message, now);
              }
              if (release) {
                release();
              }
            });
          });
    } catch (...) {
      spawned = false;
    }
    if (!spawned) {
      const gda::control::TaskKey key{instance_id, generation, task_id};
      jobs_->fail(key, gda::gcp::ErrorCode::result_unknown,
                  "GCP waiter unavailable", now_epoch_ms());
      result.status = gda::gcp::Status::error;
      result.error = gda::gcp::ErrorCode::result_unknown;
      result.error_message = "GCP waiter unavailable";
      return result;
    }
    result.status = gda::gcp::Status::pending;
    result.task_id = task_id;
    result.payload = "pending";
    return result;
  }

private:
  IpcWaiterSet *waiters_;
  CommandQueue *queue_;
  gda::control::JobService *jobs_;
  std::function<void()> release_in_flight_;
  std::string instance_id_;
};

struct NativeParsed {
  bool ok{false};
  bool is_request{false};
  JsonValue id;
  std::string method;
  JsonValue params;
  std::string error;
};

NativeParsed parse_native_request(const std::string &body) {
  NativeParsed parsed;
  JsonValue doc;
  try {
    doc = JsonValue::Parse(body);
  } catch (const godot_autopilot::util::JsonError &error) {
    parsed.error = std::string("message parse failed: ") + error.what();
    return parsed;
  } catch (...) {
    parsed.error = "message parse failed: unknown exception";
    return parsed;
  }
  if (!doc.IsObject()) {
    parsed.error = "message parse failed: root is not an object";
    return parsed;
  }
  const JsonValue *jsonrpc = doc.Find("jsonrpc");
  if (jsonrpc == nullptr || !jsonrpc->IsString() ||
      jsonrpc->GetString() != "2.0") {
    parsed.error = "message parse failed: jsonrpc must be 2.0";
    return parsed;
  }
  const JsonValue *method = doc.Find("method");
  if (method == nullptr || !method->IsString()) {
    parsed.error = "message parse failed: method must be a string";
    return parsed;
  }
  parsed.method = method->GetString();
  const JsonValue *id = doc.Find("id");
  if (id != nullptr) {
    parsed.is_request = true;
    parsed.id = *id;
  }
  const JsonValue *params = doc.Find("params");
  parsed.params = params != nullptr && params->IsObject()
                      ? *params
                      : JsonValue(JsonValue::object_tag);
  parsed.ok = true;
  return parsed;
}

std::string task_state_text(gda::gcp::TaskState state) {
  switch (state) {
    case gda::gcp::TaskState::succeeded:
      return "completed";
    case gda::gcp::TaskState::failed:
      return "failed";
    case gda::gcp::TaskState::cancelled:
      return "cancelled";
    case gda::gcp::TaskState::cancelling:
      return "cancelling";
    case gda::gcp::TaskState::running:
      return "working";
    case gda::gcp::TaskState::blocked:
      return "blocked";
    case gda::gcp::TaskState::pending:
      return "pending";
  }
  return "pending";
}

} // namespace

ServerContext *ServerContext::instance() { return g_server_context_instance; }

int ServerContext::resolve_explicit_port() {
  const char *env_port = std::getenv("GODOT_AUTOPILOT_PORT");
  if (env_port == nullptr || *env_port == '\0') {
    return 0;
  }
  const int parsed = std::atoi(env_port);
  if (parsed <= 0 || parsed > 65535) {
    return 0;
  }
  return parsed;
}

int ServerContext::resolve_preferred_port() {
  const int saved_port = PluginConfig::load_port();
  if (saved_port <= 0 || saved_port > 65535) {
    return 0;
  }
  return saved_port;
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
  const int explicit_port = resolve_explicit_port();
  const int preferred_port =
      explicit_port > 0 ? 0 : resolve_preferred_port();
  port_ = explicit_port > 0 ? explicit_port : preferred_port;
  host_ = resolve_host();
  g_server_context_instance = this;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "Native control plane configured on " + host_ +
                                ":" + std::to_string(port_));
  monitor::lifecycle("server_configure",
                     monitor::build_attrs({{"host", host_},
                                           {"port", std::to_string(port_)}}));
}

ServerContext::~ServerContext() {
  if (g_server_context_instance == this) {
    g_server_context_instance = nullptr;
  }
  if (running_ || gcp_dispatcher_) {
    stop();
  }
}

bool ServerContext::bind_transport() {
  const int explicit_port = resolve_explicit_port();
  const int preferred_port = resolve_preferred_port();
  if (explicit_port > 0) {
    port_ = explicit_port;
    port_assignment_.port = port_;
    port_assignment_.source = PortSource::kEnvExplicit;
  } else if (preferred_port > 0) {
    port_ = preferred_port;
    port_assignment_.port = port_;
    port_assignment_.source = PortSource::kConfigExplicit;
  } else {
    port_ = GDA_DEFAULT_PORT;
    port_assignment_.port = port_;
    port_assignment_.source = PortSource::kFirstRandom;
  }
  return true;
}

bool ServerContext::start() {
  std::string endpoint = host_ + ":" + std::to_string(port_);
  last_error_.clear();
  try {
    if (!is_loopback_host(host_)) {
      last_error_ = "refusing non-loopback listen address '" + host_ +
                    "': remote listening requires application authentication";
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "Native control plane start rejected: " +
                                    last_error_);
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport,
          "Native control plane start rejected",
          "endpoint=" + endpoint + " msg=" + last_error_);
      monitor::error_event("server_start_failed", "server", last_error_);
      return false;
    }

    if (!instance_started_ && !InstanceGuard::multi_instance_allowed()) {
      const InstanceConflict conflict = InstanceGuard::detect(
          util::api_host_project_root(), util::api_host_process_id());
      if (conflict.conflict) {
        last_error_ = InstanceGuard::conflict_message(conflict);
        LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                  "Native control plane start rejected: " +
                                      last_error_);
        LogSystem::instance().log_detailed(
            LogLevel::Error, LogCategory::Transport,
            "Native control plane start rejected", "endpoint=" + endpoint +
                                                       " msg=" + last_error_);
        monitor::error_event("server_start_failed", "server", last_error_);
        return false;
      }
    }

    if (!bind_transport()) {
      last_error_ = "native control plane bind failed";
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "Native control plane start rejected: " +
                                    last_error_);
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport,
          "Native control plane start rejected",
          "endpoint=" + endpoint + " msg=" + last_error_);
      rollback_start();
      monitor::error_event("server_start_failed", "server", last_error_);
      return false;
    }

    endpoint = host_ + ":" + std::to_string(port_);
    ipc_token_ = api_token::generate_token();
    if (ipc_token_.empty()) {
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "IPC token generation failed; IPC will be "
                                "disabled");
    }
    if (!ipc_token_.empty()) {
#ifdef _WIN32
      std::string ipc_error;
      if (ipc_transport::ipc_transport_instance().listen_tcp(
              "127.0.0.1", 0, ipc_token_, ipc_error)) {
        LogSystem::instance().log(
            LogLevel::Info, LogCategory::Transport,
            "IPC transport listening on tcp 127.0.0.1:" +
                std::to_string(
                    ipc_transport::ipc_transport_instance().port()));
      } else {
        LogSystem::instance().log(
            LogLevel::Warning, LogCategory::Transport,
            "IPC transport unavailable: " + ipc_error);
      }
#else
      const std::string project_root = util::api_host_project_root();
      std::string project_name;
      const std::size_t slash = project_root.find_last_of("/\\");
      if (slash != std::string::npos && slash + 1 < project_root.size()) {
        project_name = project_root.substr(slash + 1);
      }
      const std::filesystem::path run_dir =
          std::filesystem::path(ProjectStore::root_dir()) / "run";
      const std::filesystem::path fallback_dir = run_dir;
      std::error_code ec;
      std::filesystem::create_directories(run_dir, ec);
      const std::string socket_path = ipc_endpoint::derive_socket_path(
          project_name, project_root, run_dir.string(), fallback_dir.string());
      std::filesystem::create_directories(
          std::filesystem::path(socket_path).parent_path(), ec);
      std::filesystem::remove(socket_path, ec);
      std::string ipc_error;
      if (ipc_transport::ipc_transport_instance().listen_unix(
              socket_path, ipc_token_, ipc_error)) {
        LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                                  "IPC transport listening: " + socket_path);
      } else {
        LogSystem::instance().log(LogLevel::Warning, LogCategory::Transport,
                                  "IPC transport unavailable: " + ipc_error);
      }
#endif
    }

    ipc_waiters_.open();
    gcp_instance_id_ =
        "gda-gcp-" + std::to_string(util::api_host_process_id());
    gcp_control_ = std::make_unique<gda::control::ControlService>();
    gcp_sink_ = std::make_unique<GcpInvocationSink>(
        &ipc_waiters_, &queue_, &gcp_control_->jobs(),
        []() {
          ipc_transport::ipc_transport_instance().release_in_flight();
        },
        gcp_instance_id_);
    gcp_dispatcher_ = std::make_unique<gda::control::GcpDispatcher>(
        *gcp_control_, gcp_instance_id_, gcp_sink_.get());

    ipc_transport::ipc_transport_instance().set_line_handler(
        [this](std::uint64_t connection_id, const std::string &line) {
          auto &ipc = ipc_transport::ipc_transport_instance();
          if (gcp_dispatcher_ != nullptr &&
              gda::control::GcpDispatcher::looks_like_gcp(line)) {
            if (!ipc.acquire_in_flight()) {
              gda::gcp::GcpEnvelope envelope;
              envelope.v = gda::gcp::kEnvelopeVersion;
              envelope.kind = gda::gcp::Kind::response;
              envelope.id = "gcp-backpressure";
              envelope.instance_id = gcp_instance_id_;
              envelope.method = "hello";
              envelope.trace.traceparent =
                  gda::control::TraceService::make_traceparent(
                      gda::control::TraceService::new_trace_id(),
                      gda::control::TraceService::new_span_id());
              envelope.status = gda::gcp::Status::error;
              envelope.error = gda::gcp::ErrorCode::backpressure_exceeded;
              envelope.error_message = "too many in-flight IPC requests";
              envelope.request_id = std::string("unknown");
              std::string frame;
              if (gda::control::GcpDispatcher::encode(envelope, frame)) {
                ipc.queue_line(connection_id, std::move(frame));
              }
              return;
            }
            const gda::control::DispatchResult result =
                gcp_dispatcher_->handle_frame(connection_id, line);
            if (!result.response_text.empty()) {
              ipc.queue_line(connection_id, result.response_text);
            }
            for (const gda::control::OutboxEntry &event :
                 gcp_dispatcher_->drain_events()) {
              ipc.queue_line(event.connection_id, event.frame);
            }
            if (result.state != gda::control::DispatchState::awaiting) {
              ipc.release_in_flight();
            }
            return;
          }
          if (!ipc.acquire_in_flight()) {
            ipc.queue_line(connection_id,
                           gda::control::native::jsonrpc_error(
                               JsonValue(), -32003,
                               "too many in-flight IPC requests"));
            return;
          }
          const NativeParsed parsed = parse_native_request(line);
          if (!parsed.ok) {
            ipc.queue_line(connection_id,
                           gda::control::native::jsonrpc_error(
                               JsonValue(), -32700, parsed.error));
            ipc.release_in_flight();
            return;
          }
          if (!parsed.is_request) {
            ipc.release_in_flight();
            return;
          }
          const std::string method = parsed.method;
          if (method == "tools/call") {
            const JsonValue *name = parsed.params.Find("name");
            if (name == nullptr || !name->IsString() ||
                name->GetString().empty()) {
              ipc.queue_line(connection_id,
                             gda::control::native::jsonrpc_error(
                                 parsed.id, -32602, "name is required"));
              ipc.release_in_flight();
              return;
            }
            JsonValue arguments(JsonValue::object_tag);
            if (const JsonValue *raw = parsed.params.Find("arguments");
                raw != nullptr && raw->IsObject()) {
              arguments = *raw;
            }
            std::string traceparent;
            if (const JsonValue *meta = parsed.params.Find("_meta");
                meta != nullptr && meta->IsObject()) {
              if (const JsonValue *value = meta->Find("traceparent");
                  value != nullptr && value->IsString()) {
                traceparent = value->GetString();
              }
            }
            const JsonValue *input_responses =
                parsed.params.Find("inputResponses");
            bool accepted = false;
            try {
              accepted = ipc_waiters_.spawn(
                  [connection_id, id = parsed.id, name = name->GetString(),
                   arguments, traceparent,
                   input_responses_copy =
                       input_responses == nullptr
                           ? JsonValue()
                           : *input_responses](const auto &cancelled) {
                    gda::control::native::NativeCallResult call =
                        gda::control::native::call_tool(
                            name, arguments, traceparent,
                            input_responses_copy.IsObject()
                                ? &input_responses_copy
                                : nullptr);
                    if (call.pending) {
                      call = gda::control::native::await_pending(call,
                                                                 60000);
                    }
                    if (cancelled->load(std::memory_order_acquire)) {
                      ipc_transport::ipc_transport_instance()
                          .release_in_flight();
                      return;
                    }
                    ipc_transport::ipc_transport_instance().queue_line(
                        connection_id,
                        gda::control::native::jsonrpc_result(id,
                                                             call.result));
                    ipc_transport::ipc_transport_instance()
                        .release_in_flight();
                  });
            } catch (...) {
              accepted = false;
            }
            if (!accepted) {
              ipc.queue_line(connection_id,
                             gda::control::native::jsonrpc_error(
                                 parsed.id, -32003, "too many IPC waiters"));
              ipc.release_in_flight();
            }
            return;
          }
          if (method == "initialize") {
            JsonValue result(JsonValue::object_tag);
            result["protocolVersion"] = JsonValue("2025-11-25");
            result["capabilities"] = JsonValue(JsonValue::object_tag);
            JsonValue server_info(JsonValue::object_tag);
            server_info["name"] = JsonValue("godot-autopilot");
            server_info["version"] = JsonValue(GDA_VERSION);
            result["serverInfo"] = std::move(server_info);
            ipc.queue_line(connection_id,
                           gda::control::native::jsonrpc_result(parsed.id,
                                                                result));
            ipc.release_in_flight();
            return;
          }
          if (method == "ping") {
            ipc.queue_line(connection_id,
                           gda::control::native::jsonrpc_result(
                               parsed.id, JsonValue(JsonValue::object_tag)));
            ipc.release_in_flight();
            return;
          }
          if (method == "tools/list") {
            JsonValue tools(JsonValue::array_tag);
            if (catalog_ != nullptr) {
              for (const ToolInfo &info : catalog_->get_all_tools()) {
                JsonValue item(JsonValue::object_tag);
                item["name"] = JsonValue(info.name);
                item["description"] = JsonValue(info.description);
                item["inputSchema"] = info.input_schema;
                tools.PushBack(std::move(item));
              }
            }
            JsonValue result(JsonValue::object_tag);
            result["tools"] = std::move(tools);
            ipc.queue_line(connection_id,
                           gda::control::native::jsonrpc_result(parsed.id,
                                                                result));
            ipc.release_in_flight();
            return;
          }
          if (method == "tasks/get") {
            const JsonValue *task_id = parsed.params.Find("taskId");
            const std::string id = task_id != nullptr && task_id->IsString()
                                       ? task_id->GetString()
                                       : std::string();
            JsonValue result(JsonValue::object_tag);
            result["taskId"] = JsonValue(id);
            if (id.empty() || gcp_control_ == nullptr) {
              result["status"] = JsonValue("failed");
              result["error"] = JsonValue("task not found");
            } else {
              std::int64_t generation = 0;
              if (const JsonValue *value = parsed.params.Find("generation");
                  value != nullptr && value->IsInt()) {
                generation = value->GetInt();
              }
              const gda::control::TaskKey key{instance_id_, generation, id};
              const auto record = gcp_control_->jobs().get(key);
              if (!record.has_value()) {
                result["status"] = JsonValue("failed");
                result["error"] = JsonValue("task not found");
              } else {
                result["status"] = JsonValue(task_state_text(record->state));
                result["state"] = JsonValue(task_state_text(record->state));
                result["result"] = JsonValue(record->result);
                result["error"] = JsonValue(record->error_message);
              }
            }
            ipc.queue_line(connection_id,
                           gda::control::native::jsonrpc_result(parsed.id,
                                                                result));
            ipc.release_in_flight();
            return;
          }
          ipc.queue_line(connection_id,
                         gda::control::native::jsonrpc_error(
                             parsed.id, -32601,
                             "method not supported: " + method));
          ipc.release_in_flight();
        });

    register_tools();

    running_ = true;
    instance_started_ = true;
    write_instance_record();
    if (gcp_dispatcher_ != nullptr && !instance_id_.empty()) {
      gcp_instance_id_ = instance_id_;
      gcp_dispatcher_->set_instance_id(instance_id_);
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "Native control plane started on " + host_ +
                                  ":" + std::to_string(port_));
    monitor::lifecycle("server_start",
                       monitor::build_attrs({{"host", host_},
                                             {"port", std::to_string(port_)}}));
    return true;
  } catch (const std::exception &e) {
    last_error_ = "transport start failed: " + std::string(typeid(e).name()) +
                  ": " + std::string(e.what());
    LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                              "Native control plane start failed: " +
                                  last_error_);
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::Transport,
        "Native control plane start failed",
        "endpoint=" + endpoint + " msg=" + last_error_);
    rollback_start();
    monitor::error_event("server_start_failed", "server", last_error_);
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
                              "Native control plane start failed: " +
                                  last_error_);
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::Transport,
        "Native control plane start failed",
        "endpoint=" + endpoint + " msg=" + last_error_);
    rollback_start();
    monitor::error_event("server_start_failed", "server", last_error_);
    return false;
  }
}

void ServerContext::rollback_start() {
  running_ = false;
  ipc_waiters_.cancel();
  ipc_transport::ipc_transport_instance().stop();
  ipc_waiters_.join();
  gcp_dispatcher_.reset();
  gcp_sink_.reset();
  gcp_control_.reset();
  dispatch::clear_handlers();
  clear_active_registry();
  remove_instance_record();
  ipc_token_.clear();
}

void ServerContext::write_instance_record() {
  const std::string project_path = util::api_host_project_root();
  const int pid = util::api_host_process_id();
  const int64_t started_at = now_epoch_ms();
  InstanceRecord record;
  record.instance_id = "gda-" + ProjectStore::project_path_hash(project_path) +
                        "-" + std::to_string(pid) + "-" +
                        std::to_string(started_at);
  record.port = port_;
  record.pid = pid;
  record.project_path = project_path;
  if (ipc_transport::ipc_transport_instance().is_listening()) {
    auto &ipc = ipc_transport::ipc_transport_instance();
    record.endpoints.ipc.kind = ipc.kind();
    record.endpoints.ipc.path = ipc.bound_path();
    record.endpoints.ipc.host = ipc.host();
    record.endpoints.ipc.port = ipc.port();
    record.endpoints.ipc.token = ipc_token_;
    record.endpoints.ipc.protocol = "gcp-native";
    record.endpoints.ipc.framing = "length-prefixed-json";
    record.endpoints.ipc.auth = "token";
    record.endpoints.ipc.version = 1;
    record.endpoints.ipc.present = true;
  }
  record.gda_home = HomeResolver::gda_home();
  record.started_at = started_at;
  record.version = GDA_VERSION;
  record.port_source = port_source_name(port_assignment_.source);
  if (!InstanceRegistry::write(record)) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "Instance registry write failed for port " +
                                  std::to_string(port_));
    return;
  }
  instance_id_ = record.instance_id;
}

void ServerContext::remove_instance_record() {
  if (instance_id_.empty()) {
    return;
  }
  if (!InstanceRegistry::remove(instance_id_)) {
    LogSystem::instance().log(LogLevel::Warning, LogCategory::System,
                              "Instance registry entry removal failed: " +
                                  instance_id_);
  }
  instance_id_.clear();
}

void ServerContext::stop() {
  ipc_waiters_.cancel();
  ipc_transport::ipc_transport_instance().stop();
  if (!running_ && !gcp_dispatcher_) {
    ipc_waiters_.join();
    gcp_dispatcher_.reset();
    gcp_sink_.reset();
    gcp_control_.reset();
    ipc_token_.clear();
    return;
  }
  running_ = false;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "Native control plane stopping");
  ipc_waiters_.join();
  gcp_dispatcher_.reset();
  gcp_sink_.reset();
  gcp_control_.reset();
  dispatch::clear_handlers();
  clear_active_registry();
  remove_instance_record();
  ipc_token_.clear();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "Native control plane stopped");
  monitor::lifecycle("server_stop",
                     monitor::build_attrs({{"host", host_},
                                           {"port", std::to_string(port_)}}));
}

int ServerContext::get_port() const { return port_; }

const std::string &ServerContext::get_host() const { return host_; }

bool ServerContext::is_running() const { return running_; }

void ServerContext::register_tools() {
  const ToolSurface surface = resolve_tool_surface();
  install_tool_handlers(queue_, *catalog_, *bm25_index_, port_, surface);
  monitor::lifecycle(
      "tool_surface",
      monitor::build_attrs(
          {{"surface", surface == ToolSurface::Full ? "full" : "meta"}}));
}

} // namespace godot_autopilot
