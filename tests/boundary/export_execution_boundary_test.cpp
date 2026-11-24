#include "core/command_queue.hpp"
#include "core/export_guard.hpp"
#include "prompts/prompt_handlers.hpp"
#include "resources/debugger_resources.hpp"
#include "resources/resource_handlers.hpp"
#include "tools/base/dispatch.hpp"

#include <gtest/gtest.h>

#include <mcp/client/McpClient.hpp>
#include <mcp/McpError.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace {

class ExportExecutionBoundaryTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto pair = mcp::InMemoryTransport::CreatePair();
    server = mcp::McpServer::Create(pair.server);
    godot_autopilot::register_all_resources(*server, queue);
    godot_autopilot::register_debugger_resources(*server, queue);
    godot_autopilot::register_all_prompts(*server, queue);
    queue.drain();
    server_thread = std::thread([this] { server->Run(); });

    mcp::ClientOptions options;
    options.client_info = mcp::Implementation{"ExportBoundaryClient", "1.0.0"};
    options.connect_mode = mcp::ConnectMode::Auto;
    client = mcp::McpClient::Create(pair.client, options);
  }

  void TearDown() override {
    end_export();
    if (client) {
      client->Close();
    }
    if (server) {
      server->Close();
    }
    if (server_thread.joinable()) {
      server_thread.join();
    }
  }

  bool wait_for_queued_task() {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(2);
    while (queue.stats().pending == 0 &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return queue.stats().pending == 1u;
  }

  void start_export() {
    export_scope =
        std::make_unique<godot_autopilot::ExportState::Scope>(export_state);
  }

  void end_export() { export_scope.reset(); }

  godot_autopilot::CommandQueue queue;
  godot_autopilot::ExportState export_state;
  std::unique_ptr<godot_autopilot::ExportState::Scope> export_scope;
  std::unique_ptr<mcp::McpServer> server;
  std::unique_ptr<mcp::McpClient> client;
  std::thread server_thread;
};

TEST_F(ExportExecutionBoundaryTest,
       ResourceStartingExportAfterSubmissionIsRejectedAtFinalBoundary) {
  mcp::JsonValue params(mcp::JsonValue::object_tag);
  params["uri"] = mcp::JsonValue("godot://engine/version");
  std::promise<mcp::JsonValue> completed;
  auto completed_future = completed.get_future();
  std::thread worker([&] {
    try {
      completed.set_value(client->SendRequest("resources/read", params).get());
    } catch (...) {
      completed.set_exception(std::current_exception());
    }
  });

  const bool queued = wait_for_queued_task();
  start_export();
  queue.drain();
  end_export();

  std::string response;
  try {
    response = completed_future.get().Dump();
  } catch (const std::exception &error) {
    response = error.what();
  }
  worker.join();

  ASSERT_TRUE(queued);
  EXPECT_NE(response.find(godot_autopilot::dispatch::export_blocked_error_text()),
            std::string::npos);
}

TEST_F(ExportExecutionBoundaryTest,
       DebuggerResourceStartingExportAfterSubmissionIsRejectedAtFinalBoundary) {
  std::promise<mcp::ReadResourceResult> completed;
  auto completed_future = completed.get_future();
  std::thread worker([&] {
    try {
      completed.set_value(client->ReadResource("godot://editor/output-log"));
    } catch (...) {
      completed.set_exception(std::current_exception());
    }
  });

  const bool queued = wait_for_queued_task();
  start_export();
  queue.drain();
  end_export();

  try {
    completed_future.get();
    worker.join();
    FAIL() << "expected HandlerError, got a successful resource response";
  } catch (const mcp::McpError &error) {
    worker.join();
    EXPECT_EQ(error.Code(), mcp::McpErrorCode::HandlerError);
    EXPECT_EQ(error.ErrorMessage(),
              godot_autopilot::dispatch::export_blocked_error_text());
  } catch (...) {
    worker.join();
    FAIL() << "expected mcp::McpError";
  }

  ASSERT_TRUE(queued);
}

TEST_F(ExportExecutionBoundaryTest,
       DebuggerResourceExportInProgressRejectsAtEntryWithHandlerError) {
  start_export();

  try {
    client->ReadResource("godot://editor/output-log");
    end_export();
    FAIL() << "expected HandlerError, got a successful resource response";
  } catch (const mcp::McpError &error) {
    end_export();
    EXPECT_EQ(error.Code(), mcp::McpErrorCode::HandlerError);
    EXPECT_EQ(error.ErrorMessage(),
              godot_autopilot::dispatch::export_blocked_error_text());
  } catch (...) {
    end_export();
    FAIL() << "expected mcp::McpError";
  }
}

TEST_F(ExportExecutionBoundaryTest,
       PromptStartingExportAfterSubmissionIsRejectedAtFinalBoundary) {
  mcp::JsonValue params(mcp::JsonValue::object_tag);
  params["name"] = mcp::JsonValue("create-3d-scene");
  std::promise<mcp::JsonValue> completed;
  auto completed_future = completed.get_future();
  std::thread worker([&] {
    try {
      completed.set_value(client->SendRequest("prompts/get", params).get());
    } catch (...) {
      completed.set_exception(std::current_exception());
    }
  });

  const bool queued = wait_for_queued_task();
  start_export();
  queue.drain();
  end_export();

  std::string response;
  try {
    response = completed_future.get().Dump();
  } catch (const std::exception &error) {
    response = error.what();
  }
  worker.join();

  ASSERT_TRUE(queued);
  EXPECT_NE(response.find(godot_autopilot::dispatch::export_blocked_error_text()),
            std::string::npos);
}

} // namespace
