#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/command_queue.hpp"
#include "tools/base/debugger_access.hpp"
#include "tools/base/runtime_ops.hpp"

namespace {

using godot_autopilot::DebuggerRequestBackend;
using godot_autopilot::DebuggerSessionSnapshot;
using godot_autopilot::runtime_ops::GameJobTable;
using godot_autopilot::runtime_ops::game_jobs;
using godot_autopilot::runtime_ops::game_jobs_mutex;
using godot_autopilot::runtime_ops::handle_game_job_start;
using godot_autopilot::runtime_ops::handle_game_status;
using godot_autopilot::runtime_ops::handle_gda_send;
using godot_autopilot::runtime_ops::on_session_ready;
using godot_autopilot::runtime_ops::set_editor_queue;
using godot_autopilot::runtime_ops::wait_pending_response;

struct SentRequest {
  int32_t session_id = -1;
  std::string payload;
};

struct SentCancel {
  int32_t session_id = -1;
  int64_t request_id = 0;
};

class FakeDebuggerBackend : public DebuggerRequestBackend {
public:
  std::vector<DebuggerSessionSnapshot> sessions() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_;
  }

  bool send_request(int32_t session_id,
                    const std::string &payload) const override {
    std::lock_guard<std::mutex> lock(mutex_);
    ++attempts_;
    if (!send_ok_) {
      return false;
    }
    sent_.push_back({session_id, payload});
    return true;
  }

  void send_cancel(int32_t session_id, int64_t request_id) const override {
    std::lock_guard<std::mutex> lock(mutex_);
    cancels_.push_back({session_id, request_id});
  }

  void set_sessions(std::vector<DebuggerSessionSnapshot> sessions) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_ = std::move(sessions);
  }

  void set_send_ok(bool send_ok) {
    std::lock_guard<std::mutex> lock(mutex_);
    send_ok_ = send_ok;
  }

  std::vector<SentRequest> sent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sent_;
  }

  std::vector<SentCancel> cancels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancels_;
  }

  int attempts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attempts_;
  }

private:
  mutable std::mutex mutex_;
  std::vector<DebuggerSessionSnapshot> sessions_;
  mutable std::vector<SentRequest> sent_;
  mutable std::vector<SentCancel> cancels_;
  bool send_ok_ = true;
  mutable int attempts_ = 0;
};

class GameSessionDispatchFixture : public ::testing::Test {
protected:
  void SetUp() override {
    {
      std::lock_guard<std::mutex> lock(game_jobs_mutex());
      game_jobs().jobs.clear();
      game_jobs().next_id = 1;
    }
    backend_ = std::make_shared<FakeDebuggerBackend>();
    godot_autopilot::set_debugger_request_backend(backend_);
    queue_.drain();
    set_editor_queue(&queue_);
  }

  void TearDown() override {
    set_editor_queue(nullptr);
    godot_autopilot::clear_debugger_request_backend();
    {
      std::lock_guard<std::mutex> lock(game_jobs_mutex());
      game_jobs().jobs.clear();
    }
  }

  int64_t pending_id(const mcp::JsonValue &result) const {
    const auto *value = result.Find("__gda_pending");
    return value != nullptr && value->IsInt() ? value->GetInt() : 0;
  }

  mcp::JsonValue start_job_args() const {
    mcp::JsonValue params = mcp::JsonValue::Parse(
        R"({"action":"get_property","property":"position"})");
    mcp::JsonValue args = mcp::JsonValue::Parse(
        R"({"op":"eval","session_id":1,"timeout_ms":1000})");
    args["params"] = std::move(params);
    return args;
  }

  godot_autopilot::CommandQueue queue_;
  std::shared_ptr<FakeDebuggerBackend> backend_;
};

} // namespace

TEST_F(GameSessionDispatchFixture, SingleReadySessionIsAutoSelected) {
  backend_->set_sessions({{7, true, true, true}});
  const mcp::JsonValue result = handle_gda_send(
      "status", mcp::JsonValue(mcp::JsonValue::object_tag), 1000);
  const int64_t request_id = pending_id(result);
  ASSERT_GT(request_id, 0);
  const auto sent = backend_->sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].session_id, 7);
  std::string detail;
  godot_autopilot::runtime_ops::discard_pending(request_id, detail);
}

TEST_F(GameSessionDispatchFixture, MultipleReadySessionsRejectBeforeAnySend) {
  backend_->set_sessions({{7, true, true, true}, {8, true, true, true}});
  const mcp::JsonValue result = handle_gda_send(
      "status", mcp::JsonValue(mcp::JsonValue::object_tag), 1000);
  ASSERT_NE(result.Find("error"), nullptr);
  ASSERT_NE(result.Find("code"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "ambiguous_session");
  EXPECT_TRUE(backend_->sent().empty());
  EXPECT_EQ(backend_->attempts(), 0);
}

TEST_F(GameSessionDispatchFixture, ExplicitSessionTargetsOnlyRequestedSession) {
  backend_->set_sessions({{7, true, true, true}, {8, true, true, true}});
  const mcp::JsonValue result = handle_gda_send(
      "status", mcp::JsonValue(mcp::JsonValue::object_tag), 1000, false, true, 8);
  const int64_t request_id = pending_id(result);
  ASSERT_GT(request_id, 0);
  const auto sent = backend_->sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].session_id, 8);
  std::string detail;
  godot_autopilot::runtime_ops::discard_pending(request_id, detail);
}

