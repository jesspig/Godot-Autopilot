#include "server_context.hpp"
#include "config.hpp"
#include "core/observability/log_system.hpp"
#include "plugin_config.hpp"
#include <version.hpp>
#include "prompts/debugger_prompts.hpp"
#include "prompts/prompt_handlers.hpp"
#include "resources/debugger_resources.hpp"
#include "resources/resource_handlers.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/tool_catalog.hpp"
#include "util/bm25_index.hpp"
#include "adapters/mcp_json_bridge.hpp"
#include "core/observability/monitor.hpp"
#include <cstdlib>
#include <chrono>
#include <cctype>
#include <functional>
#include <thread>
#include <exception>
#include <map>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>
#include <mcp/Content.hpp>
#include <mcp/JsonRpc.hpp>
#include <mcp/server/ServerOptions.hpp>
#include <mcp/storage/FileTaskStore.hpp>
#include <mcp/protocol/MessageFilter.hpp>
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
#include "core/transport/godot_transport.hpp"
#include "core/transport/http_transport.hpp"
#include "core/transport/http_transport_ext.hpp"
#include "core/transport/ipc_endpoint.hpp"
#include "core/transport/ipc_transport.hpp"
#include "core/transport/ipc_waiters.hpp"
#include "core/storage/project_store.hpp"
#include "core/storage/storage_paths.hpp"
#include <cstdlib>
#include <filesystem>
#include "core/storage/instance_guard.hpp"
#include "core/storage/instance_registry.hpp"
#include "core/transport/mcp_http_shim.hpp"
#include "util/api_host_context.hpp"

namespace godot_autopilot {

namespace {
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
  GcpInvocationSink(std::shared_ptr<GodotTransport> transport,
                    IpcWaiterSet *waiters, CommandQueue *queue,
                    gda::control::JobService *jobs,
                    std::function<void()> release_in_flight,
                    std::string instance_id)
      : transport_(std::move(transport)),
        waiters_(waiters),
        queue_(queue),
        jobs_(jobs),
        release_in_flight_(std::move(release_in_flight)),
        instance_id_(std::move(instance_id)) {}

