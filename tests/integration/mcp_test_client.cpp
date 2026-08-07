

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

#ifdef GetObject
#undef GetObject
#endif
#endif

#include "mcp_test_client.hpp"

#include <mcp/Content.hpp>
#include <mcp/JsonValue.hpp>
#include <mcp/client/McpClient.hpp>
#include <mcp/transport/StreamableHttpClientTransport.hpp>

#include <exception>
#include <optional>
#include <thread>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace gsd_test {

#ifdef _WIN32
namespace {

class WsaGuard {
public:
  WsaGuard() { WSAStartup(MAKEWORD(2, 2), &data_); }
  ~WsaGuard() { WSACleanup(); }

private:
  WSADATA data_;
};

bool tcp_port_open(int port) {
  WsaGuard wsa;
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET)
    return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(static_cast<u_short>(port));
  const bool open =
      connect(s, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == 0;
  closesocket(s);
  return open;
}

std::string error_json(const std::string &message) {
  mcp::JsonValue obj(mcp::JsonValue::object_tag);
  obj["error"] = message;
  return obj.Dump();
}

} // namespace

struct McpTestClient::Impl {
  int port = 0;
  std::unique_ptr<mcp::McpClient> client;
  std::string last_reason;
  bool last_was_error = false;
};

McpTestClient::McpTestClient(int port) : impl_(std::make_unique<Impl>()) {
  impl_->port = port;
}

McpTestClient::~McpTestClient() = default;

bool McpTestClient::connect(std::chrono::seconds timeout) {
  impl_->client.reset();
  impl_->last_reason.clear();
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (!tcp_port_open(impl_->port)) {
      impl_->last_reason = "TCP 连接失败（端口未监听）";
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    try {
      mcp::HttpClientTransportOptions transport_opts;
      transport_opts.endpoint =
          "http://127.0.0.1:" + std::to_string(impl_->port) + "/mcp";
      transport_opts.name = "gsd-test-client";

      auto factory =
          std::make_shared<mcp::StreamableHttpClientTransport>(transport_opts);
      auto session = factory->Connect();

      mcp::ClientOptions client_opts;
      client_opts.initialization_timeout = std::chrono::seconds(5);
      client_opts.discover_probe_timeout = std::chrono::seconds(5);

      client_opts.input_required_config =
          mcp::ClientOptions::InputRequiredConfig{false, 0,
                                                  std::chrono::seconds(60)};

      impl_->client = mcp::McpClient::Create(session, client_opts);
      return true;
    } catch (const std::exception &e) {
      impl_->last_reason = e.what();
      impl_->client.reset();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } catch (...) {
      impl_->last_reason = "未知异常（非 std::exception）";
      impl_->client.reset();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
  return false;
}

std::vector<ToolSummary> McpTestClient::list_tools() {
  std::vector<ToolSummary> out;
  if (!impl_->client)
    return out;
  const auto result = impl_->client->ListTools();
  out.reserve(result.tools.size());
  for (const auto &tool : result.tools) {
    out.push_back({tool.name, tool.description.value_or("")});
  }
  return out;
}

std::string McpTestClient::call_tool(const std::string &name,
                                     const std::string &args_json) {
  impl_->last_was_error = false;
  if (!impl_->client) {
    impl_->last_was_error = true;
    return error_json("not connected");
  }
  try {
    std::optional<mcp::JsonValue> args;
    if (!args_json.empty())
      args = mcp::JsonValue::Parse(args_json);
    const auto result = impl_->client->CallTool(name, args);
    impl_->last_was_error = result.is_error;

    std::string text;
    for (const auto &content : result.content) {
      if (const auto *tc = std::get_if<mcp::TextContent>(&content)) {
        text += tc->text;
      }
    }
    if (text.empty() && result.structured_content) {
      text = result.structured_content->Dump();
    }
    if (text.empty()) {
      text = mcp::JsonValue(mcp::JsonValue::object_tag).Dump();
    }
    return text;
  } catch (const std::exception &e) {
    impl_->last_was_error = true;
    return error_json(e.what());
  } catch (...) {
    impl_->last_was_error = true;
    return error_json("未知异常（非 std::exception）");
  }
}

std::string McpTestClient::call_tool(const std::string &name) {
  return call_tool(name, "");
}

bool McpTestClient::last_call_was_error() const {
  return impl_->last_was_error;
}

#else
struct McpTestClient::Impl {};

McpTestClient::McpTestClient(int) : impl_(std::make_unique<Impl>()) {}
McpTestClient::~McpTestClient() = default;

bool McpTestClient::connect(std::chrono::seconds) { return false; }
std::vector<ToolSummary> McpTestClient::list_tools() { return {}; }
std::string McpTestClient::call_tool(const std::string &, const std::string &) {
  return R"({"error":"L2 测试仅支持 Windows"})";
}
std::string McpTestClient::call_tool(const std::string &) {
  return R"({"error":"L2 测试仅支持 Windows"})";
}
bool McpTestClient::last_call_was_error() const { return true; }
#endif

} // namespace gsd_test