TEST_F(GameSessionDispatchFixture, InvalidSessionIsRejectedBeforeSend) {
  backend_->set_sessions({{7, true, true, true}});
  const mcp::JsonValue result = handle_gda_send(
      "status", mcp::JsonValue(mcp::JsonValue::object_tag), 1000, false, true, 99);
  ASSERT_NE(result.Find("code"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "invalid_session");
  EXPECT_TRUE(backend_->sent().empty());
}

TEST_F(GameSessionDispatchFixture, GameToolRejectsNonIntegerSessionId) {
  backend_->set_sessions({{7, true, true, true}});
  const mcp::JsonValue result = handle_game_status(
      mcp::JsonValue::Parse(R"({"session_id":"7"})"));
  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_EQ(result["error"].GetString(), "session_id must be an integer");
  EXPECT_TRUE(backend_->sent().empty());
}

TEST_F(GameSessionDispatchFixture, TimeoutCancelsOnlyTheSelectedSession) {
  backend_->set_sessions({{3, true, true, true}, {4, true, true, true}});
  const mcp::JsonValue result = handle_gda_send(
      "status", mcp::JsonValue(mcp::JsonValue::object_tag), 1000, false, true, 3);
  const int64_t request_id = pending_id(result);
  ASSERT_GT(request_id, 0);
  const mcp::JsonValue timeout = wait_pending_response(request_id, 1);
  ASSERT_NE(timeout.Find("error"), nullptr);
  const auto cancels = backend_->cancels();
  ASSERT_EQ(cancels.size(), 1u);
  EXPECT_EQ(cancels[0].session_id, 3);
  EXPECT_EQ(cancels[0].request_id, request_id);
}

TEST_F(GameSessionDispatchFixture, QueuedExplicitRequestWaitsForTargetReady) {
  backend_->set_sessions({{1, true, false, false}, {2, true, true, true}});
  const mcp::JsonValue result = handle_gda_send(
      "status", mcp::JsonValue(mcp::JsonValue::object_tag), 1000, false, true, 1);
  const int64_t request_id = pending_id(result);
  ASSERT_GT(request_id, 0);
  EXPECT_TRUE(backend_->sent().empty());

  on_session_ready(2);
  EXPECT_TRUE(backend_->sent().empty());

  backend_->set_sessions({{1, true, true, true}, {2, true, true, true}});
  on_session_ready(1);
  const auto sent = backend_->sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].session_id, 1);
  std::string detail;
  godot_autopilot::runtime_ops::discard_pending(request_id, detail);
}

TEST_F(GameSessionDispatchFixture, QueuedImplicitRequestFailsIfMultipleBecomeReady) {
  backend_->set_sessions({{1, true, false, false}, {2, true, false, false}});
  const mcp::JsonValue result = handle_gda_send(
      "status", mcp::JsonValue(mcp::JsonValue::object_tag), 1000);
  const int64_t request_id = pending_id(result);
  ASSERT_GT(request_id, 0);
  backend_->set_sessions({{1, true, true, true}, {2, true, true, true}});
  on_session_ready(1);
  EXPECT_TRUE(backend_->sent().empty());
  const mcp::JsonValue response = wait_pending_response(request_id, 10);
  ASSERT_NE(response.Find("code"), nullptr);
  EXPECT_EQ(response["code"].GetString(), "ambiguous_session");
}

TEST_F(GameSessionDispatchFixture, FullJobTableRejectsBeforeSend) {
  const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  {
    std::lock_guard<std::mutex> lock(game_jobs_mutex());
    for (size_t i = 0; i < GameJobTable::kMaxJobs; ++i) {
      ASSERT_GE(game_jobs().register_job(1000 + static_cast<int64_t>(i),
                                         "eval", 25000, now_ms), 0);
    }
  }
  backend_->set_sessions({{1, true, true, true}});
  const mcp::JsonValue result = handle_game_job_start(start_job_args());
  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_NE(result["error"].GetString().find("job table is full"),
            std::string::npos);
  EXPECT_TRUE(backend_->sent().empty());
  EXPECT_EQ(backend_->attempts(), 0);
}

TEST_F(GameSessionDispatchFixture, SendFailureReleasesReservedJobSlot) {
  backend_->set_sessions({{1, true, true, true}});
  backend_->set_send_ok(false);
  const mcp::JsonValue result = handle_game_job_start(start_job_args());
  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "send_failed");
  EXPECT_EQ(backend_->attempts(), 1);
  std::lock_guard<std::mutex> lock(game_jobs_mutex());
  EXPECT_TRUE(game_jobs().jobs.empty());
}

TEST_F(GameSessionDispatchFixture, SuccessfulSendBindsReservedJobToRequest) {
  backend_->set_sessions({{1, true, true, true}});
  const mcp::JsonValue result = handle_game_job_start(start_job_args());
  ASSERT_EQ(result.Find("error"), nullptr);
  ASSERT_NE(result.Find("result"), nullptr);
  const int64_t job_id = result["result"]["job_id"].GetInt();
  const int64_t request_id = result["result"]["request_id"].GetInt();
  ASSERT_GT(job_id, 0);
  ASSERT_GT(request_id, 0);
  const auto sent = backend_->sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_NE(sent[0].payload.find(std::to_string(request_id)), std::string::npos);
  {
    std::lock_guard<std::mutex> lock(game_jobs_mutex());
    ASSERT_NE(game_jobs().jobs.find(job_id), game_jobs().jobs.end());
    EXPECT_EQ(game_jobs().jobs.at(job_id).request_id, request_id);
  }
  std::string detail;
  godot_autopilot::runtime_ops::discard_pending(request_id, detail);
  std::lock_guard<std::mutex> lock(game_jobs_mutex());
  EXPECT_TRUE(game_jobs().release_job(job_id));
}
