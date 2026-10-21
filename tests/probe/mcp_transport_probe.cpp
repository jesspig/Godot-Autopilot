#include <mcp/JsonRpc.hpp>
#include <mcp/ProtocolVersion.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include "core/godot_transport.hpp"
#include "core/mcp_http_shim.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

namespace {

using godot_autopilot::GodotTransport;
using godot_autopilot::HttpRequest;
using godot_autopilot::HttpResponse;
using godot_autopilot::McpHttpShim;
using godot_autopilot::mcp_wire::ParsedRequest;
using godot_autopilot::mcp_wire::parse_request;

const char *kInitializeBody =
    R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{)"
    R"("protocolVersion":"2026-07-28","capabilities":{},)"
    R"("clientInfo":{"name":"gate-probe","version":"1.0"},)"
    R"("_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})";

const char *kInitializedBody =
    R"({"jsonrpc":"2.0","method":"notifications/initialized",)"
    R"("params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})";

const char *kToolsListBody =
    R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{)"
    R"("_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})";

const char *kToolsCallBody =
    R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{)"
    R"("name":"probe_echo","arguments":{},)"
    R"("_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})";

const char *kDiscoverBody =
    R"({"jsonrpc":"2.0","id":1,"method":"server/discover","params":{)"
    R"("_meta":{"io.modelcontextprotocol/clientInfo":{"name":"gate-probe","version":"1.0"},)"
    R"("io.modelcontextprotocol/clientCapabilities":{},)"
    R"("io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})";

constexpr std::chrono::milliseconds kReceiveTimeout{10000};

void register_probe_tool(mcp::McpServer &server) {
  server.RegisterTool(
      "probe_echo", mcp::ToolOptions{},
      [](const mcp::RequestContext<mcp::CallToolRequestParams> &) {
        return mcp::CallToolResult{};
      });
}

mcp::ServerOptions probe_options() {
  mcp::ServerOptions options;
  options.server_info = mcp::Implementation{"godot-autopilot-gate", "0.0.1"};
  return options;
}

bool looks_like_response(const std::string &text) {
  return text.find("\"result\"") != std::string::npos ||
         text.find("\"error\"") != std::string::npos;
}

bool receive_with_timeout(mcp::ITransport &transport, mcp::JsonRpcMessage &out) {
  auto promise = std::make_shared<std::promise<mcp::JsonRpcMessage>>();
  std::future<mcp::JsonRpcMessage> future = promise->get_future();
  std::thread worker([&transport, promise]() {
    std::error_code ec;
    mcp::JsonRpcMessage message;
    transport.GetMessageChannel().AsyncReceive(
        [&](std::error_code recv_ec, mcp::JsonRpcMessage received) {
          ec = recv_ec;
          message = std::move(received);
        });
    if (ec) {
      promise->set_exception(
          std::make_exception_ptr(std::runtime_error(ec.message())));
      return;
    }
    promise->set_value(std::move(message));
  });
  worker.detach();
  if (future.wait_for(kReceiveTimeout) != std::future_status::ready) {
    return false;
  }
  try {
    out = future.get();
  } catch (const std::exception &e) {
    std::cout << "receive failed: " << e.what() << std::endl;
    return false;
  }
  return true;
}

int run_path_a() {
  auto pair = mcp::InMemoryTransport::CreatePair();
  auto server = mcp::McpServer::Create(pair.server, probe_options());
  register_probe_tool(*server);

  pair.client->SendMessageAsync(mcp::DeserializeMessage(kInitializeBody));

  mcp::JsonRpcMessage initialize_response;
  if (!receive_with_timeout(*pair.client, initialize_response)) {
    std::cout << "PATH_A initialize receive failed" << std::endl;
    server->Close();
    return 1;
  }
  const std::string initialize_text = mcp::SerializeMessage(initialize_response);
  std::cout << "PATH_A initialize response: " << initialize_text << std::endl;

  pair.client->SendMessageAsync(mcp::DeserializeMessage(kInitializedBody));
  pair.client->SendMessageAsync(mcp::DeserializeMessage(kToolsListBody));

  mcp::JsonRpcMessage list_response;
  if (!receive_with_timeout(*pair.client, list_response)) {
    std::cout << "PATH_A tools/list receive failed" << std::endl;
    server->Close();
    return 1;
  }
  const std::string list_text = mcp::SerializeMessage(list_response);
  std::cout << "PATH_A tools/list response: " << list_text << std::endl;

  const bool ok =
      looks_like_response(initialize_text) && looks_like_response(list_text);
  std::cout << "PATH_A result: " << (ok ? "PASS" : "FAIL") << std::endl;
  server->Close();
  return ok ? 0 : 1;
}

