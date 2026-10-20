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
#include "monitor.hpp"
#include <cstdlib>
#include <chrono>
#include <exception>
#include <map>
#include <string_view>
#include <typeinfo>
#include <vector>
#include <mcp/Content.hpp>
#include <mcp/JsonRpc.hpp>
#include <mcp/server/ServerOptions.hpp>
#include <mcp/protocol/MessageFilter.hpp>
#include <memory>

#include "core/gda_home.hpp"
#include "core/http_transport.hpp"
#include "core/http_transport_ext.hpp"
#include "core/instance_registry.hpp"
#include "core/mcp_http_shim.hpp"
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
  const int preferred_port = resolve_preferred_port();
  port_ = explicit_port > 0
              ? explicit_port
              : (preferred_port > 0 ? preferred_port : GDA_DEFAULT_PORT);
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
  if (assignment.source == PortSource::kAutoAssigned) {
    const std::vector<int> candidates = PortResolver::candidate_sequence(pid);
    int probed = 0;
    bool bound = false;
    for (int candidate : candidates) {
      if (probed >= GDA_PORT_PROBE_LIMIT) {
        break;
      }
      ++probed;
      if (http_transport_bind(static_cast<uint16_t>(candidate), host_, error)) {
        assignment.port = candidate;
        bound = true;
        break;
      }
    }
    if (!bound) {
      last_error_ = "no free port candidate bound after " +
                    std::to_string(probed) + " probes: " + error;
      return false;
    }
    assignment.note = assignment.note + "; bound port " +
                      std::to_string(assignment.port);
  } else {
    if (!http_transport_bind(static_cast<uint16_t>(assignment.port), host_,
                             error)) {
      last_error_ = "the requested port " + std::to_string(assignment.port) +
                    " is unavailable: " + error;
      return false;
    }
  }
  port_ = assignment.port;
  port_assignment_ = assignment;
  requested_port_ = 0;
  return true;
}

bool ServerContext::start() {
  const std::string endpoint = host_ + ":" + std::to_string(port_);
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

    if (!bind_transport()) {
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "MCP server start rejected: " + last_error_);
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport, "MCP server start rejected",
          "endpoint=" + endpoint + " msg=" + last_error_);
      monitor::error_event("server_start_failed", "server", last_error_);
      return false;
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "MCP server initializing: host=" + host_ +
                                  " port=" + std::to_string(port_));

    transport_ = godot_transport_reset();

    mcp::ServerOptions opts;
    opts.server_info = mcp::Implementation{"godot-autopilot", GDA_VERSION};
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
        if (const mcp::JsonValue *tp = req.meta->Find("traceparent")) {
          if (tp->IsString()) {
            traceparent = tp->GetString();
          }
        }
      }
      monitor::begin_request(monitor::request_id_text(req.id), std::string(method), digest,
                             traceparent);
    };
    opts.outgoing_filters = std::make_shared<mcp::FilterPipeline>();
    opts.outgoing_filters->AddFilter(
        std::make_shared<mcp::MessageFilterFuncAdapter>(
            [](const mcp::JsonRpcMessage &message, mcp::MessageFilterNext next) {
              if (const auto *response =
                      std::get_if<mcp::JsonRpcResponse>(&message)) {
                monitor::end_request(
                    monitor::request_id_text(response->id), true, "",
                    static_cast<int64_t>(response->result.Dump().size()));
              } else if (const auto *error =
                             std::get_if<mcp::JsonRpcErrorResponse>(&message)) {
                monitor::note_protocol_error(
                    error->id ? monitor::request_id_text(*error->id) : std::string(),
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
    server_ = mcp::McpServer::Create(transport_, opts);
    if (!server_) {
      last_error_ = "McpServer::Create returned null";
      LogSystem::instance().log(LogLevel::Error, LogCategory::Transport,
                                "MCP server start failed: " + last_error_);
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::Transport, "MCP server start failed",
          "endpoint=" + endpoint + " msg=" + last_error_);
      transport_.reset();
      dispatch::clear_handlers();
      clear_active_registry();
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
    write_instance_record();

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
    if (server_) { try { server_->Close(); } catch (...) {} server_.reset(); }
    if (transport_) { try { transport_->Close(); } catch (...) {} transport_.reset(); }
    http_transport_instance().stop();
    dispatch::clear_handlers();
    clear_active_registry();
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
    if (server_) { try { server_->Close(); } catch (...) {} server_.reset(); }
    if (transport_) { try { transport_->Close(); } catch (...) {} transport_.reset(); }
    http_transport_instance().stop();
    dispatch::clear_handlers();
    clear_active_registry();
    monitor::error_event("server_start_failed", "server", last_error_);
    return false;
  }
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
  record.api_token = "";
  record.endpoints.mcp =
      EndpointEntry{host_, port_, GDA_MCP_PATH, "mcp", "none"};
  record.endpoints.api =
      EndpointEntry{host_, port_, GDA_API_PATH, "http", "none"};
  record.gda_home = HomeResolver::gda_home();
  record.started_at = started_at;
  record.version = GDA_VERSION;
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
  if (http_transport_is_listening()) {
    http_transport_instance().stop();
  }
  dispatch::clear_handlers();
  clear_active_registry();
  remove_instance_record();

  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "MCP server stopped");
  monitor::lifecycle("server_stop",
                     monitor::build_attrs({{"host", host_},
                                           {"port", std::to_string(port_)}}));
}

bool ServerContext::restart(uint16_t port) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "MCP server restart requested on port " +
                                std::to_string(port));
  LogSystem::instance().log_detailed(
      LogLevel::Info, LogCategory::Transport, "MCP server restart requested",
      "endpoint=" + host_ + ":" + std::to_string(port) +
          " previous_port=" + std::to_string(port_));
  monitor::lifecycle("server_restart",
                     monitor::build_attrs({{"host", host_},
                                           {"port", std::to_string(port)}}));
  stop();
  requested_port_ = port;
  port_ = port;
  return start();
}

int ServerContext::get_port() const { return port_; }

const std::string &ServerContext::get_host() const { return host_; }

bool ServerContext::is_running() const { return running_; }

const PortAssignment &ServerContext::port_assignment() const {
  return port_assignment_;
}

void ServerContext::set_port_assignment(const PortAssignment &assignment) {
  port_assignment_ = assignment;
}

const EndpointTable &ServerContext::endpoints() const { return endpoints_; }

void ServerContext::set_endpoints(const EndpointTable &endpoints) {
  endpoints_ = endpoints;
}

void ServerContext::register_tools() {
  register_all_tools(*server_, queue_, *catalog_, *bm25_index_, port_);
  register_all_resources(*server_, queue_);
  register_all_prompts(*server_, queue_);
  register_debugger_resources(*server_, queue_);
  register_debugger_prompts(*server_);
}

} // namespace godot_autopilot