  gda::control::InvocationResult
  dispatch(const gda::control::InvocationRequest &request) override {
    gda::control::InvocationResult result;
    if (!transport_ || waiters_ == nullptr || queue_ == nullptr ||
        jobs_ == nullptr) {
      result.status = gda::gcp::Status::error;
      result.error = gda::gcp::ErrorCode::result_unknown;
      result.error_message = "GCP dispatch sink unavailable";
      return result;
    }
    const std::string connection = "gcp-" + request.request_id;
    godot_autopilot::util::JsonValue arguments(
        godot_autopilot::util::JsonValue::object_tag);
    for (const gda::gcp::Param &param : request.params) {
      if (param.key == "confirm" || param.key == "generation" ||
          param.key == "wait" || param.key == "deferred" ||
          param.key == "await_async" || is_sensitive_param_key(param.key)) {
        continue;
      }
      arguments[param.key] = godot_autopilot::util::JsonValue(param.value);
    }
    std::string traceparent = request.traceparent;
    if (!gda::gcp::valid_traceparent(traceparent)) {
      traceparent = gda::control::TraceService::make_traceparent(
          gda::control::TraceService::new_trace_id(),
          gda::control::TraceService::new_span_id());
    }
    godot_autopilot::util::JsonValue meta(
        godot_autopilot::util::JsonValue::object_tag);
    meta["traceparent"] = godot_autopilot::util::JsonValue(traceparent);
    godot_autopilot::util::JsonValue params(
        godot_autopilot::util::JsonValue::object_tag);
    params["name"] = godot_autopilot::util::JsonValue(request.tool);
    params["arguments"] = std::move(arguments);
    params["_meta"] = std::move(meta);
    godot_autopilot::util::JsonValue document(
        godot_autopilot::util::JsonValue::object_tag);
    document["jsonrpc"] = godot_autopilot::util::JsonValue("2.0");
    document["id"] = godot_autopilot::util::JsonValue(request.request_id);
    document["method"] = godot_autopilot::util::JsonValue("tools/call");
    document["params"] = std::move(params);

    const mcp_wire::ParsedRequest parsed =
        mcp_wire::parse_request(document.Dump());
    if (!parsed.ok) {
      result.status = gda::gcp::Status::error;
      result.error = gda::gcp::ErrorCode::result_unknown;
      result.error_message = "GCP request could not be encoded";
      return result;
    }
    std::string submit_error;
    if (!transport_->submit(parsed, connection, submit_error)) {
      result.status = gda::gcp::Status::error;
      result.error = gda::gcp::ErrorCode::result_unknown;
      result.error_message = submit_error;
      return result;
    }
    const std::string task_id = jobs_->make_task_id();
    jobs_->create(request.instance_id, request.generation, task_id, 0,
                  now_epoch_ms());
    const std::string request_key = parsed.request_id;
    const std::string instance_id = request.instance_id;
    const std::int64_t generation = request.generation;
    const std::int64_t deadline_ms = request.deadline_ms > 0
                                         ? request.deadline_ms
                                         : gda::gcp::kDefaultDeadlineMs;
    bool spawned = false;
    try {
      spawned = waiters_->spawn(
          [transport = transport_, connection, request_key, queue = queue_,
           jobs = jobs_, task_id, instance_id, generation, deadline_ms,
           release = release_in_flight_](const auto &cancelled) {
          std::string response;
          bool cancellation_sent = false;
          const bool ready = transport->await_response(
              connection, request_key, response,
              std::chrono::milliseconds(deadline_ms), &cancellation_sent);
          if (cancelled->load(std::memory_order_acquire)) {
            if (release) {
              release();
            }
            return;
          }
          gda::control::InvocationResult final;
          if (ready) {
            try {
              const godot_autopilot::util::JsonValue doc =
                  godot_autopilot::util::JsonValue::Parse(response);
              const godot_autopilot::util::JsonValue *failure =
                  doc.Find("error");
              if (failure != nullptr && failure->IsObject()) {
                final.status = gda::gcp::Status::error;
                final.error = gda::gcp::ErrorCode::result_unknown;
                final.error_message = failure->Dump();
              } else {
                final.status = gda::gcp::Status::ok;
                final.payload = response;
              }
            } catch (...) {
              final.status = gda::gcp::Status::error;
              final.error = gda::gcp::ErrorCode::result_unknown;
              final.error_message = "GCP response decode failed";
            }
          } else {
            final.status = gda::gcp::Status::error;
            final.error = gda::gcp::ErrorCode::result_unknown;
            final.error_message = "result_unknown";
          }
          queue->submit([jobs, task_id, instance_id, generation,
                         final = std::move(final), release]() {
            const gda::control::TaskKey key{instance_id, generation, task_id};
            const std::int64_t now = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                                         .count();
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
  std::shared_ptr<GodotTransport> transport_;
  IpcWaiterSet *waiters_;
  CommandQueue *queue_;
  gda::control::JobService *jobs_;
  std::function<void()> release_in_flight_;
  std::string instance_id_;
};

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
                            "Server configured on " + host_ + ":" +
                                std::to_string(port_));
  monitor::lifecycle("server_configure",
                     monitor::build_attrs({{"host", host_},
                                           {"port", std::to_string(port_)}}));
}

ServerContext::~ServerContext() {
  if (g_server_context_instance == this) {
    g_server_context_instance = nullptr;
  }
  if (running_ || server_ || transport_) {
    stop();
  }
}

bool ServerContext::bind_transport() {
  const int pid = util::api_host_process_id();
  const int explicit_port = resolve_explicit_port();
  const int preferred_port = resolve_preferred_port();
  PortAssignment assignment;
  if (requested_port_ > 0) {
    assignment.port = requested_port_;
    assignment.source = PortSource::kUserApplied;
    assignment.requested_port = requested_port_;
  } else {
    assignment = PortResolver::resolve(explicit_port, preferred_port, pid);
  }

  std::string error;
  const bool explicit_request =
      requested_port_ > 0 || explicit_port > 0;
  auto bind_exact = [&](int port) {
    return http_transport_bind(static_cast<uint16_t>(port), host_, error);
  };

  if (explicit_request) {
    if (!bind_exact(assignment.port)) {
      last_error_ = "the requested port " + std::to_string(assignment.port) +
                    " is unavailable: " + error;
      return false;
    }
  } else if (preferred_port > 0 && bind_exact(preferred_port)) {
    assignment.port = preferred_port;
    assignment.source = PortSource::kConfigExplicit;
  } else {
    if (preferred_port > 0) {
      const std::string avoided = error;
      assignment.source = PortSource::kAutoReassigned;
      assignment.note = "the persisted port " + std::to_string(preferred_port) +
                        " was unavailable (" + avoided + ")";
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::Transport,
          "Persisted MCP port " + std::to_string(preferred_port) +
              " is unavailable; reassigning a random port");
    } else {
      assignment.source = PortSource::kFirstRandom;
      assignment.note = "first random port assignment";
    }
    const std::vector<int> candidates = PortResolver::candidate_sequence(pid);
    int probed = 0;
    bool bound = false;
    std::string avoided_reason;
    for (int candidate : candidates) {
      if (probed >= GDA_PORT_PROBE_LIMIT) {
        break;
      }
      ++probed;
      if (bind_exact(candidate)) {
        assignment.port = candidate;
        bound = true;
        break;
      }
      if (avoided_reason.empty()) {
        avoided_reason = error;
      }
    }
    if (!bound) {
      last_error_ = "no free port candidate bound after " +
                    std::to_string(probed) + " probes: " + error;
      return false;
    }
    if (!avoided_reason.empty()) {
      assignment.note += "; first bind error: " + avoided_reason;
    }
    if (!PluginConfig::save_port(assignment.port)) {
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::System,
          "Failed to persist MCP port " + std::to_string(assignment.port));
    }
  }

  port_ = assignment.port;
  port_assignment_ = assignment;
  requested_port_ = 0;
  if (assignment.source == PortSource::kUserApplied) {
    if (!PluginConfig::save_port(port_)) {
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::System,
          "Failed to persist MCP port " + std::to_string(port_));
    }
  }
  return true;
}

std::unique_ptr<mcp::McpServer>
ServerContext::create_server(const std::shared_ptr<GodotTransport> &transport,
                             const mcp::ServerOptions &options) {
  return mcp::McpServer::Create(transport, options);
}

bool ServerContext::start() {
  std::string endpoint = host_ + ":" + std::to_string(port_);
  last_error_.clear();
  try {
    if (!is_loopback_host(host_)) {
      last_error_ = "refusing non-loopback listen address '" + host_ +
                    "': remote listening requires application authentication";
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "MCP server start rejected: " + last_error_);
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport, "MCP server start rejected",
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
                                  "MCP server start rejected: " + last_error_);
        LogSystem::instance().log_detailed(
            LogLevel::Error, LogCategory::Transport,
            "MCP server start rejected", "endpoint=" + endpoint + " msg=" +
                                            last_error_);
        monitor::error_event("server_start_failed", "server", last_error_);
        return false;
      }
    }