int run_path_bc() {
  auto transport = std::make_shared<GodotTransport>();
  auto server = mcp::McpServer::Create(transport, probe_options());
  register_probe_tool(*server);

  const ParsedRequest initialize = parse_request(kInitializeBody);
  std::string error;
  if (!transport->submit(initialize, error)) {
    std::cout << "PATH_BC initialize submit failed: " << error << std::endl;
    server->Close();
    return 1;
  }
  std::string initialize_text;
  if (!transport->await_response(initialize.request_id, initialize_text,
                                 kReceiveTimeout)) {
    std::cout << "PATH_BC initialize timed out" << std::endl;
    server->Close();
    return 1;
  }
  std::cout << "PATH_BC initialize response: " << initialize_text << std::endl;

  const ParsedRequest initialized = parse_request(kInitializedBody);
  if (!transport->submit(initialized, error)) {
    std::cout << "PATH_BC notification submit failed: " << error << std::endl;
    server->Close();
    return 1;
  }

  const ParsedRequest tools_list = parse_request(kToolsListBody);
  if (!transport->submit(tools_list, error)) {
    std::cout << "PATH_BC tools/list submit failed: " << error << std::endl;
    server->Close();
    return 1;
  }
  std::string list_text;
  if (!transport->await_response(tools_list.request_id, list_text,
                                 kReceiveTimeout)) {
    std::cout << "PATH_BC tools/list timed out" << std::endl;
    server->Close();
    return 1;
  }
  std::cout << "PATH_BC tools/list response: " << list_text << std::endl;

  const bool ok =
      looks_like_response(initialize_text) && looks_like_response(list_text);
  std::cout << "PATH_BC result: " << (ok ? "PASS" : "FAIL") << std::endl;
  server->Close();
  return ok ? 0 : 1;
}

HttpRequest make_post_request(const std::string &body) {
  HttpRequest request;
  request.method = "POST";
  request.path = "/mcp";
  request.headers["accept"] = "application/json, text/event-stream";
  request.headers["content-type"] = "application/json";
  request.body = body;
  return request;
}

