#include <mcp/JsonRpc.hpp>
#include <mcp/McpError.hpp>
#include <mcp/client/McpClient.hpp>
#include <mcp/protocol/MessageFilter.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/storage/FileTaskStore.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int64_t kExpectedCode =
    static_cast<int64_t>(mcp::McpErrorCode::InternalError);

struct CapturedError {
  int64_t code = 0;
  std::string message;
  std::string wire;
};

std::mutex g_capture_mutex;
std::vector<CapturedError> g_captured;

void record_error(const mcp::JsonRpcErrorResponse &err) {
  CapturedError entry;
  entry.code = static_cast<int64_t>(err.error.code);
  entry.message = err.error.message;
  entry.wire = mcp::SerializeMessage(mcp::JsonRpcMessage{err});
  std::lock_guard<std::mutex> lock(g_capture_mutex);
  g_captured.push_back(std::move(entry));
}

std::shared_ptr<mcp::FilterPipeline> capturing_filters() {
  auto pipeline = std::make_shared<mcp::FilterPipeline>();
  pipeline->AddFilter(std::make_shared<mcp::MessageFilterFuncAdapter>(
      [](const mcp::JsonRpcMessage &message, mcp::MessageFilterNext next) {
        if (const auto *err =
                std::get_if<mcp::JsonRpcErrorResponse>(&message)) {
          record_error(*err);
        }
        next(message);
      }));
  return pipeline;
}

std::filesystem::path blocked_store_path() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "gda_error_code_probe_blocked";
  std::error_code ec;
  std::filesystem::remove(path, ec);
  std::filesystem::create_directories(path, ec);
  return path;
}

int run_probe() {
  const std::filesystem::path store_path = blocked_store_path();
  auto pair = mcp::InMemoryTransport::CreatePair();

  mcp::ServerOptions options;
  options.server_info = mcp::Implementation{"gda-error-code-probe", "0.0.1"};
  options.enable_tasks_extension = true;
  options.task_store = std::make_shared<mcp::FileTaskStore>(store_path);
  options.outgoing_filters = capturing_filters();
  auto server = mcp::McpServer::Create(pair.server, options);

  mcp::ToolOptions tool_options;
  tool_options.execution = mcp::ToolExecution{
      mcp::ToolExecutionMode::Task, std::nullopt, mcp::TaskSupport::Optional};
  server->RegisterTool(
      "probe_echo", tool_options,
      [](const mcp::RequestContext<mcp::CallToolRequestParams> &) {
        return mcp::CallToolResult{};
      });

  mcp::ClientOptions client_options;
  client_options.client_info =
      mcp::Implementation{"gda-error-code-probe-client", "0.0.1"};
  client_options.connect_mode = mcp::ConnectMode::Auto;
  client_options.enable_tasks_extension = true;
  auto client = mcp::McpClient::Create(pair.client, client_options);

  bool client_threw = false;
  int64_t client_code = 0;
  std::string client_message;
  try {
    client->CallToolAsTask("probe_echo");
  } catch (const mcp::McpError &e) {
    client_threw = true;
    client_code = static_cast<int64_t>(e.Code());
    client_message = e.what();
  }

  client->Close();
  server->Close();
  std::error_code ec;
  std::filesystem::remove(store_path, ec);

  std::lock_guard<std::mutex> lock(g_capture_mutex);
  std::cout << "probe captured " << g_captured.size()
            << " error response(s); expected code=" << kExpectedCode
            << std::endl;
  for (const auto &entry : g_captured) {
    std::cout << "filter code=" << entry.code
              << " message=" << entry.message << std::endl;
    std::cout << "filter wire=" << entry.wire << std::endl;
  }
  std::cout << "client threw=" << (client_threw ? "yes" : "no")
            << " code=" << client_code
            << " message=" << client_message << std::endl;

  if (!client_threw || g_captured.empty()) {
    std::cout << "probe result: INCONCLUSIVE (no forced failure captured)"
              << std::endl;
    return 2;
  }
  bool all_clean = client_code == kExpectedCode;
  for (const auto &entry : g_captured) {
    if (entry.code != kExpectedCode) {
      all_clean = false;
    }
  }
  std::cout << "probe result: "
            << (all_clean ? "CLEAN(-32603)" : "GARBAGE_DETECTED") << std::endl;
  return all_clean ? 0 : 1;
}

} // namespace

int main() { return run_probe(); }