    if (!bind_transport()) {
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "MCP server start rejected: " + last_error_);
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport, "MCP server start rejected",
          "endpoint=" + endpoint + " msg=" + last_error_);
      monitor::error_event("server_start_failed", "server", last_error_);
      return false;
    }

    endpoint = host_ + ":" + std::to_string(port_);

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "MCP server initializing: host=" + host_ +
                                  " port=" + std::to_string(port_));

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
            "IPC transport unavailable (HTTP only): " + ipc_error);
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
        LogSystem::instance().log(
            LogLevel::Warning, LogCategory::Transport,
            "IPC transport unavailable (HTTP only): " + ipc_error);
      }
#endif
    }

    transport_ = godot_transport_reset();
    ipc_waiters_.open();
    gcp_instance_id_ =
        "gda-gcp-" + std::to_string(util::api_host_process_id());
    gcp_control_ = std::make_unique<gda::control::ControlService>();
    gcp_sink_ = std::make_unique<GcpInvocationSink>(
        transport_, &ipc_waiters_, &queue_, &gcp_control_->jobs(),
        []() {
          ipc_transport::ipc_transport_instance().release_in_flight();
        },
        gcp_instance_id_);
    gcp_dispatcher_ = std::make_unique<gda::control::GcpDispatcher>(
        *gcp_control_, gcp_instance_id_, gcp_sink_.get());
    transport_->set_inbound_rewrite([](mcp::JsonRpcMessage &message) {
      auto *request = std::get_if<mcp::JsonRpcRequest>(&message);
      if (request == nullptr || request->method != "tools/call" ||
          !request->params || !request->params->IsObject()) {
        return;
      }
      godot_autopilot::adapters::SdkJson *params = &*request->params;
      const godot_autopilot::adapters::SdkJson *name_value =
          params->Find("name");
      if (name_value == nullptr || !name_value->IsString()) {
        return;
      }
      const std::string name = name_value->GetString();
      auto canonical = dispatch::resolve_alias_canonical(name);
      if (!canonical) {
        return;
      }
      godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
      inner["name"] = godot_autopilot::util::JsonValue(name);
      if (const godot_autopilot::adapters::SdkJson *arguments =
              params->Find("arguments")) {
        inner["arguments"] = godot_autopilot::adapters::FromSdk(*arguments);
      } else {
        inner["arguments"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
      }
      godot_autopilot::util::JsonValue wrapped(godot_autopilot::util::JsonValue::object_tag);
      wrapped["name"] = godot_autopilot::util::JsonValue("call_tool");
      wrapped["arguments"] = std::move(inner);
      *params = godot_autopilot::adapters::ToSdk(wrapped);
      LogSystem::instance().log(
          LogLevel::Debug, LogCategory::Tools,
          "direct tool alias '" + name + "' rewritten to call_tool wrapper");
    });

    ipc_transport::ipc_transport_instance().set_line_handler(
        [this, transport = transport_](std::uint64_t connection_id,
                                 const std::string &line) {
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
                           mcp_wire::build_error_response_text(
                               -32003, "too many in-flight IPC requests"));
            return;
          }
          const std::string connection =
              "ipc-" + std::to_string(connection_id);
          const mcp_wire::ParsedRequest parsed = mcp_wire::parse_request(line);
          if (!parsed.ok) {
            ipc.queue_line(connection_id,
                           mcp_wire::build_error_response_text(-32700,
                                                               parsed.error));
            ipc.release_in_flight();
            return;
          }
          std::string error;
          if (!transport || !transport->submit(parsed, connection, error)) {
            ipc.queue_line(connection_id,
                           mcp_wire::build_error_response_text(-32603, error));
            ipc.release_in_flight();
            return;
          }
          if (parsed.is_notification) {
            ipc.release_in_flight();
            return;
          }
          const std::string request_id = parsed.request_id;
          mcp::RequestId client_id{int64_t{0}};
          if (const auto *request = mcp::AsRequest(parsed.message)) {
            client_id = request->id;
          }
          bool accepted = false;
          try {
            accepted = ipc_waiters_.spawn(
                [transport, connection, request_id, connection_id,
                 method = parsed.method, client_id](const auto &cancelled) {
                  std::string response;
                  bool cancellation_sent = false;
                  const bool ready = transport->await_response(
                      connection, request_id, response,
                      std::chrono::milliseconds(60000), &cancellation_sent);
                  if (cancelled->load(std::memory_order_acquire)) {
                    ipc_transport::ipc_transport_instance().release_in_flight();
                    return;
                  }
                  if (ready) {
                    ipc_transport::ipc_transport_instance().queue_line(
                        connection_id, std::move(response));
                  } else {
                    ipc_transport::ipc_transport_instance().queue_line(
                        connection_id,
                        mcp_wire::build_timeout_response_text(
                            client_id, method, cancellation_sent));
                  }
                  ipc_transport::ipc_transport_instance().release_in_flight();
                });
          } catch (...) {
            accepted = false;
          }
          if (!accepted) {
            ipc.queue_line(connection_id,
                           mcp_wire::build_error_response_text(
                               -32003, "too many IPC waiters"));
            ipc.release_in_flight();
          }
        });

    mcp::ServerOptions opts;
    opts.server_info = mcp::Implementation{"godot-autopilot", GDA_VERSION};
    opts.enable_progressive_discovery = true;
    route_sdk_logs();
    const std::filesystem::path task_dir = to_fs_path(StoragePaths::task_dir());
    std::error_code task_dir_error;
    std::filesystem::create_directories(task_dir, task_dir_error);
    if (task_dir_error) {
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::Tools,
          "cannot create task directory: " + task_dir_error.message());
    }
    opts.enable_tasks_extension = true;
    opts.enable_skills_extension = true;
    const std::string task_run_stamp = std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    const std::filesystem::path task_path =
        task_dir / ("tasks-" + task_run_stamp + ".jsonl");
    opts.task_store = std::make_shared<mcp::FileTaskStore>(task_path);
    monitor::lifecycle(
        "tasks_enabled",
        monitor::build_attrs({{"path", task_path.string()}}));
    const mcp::CacheHint immediate_expiry_hint{int64_t{0}, "private"};
    opts.cache_hints = std::map<std::string, mcp::CacheHint, std::less<>>{
        {"server/discover", immediate_expiry_hint},
        {"tools/list", immediate_expiry_hint},
        {"prompts/list", immediate_expiry_hint},
        {"resources/list", immediate_expiry_hint},
        {"resources/templates/list", immediate_expiry_hint},
        {"resources/read", immediate_expiry_hint},
    };
    opts.on_method_called = [this](std::string_view method) {
      LogSystem::instance().log(LogLevel::Debug, LogCategory::Transport,
                                "MCP request: " + std::string(method));
    };
    opts.on_client_connected = [](const mcp::Implementation &info) {
      LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                                "Client connected: " + info.name + " " +
                                    info.version);
      monitor::note_client(info.name, info.version, true);
    };
    opts.on_initialized = [] {
      LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                                "Client initialization complete");
    };
    opts.on_protocol_error = [endpoint](std::string_view error) {
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "Protocol error: " + std::string(error));
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport,
          "Protocol error on " + endpoint,
          "endpoint=" + endpoint + " msg=" + std::string(error));
      monitor::error_event("protocol_error", "protocol", std::string(error));
    };
    opts.on_transport_close = [] {
      LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                                "Client disconnected");
      monitor::note_transport("client_disconnected", true);
    };
    opts.on_transport_error = [endpoint](std::string_view msg) {
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "Transport error: " + std::string(msg));
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport,
          "Transport error on " + endpoint,
          "endpoint=" + endpoint + " msg=" + std::string(msg));
      monitor::note_transport("transport_error", false,
                              monitor::build_attrs({{"msg", std::string(msg)}}));
    };
    opts.on_request = [](std::string_view method, const mcp::JsonRpcRequest &req) {
      const std::string digest = req.params ? req.params->Dump() : std::string();
      std::string traceparent;
      if (req.meta) {
        if (const godot_autopilot::adapters::SdkJson *tp =
                req.meta->Find("traceparent")) {
          if (tp->IsString()) {
            traceparent = tp->GetString();
          }
        }
      }
      const std::string internal_request_key = mcp_wire::request_id_key(req.id);
      monitor::begin_request(
          internal_request_key, std::string(method), digest, traceparent,
          godot_transport_instance()->client_request_id_text(internal_request_key));
    };
    opts.outgoing_filters = std::make_shared<mcp::FilterPipeline>();
    opts.outgoing_filters->AddFilter(
        std::make_shared<mcp::MessageFilterFuncAdapter>(
            [](const mcp::JsonRpcMessage &message, mcp::MessageFilterNext next) {
              if (const auto *response =
                      std::get_if<mcp::JsonRpcResponse>(&message)) {
                monitor::end_request(
                    mcp_wire::request_id_key(response->id), true, "",
                    static_cast<int64_t>(response->result.Dump().size()));
              } else if (const auto *error =
                             std::get_if<mcp::JsonRpcErrorResponse>(&message)) {
                const std::string error_request_key =
                    error->id ? mcp_wire::request_id_key(*error->id)
                              : std::string();
                const std::string error_request_id =
                    error->id
                        ? godot_transport_instance()->client_request_id_text(
                              error_request_key)
                        : std::string();
                ErrorPathEvent path_event;
                path_event.request_id = error_request_id;
                path_event.error_code = static_cast<int>(error->error.code);
                path_event.exception_type =
                    classify_error_path_exception(error->error.message);
                if (error->id) {
                  const std::shared_ptr<GodotTransport> transport =
                      godot_transport_instance();
                  transport->pending_request_meta(
                      mcp_wire::request_id_key(*error->id), path_event.method,
                      path_event.connection_id);
                }
                log_error_path(path_event);
                monitor::note_protocol_error(
                    error_request_key,
                    static_cast<int64_t>(error->error.code),
                    error->error.message, 0);
              }
              next(message);
            }));
    opts.on_error = [](const mcp::JsonRpcErrorResponse &err) {
      monitor::note_protocol_error(err.id ? monitor::request_id_text(*err.id) : std::string(),
                                   static_cast<int64_t>(err.error.code),
                                   err.error.message, 0);
    };
    opts.on_notification = [](const mcp::JsonRpcNotification &notif) {
      monitor::note_notification(
          notif.method, notif.params ? notif.params->Dump() : std::string());
    };
    server_ = create_server(transport_, opts);
    if (!server_) {
      last_error_ = "McpServer::Create returned null";
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "MCP server start failed: " + last_error_);
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport, "MCP server start failed",
          "endpoint=" + endpoint + " msg=" + last_error_);
      rollback_start();
      monitor::error_event("server_start_failed", "server", last_error_);
      return false;
    }

    LogSystem::instance().log(LogLevel::Debug, LogCategory::Transport,
                              "Registering tools/catalog/resources/prompts...");

    register_tools();

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "Tool registration complete");

    const McpHttpShim shim;
    const HttpRouteHandler mcp_route = [shim](const HttpRequest &request,
                                               HttpResponse &response) {
      shim.handle(request, response);
    };
    HttpTransport &http_transport = http_transport_instance();
    http_transport.register_route("POST", GDA_MCP_PATH, mcp_route);
    http_transport.register_route("GET", GDA_MCP_PATH, mcp_route);
    http_transport.register_route("DELETE", GDA_MCP_PATH, mcp_route);

    running_ = true;
    instance_started_ = true;
    write_instance_record();
    if (gcp_dispatcher_ != nullptr && !instance_id_.empty()) {
      gcp_instance_id_ = instance_id_;
      gcp_dispatcher_->set_instance_id(instance_id_);
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "MCP server started on " + host_ + ":" +
                                  std::to_string(port_));
    monitor::lifecycle("server_start",
                       monitor::build_attrs({{"host", host_},
                                             {"port", std::to_string(port_)}}));
    return true;
  } catch (const std::exception &e) {
    last_error_ = "transport start failed: " + std::string(typeid(e).name()) +
                  ": " + std::string(e.what());
    LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                              "MCP server start failed: " + last_error_);
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::Transport, "MCP server start failed",
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
                              "MCP server start failed: " + last_error_);
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::Transport, "MCP server start failed",
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
  if (server_) {
    try {
      server_->Close();
    } catch (...) {
    }
    server_.reset();
  }
  if (transport_) {
    try {
      transport_->Close();
    } catch (...) {
    }
    transport_.reset();
  }
  ipc_waiters_.join();
  gcp_dispatcher_.reset();
  gcp_sink_.reset();
  gcp_control_.reset();
  http_transport_instance().stop();
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
  record.endpoints.mcp =
      EndpointEntry{host_, port_, GDA_MCP_PATH, "mcp", "none"};
  if (ipc_transport::ipc_transport_instance().is_listening()) {
    auto &ipc = ipc_transport::ipc_transport_instance();
    record.endpoints.ipc.kind = ipc.kind();
    record.endpoints.ipc.path = ipc.bound_path();
    record.endpoints.ipc.host = ipc.host();
    record.endpoints.ipc.port = ipc.port();
    record.endpoints.ipc.token = ipc_token_;
    record.endpoints.ipc.protocol = "mcp-jsonrpc";
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
  if (!running_ && !server_ && !transport_) {
    ipc_waiters_.join();
    gcp_dispatcher_.reset();
    gcp_sink_.reset();
    gcp_control_.reset();
    http_transport_instance().stop();
    ipc_token_.clear();
    return;
  }
  running_ = false;

  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "MCP server stopping");

  if (server_)
    server_->Close();
  if (transport_)
    transport_->Close();
  server_.reset();
  transport_.reset();
  ipc_waiters_.join();
  gcp_dispatcher_.reset();
  gcp_sink_.reset();
  gcp_control_.reset();
  if (http_transport_is_listening()) {
    http_transport_instance().stop();
  }
  dispatch::clear_handlers();
  clear_active_registry();
  remove_instance_record();
  ipc_token_.clear();

  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "MCP server stopped");
  monitor::lifecycle("server_stop",
                     monitor::build_attrs({{"host", host_},
                                           {"port", std::to_string(port_)}}));
}

int ServerContext::get_port() const { return port_; }

const std::string &ServerContext::get_host() const { return host_; }

bool ServerContext::is_running() const { return running_; }

void ServerContext::register_tools() {
  const ToolSurface surface = resolve_tool_surface();
  register_all_tools(*server_, queue_, *catalog_, *bm25_index_, port_,
                     surface);
  monitor::lifecycle(
      "tool_surface",
      monitor::build_attrs(
          {{"surface", surface == ToolSurface::Full ? "full" : "meta"}}));
  register_all_resources(*server_, queue_);
  register_all_prompts(*server_, queue_);
  register_debugger_resources(*server_, queue_);
  register_debugger_prompts(*server_);
}

} // namespace godot_autopilot