int run_path_shim() {
  const McpHttpShim shim;
  bool ok = true;

  auto transport = godot_autopilot::godot_transport_reset();
  auto server = mcp::McpServer::Create(transport, probe_options());
  register_probe_tool(*server);

  HttpResponse guard_initialize_response;
  shim.handle(make_post_request(kInitializeBody), guard_initialize_response);
  HttpResponse guard_list_response;
  shim.handle(make_post_request(kToolsListBody), guard_list_response);
  std::cout << "PATH_SHIM uninitialized tools/list status: "
            << guard_list_response.status_code << std::endl;
  std::cout << "PATH_SHIM uninitialized tools/list body: "
            << guard_list_response.body << std::endl;
  ok = ok && guard_list_response.status_code == 400 &&
       guard_list_response.body.find("Server not initialized") !=
           std::string::npos;
  server->Close();

  transport = godot_autopilot::godot_transport_reset();
  server = mcp::McpServer::Create(transport, probe_options());
  register_probe_tool(*server);

  HttpResponse initialize_response;
  shim.handle(make_post_request(kInitializeBody), initialize_response);
  std::cout << "PATH_SHIM initialize status: "
            << initialize_response.status_code << std::endl;
  std::cout << "PATH_SHIM initialize body: " << initialize_response.body
            << std::endl;
  ok = ok && initialize_response.status_code == 200 &&
       looks_like_response(initialize_response.body);

  HttpResponse initialized_response;
  shim.handle(make_post_request(kInitializedBody), initialized_response);
  std::cout << "PATH_SHIM notifications/initialized status: "
            << initialized_response.status_code << std::endl;
  std::cout << "PATH_SHIM notifications/initialized body: ["
            << initialized_response.body << "]" << std::endl;
  ok = ok && initialized_response.status_code == 202 &&
       initialized_response.body.empty();

  HttpResponse list_response;
  shim.handle(make_post_request(kToolsListBody), list_response);
  std::cout << "PATH_SHIM tools/list status: " << list_response.status_code
            << std::endl;
  std::cout << "PATH_SHIM tools/list body: " << list_response.body << std::endl;
  ok = ok && list_response.status_code == 200 &&
       looks_like_response(list_response.body);

  HttpResponse call_response;
  shim.handle(make_post_request(kToolsCallBody), call_response);
  std::cout << "PATH_SHIM tools/call status: " << call_response.status_code
            << std::endl;
  std::cout << "PATH_SHIM tools/call body: " << call_response.body << std::endl;
  ok = ok && call_response.status_code == 200 &&
       looks_like_response(call_response.body);

  HttpRequest bad_accept = make_post_request(kToolsListBody);
  bad_accept.headers["accept"] = "application/json";
  HttpResponse bad_accept_response;
  shim.handle(bad_accept, bad_accept_response);
  std::cout << "PATH_SHIM bad accept status: "
            << bad_accept_response.status_code << std::endl;
  ok = ok && bad_accept_response.status_code == 406;

  HttpRequest bad_content_type = make_post_request(kToolsListBody);
  bad_content_type.headers["content-type"] = "text/plain";
  HttpResponse bad_content_type_response;
  shim.handle(bad_content_type, bad_content_type_response);
  std::cout << "PATH_SHIM bad content type status: "
            << bad_content_type_response.status_code << std::endl;
  ok = ok && bad_content_type_response.status_code == 415;

  HttpRequest bad_version = make_post_request(kToolsListBody);
  bad_version.headers["mcp-protocol-version"] = "1999-01-01";
  HttpResponse bad_version_response;
  shim.handle(bad_version, bad_version_response);
  std::cout << "PATH_SHIM bad version status: "
            << bad_version_response.status_code << std::endl;
  ok = ok && bad_version_response.status_code == 400;

  HttpRequest stream_request;
  stream_request.method = "GET";
  stream_request.path = "/mcp";
  stream_request.headers["accept"] = "text/event-stream";
  stream_request.headers["x-gda-connection-id"] = "probe-1";
  HttpResponse stream_response;
  shim.handle(stream_request, stream_response);
  std::cout << "PATH_SHIM event stream status: " << stream_response.status_code
            << std::endl;
  std::cout << "PATH_SHIM event stream body: " << stream_response.body
            << std::endl;
  ok = ok && stream_response.status_code == 200 &&
       stream_response.body.find("event: endpoint") != std::string::npos;

  server->Close();

  transport = godot_autopilot::godot_transport_reset();
  server = mcp::McpServer::Create(transport, probe_options());
  register_probe_tool(*server);

  HttpResponse discover_response;
  shim.handle(make_post_request(kDiscoverBody), discover_response);
  std::cout << "PATH_SHIM server/discover status: "
            << discover_response.status_code << std::endl;
  std::cout << "PATH_SHIM server/discover body: " << discover_response.body
            << std::endl;
  ok = ok && discover_response.status_code == 200 &&
       discover_response.body.find("supportedVersions") != std::string::npos;

  HttpResponse modern_list_response;
  shim.handle(make_post_request(kToolsListBody), modern_list_response);
  std::cout << "PATH_SHIM modern tools/list status: "
            << modern_list_response.status_code << std::endl;
  std::cout << "PATH_SHIM modern tools/list body: " << modern_list_response.body
            << std::endl;
  ok = ok && modern_list_response.status_code == 200 &&
       looks_like_response(modern_list_response.body);

  std::cout << "PATH_SHIM result: " << (ok ? "PASS" : "FAIL") << std::endl;
  server->Close();
  return ok ? 0 : 1;
}

} // namespace

int main() {
  const int path_a = run_path_a();
  const int path_bc = run_path_bc();
  const int path_shim = run_path_shim();
  std::cout << "gate summary: path_a=" << (path_a == 0 ? "PASS" : "FAIL")
            << " path_bc=" << (path_bc == 0 ? "PASS" : "FAIL")
            << " path_shim=" << (path_shim == 0 ? "PASS" : "FAIL") << std::endl;
  return (path_a == 0 && path_bc == 0 && path_shim == 0) ? 0 : 1;
}
