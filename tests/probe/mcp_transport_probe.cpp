#include <mcp/JsonRpc.hpp>
#include <mcp/ProtocolVersion.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include "core/godot_transport.hpp"

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

} // namespace

int main() {
  const int path_a = run_path_a();
  const int path_bc = run_path_bc();
  std::cout << "gate summary: path_a=" << (path_a == 0 ? "PASS" : "FAIL")
            << " path_bc=" << (path_bc == 0 ? "PASS" : "FAIL") << std::endl;
  return (path_a == 0 && path_bc == 0) ? 0 : 1;
}
