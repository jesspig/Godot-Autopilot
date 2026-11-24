#include "core/command_queue.hpp"
#include "core/server_context.hpp"
#include "tools/base/dispatch.hpp"
#include "core/transport/http_transport_ext.hpp"
#include "core/transport/ipc_transport.hpp"
#include "core/transport/ipc_waiters.hpp"
#include "core/transport/port_recovery.hpp"
#include "core/transport/transport_lifecycle.hpp"
#include "tools/base/register_all.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using godot_autopilot::InstanceRecord;
using godot_autopilot::port_recovery::RecoveryPlan;
using godot_autopilot::port_recovery::ResidualInstance;
using godot_autopilot::port_recovery::build_hint;
using godot_autopilot::port_recovery::describe;
using godot_autopilot::transport_lifecycle::ShutdownMode;
using godot_autopilot::transport_lifecycle::StopOutcome;
using godot_autopilot::transport_lifecycle::WorkerPhase;
using godot_autopilot::transport_lifecycle::count_reapable;
using godot_autopilot::transport_lifecycle::remaining_after_reap;
using godot_autopilot::transport_lifecycle::should_reap;
using godot_autopilot::transport_lifecycle::worker_accepts_job;
using godot_autopilot::transport_lifecycle::worker_budget_left;
using godot_autopilot::transport_lifecycle::worker_phase;
using godot_autopilot::transport_lifecycle::worker_stop_outcome;

namespace {

constexpr int64_t kGraceBudgetMs = 2000;

class ScopedEnv {
public:
  ScopedEnv(const char *name, const std::string &value) : name_(name) {
    const char *previous = std::getenv(name);
    if (previous != nullptr) {
      previous_ = previous;
      had_previous_ = true;
    }
    set(value);
  }
  ~ScopedEnv() {
    if (had_previous_) {
      set(previous_);
    } else {
#ifdef _WIN32
      _putenv_s(name_.c_str(), "");
#else
      unsetenv(name_.c_str());
#endif
    }
  }
  ScopedEnv(const ScopedEnv &) = delete;
  ScopedEnv &operator=(const ScopedEnv &) = delete;

private:
  void set(const std::string &value) {
#ifdef _WIN32
    _putenv_s(name_.c_str(), value.c_str());
#else
    setenv(name_.c_str(), value.c_str(), 1);
#endif
  }
  std::string name_;
  std::string previous_;
  bool had_previous_{false};
};

class ServerCreateRollbackContext : public godot_autopilot::ServerContext {
public:
  explicit ServerCreateRollbackContext(godot_autopilot::CommandQueue &queue)
      : ServerContext(queue) {}
  int create_calls() const { return create_calls_; }

protected:
  bool bind_transport() override { return true; }
  std::unique_ptr<mcp::McpServer> create_server(
      const std::shared_ptr<godot_autopilot::GodotTransport> &,
      const mcp::ServerOptions &) override {
    ++create_calls_;
    return nullptr;
  }

private:
  int create_calls_{0};
};

InstanceRecord make_record(const std::string &instance_id, int port, int pid,
                            const std::string &project_path) {
  InstanceRecord record;
  record.instance_id = instance_id;
  record.port = port;
  record.pid = pid;
  record.project_path = project_path;
  return record;
}

} // namespace

TEST(TransportLifecycleTest, PhaseTracksFinishedBeforeExecuting) {
  EXPECT_EQ(worker_phase(true, false), WorkerPhase::kFinished);
  EXPECT_EQ(worker_phase(true, true), WorkerPhase::kFinished);
  EXPECT_EQ(worker_phase(false, true), WorkerPhase::kExecuting);
  EXPECT_EQ(worker_phase(false, false), WorkerPhase::kRunning);
}

TEST(TransportLifecycleTest, StopFlagBlocksNewJobs) {
  EXPECT_TRUE(worker_accepts_job(false));
  EXPECT_FALSE(worker_accepts_job(true));
}

TEST(TransportLifecycleTest, BudgetComparisonIsExclusive) {
  EXPECT_TRUE(worker_budget_left(0, kGraceBudgetMs));
  EXPECT_TRUE(worker_budget_left(kGraceBudgetMs - 1, kGraceBudgetMs));
  EXPECT_FALSE(worker_budget_left(kGraceBudgetMs, kGraceBudgetMs));
  EXPECT_FALSE(worker_budget_left(kGraceBudgetMs + 1, kGraceBudgetMs));
}

TEST(TransportLifecycleTest, WorkerFinishedWithinBudgetIsJoined) {
  EXPECT_EQ(worker_stop_outcome(ShutdownMode::kGraceful, WorkerPhase::kFinished,
                                0, kGraceBudgetMs),
            StopOutcome::kJoin);
  EXPECT_EQ(worker_stop_outcome(ShutdownMode::kGraceful, WorkerPhase::kFinished,
                                kGraceBudgetMs, kGraceBudgetMs),
            StopOutcome::kJoin);
}

TEST(TransportLifecycleTest, WorkerStillRunningWithinBudgetKeepsWaiting) {
  EXPECT_EQ(worker_stop_outcome(ShutdownMode::kGraceful, WorkerPhase::kRunning,
                                0, kGraceBudgetMs),
            StopOutcome::kKeepWaiting);
  EXPECT_EQ(worker_stop_outcome(ShutdownMode::kGraceful, WorkerPhase::kExecuting,
                                kGraceBudgetMs - 1, kGraceBudgetMs),
            StopOutcome::kKeepWaiting);
}

TEST(TransportLifecycleTest, WorkerOverBudgetIsParkedOnGracefulShutdown) {
  EXPECT_EQ(worker_stop_outcome(ShutdownMode::kGraceful, WorkerPhase::kExecuting,
                                kGraceBudgetMs, kGraceBudgetMs),
            StopOutcome::kPark);
  EXPECT_EQ(worker_stop_outcome(ShutdownMode::kGraceful, WorkerPhase::kRunning,
                                kGraceBudgetMs * 3, kGraceBudgetMs),
            StopOutcome::kPark);
}

TEST(TransportLifecycleTest, FinalShutdownJoinsInsteadOfParking) {
  EXPECT_EQ(worker_stop_outcome(ShutdownMode::kFinal, WorkerPhase::kExecuting,
                                kGraceBudgetMs, kGraceBudgetMs),
            StopOutcome::kJoinBlocking);
  EXPECT_EQ(worker_stop_outcome(ShutdownMode::kFinal, WorkerPhase::kRunning,
                                kGraceBudgetMs * 3, kGraceBudgetMs),
            StopOutcome::kJoinBlocking);
}

TEST(TransportLifecycleTest, ReapOnlyAcceptsFinishedWorkers) {
  EXPECT_TRUE(should_reap(true));
  EXPECT_FALSE(should_reap(false));
}

TEST(TransportLifecycleTest, ParkedContainerClearsAfterReap) {
  const std::vector<bool> finished_flags = {true, true, false};
  EXPECT_EQ(count_reapable(finished_flags), 2u);
  EXPECT_EQ(remaining_after_reap(finished_flags.size(), 2u), 1u);
  EXPECT_EQ(remaining_after_reap(finished_flags.size(),
                                 count_reapable(finished_flags)),
            1u);
  const std::vector<bool> all_finished = {true, true, true};
  EXPECT_EQ(remaining_after_reap(all_finished.size(),
                                 count_reapable(all_finished)),
            0u);
}

TEST(TransportLifecycleTest, EmptyParkedContainerReapsToZero) {
  const std::vector<bool> finished_flags = {};
  EXPECT_EQ(count_reapable(finished_flags), 0u);
  EXPECT_EQ(remaining_after_reap(0u, 0u), 0u);
}

TEST(IpcWaiterSetTest, CancelRejectsNewWaitersAndJoinWaitsForCompletion) {
  godot_autopilot::IpcWaiterSet waiters;
  waiters.open();
  std::promise<void> release;
  auto release_future = release.get_future().share();
  std::atomic<bool> started{false};
  std::atomic<bool> observed_cancel{false};
  std::shared_ptr<std::atomic<bool>> cancellation;

  ASSERT_TRUE(waiters.spawn(
      [&](const godot_autopilot::IpcWaiterSet::CancellationToken &cancelled) {
        cancellation = cancelled;
        started.store(true, std::memory_order_release);
        release_future.wait();
        observed_cancel.store(cancelled->load(std::memory_order_acquire),
                              std::memory_order_release);
      }));
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(2);
  while (!started.load(std::memory_order_acquire) &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_TRUE(started.load(std::memory_order_acquire));
  ASSERT_NE(cancellation, nullptr);
  EXPECT_EQ(waiters.size(), 1u);

  waiters.cancel();
  EXPECT_TRUE(cancellation->load(std::memory_order_acquire));
  EXPECT_FALSE(waiters.spawn([](const auto &) {}));
  release.set_value();
  waiters.join();

  EXPECT_TRUE(observed_cancel.load(std::memory_order_acquire));
  EXPECT_EQ(waiters.size(), 0u);
}

TEST(ServerContextRollbackTest, CreateFailureRollsBackAndAllowsRetry) {
  const std::filesystem::path home =
      std::filesystem::temp_directory_path() /
      ("gda-t05-rollback-" +
       std::to_string(std::chrono::steady_clock::now()
                          .time_since_epoch()
                          .count()));
  std::filesystem::remove_all(home);
  ScopedEnv home_env("GODOT_AUTOPILOT_HOME", home.string());
  ScopedEnv multi_env("GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE", "1");
  ScopedEnv host_env("GODOT_AUTOPILOT_HOST", "127.0.0.1");
  ScopedEnv port_env("GODOT_AUTOPILOT_PORT", "9527");

  godot_autopilot::http_transport_instance().stop();
  godot_autopilot::ipc_transport::ipc_transport_instance().stop();
  godot_autopilot::godot_transport_reset();
  godot_autopilot::dispatch::clear_handlers();
  godot_autopilot::clear_active_registry();

  godot_autopilot::CommandQueue queue;
  ServerCreateRollbackContext context(queue);
  EXPECT_FALSE(context.start());
  EXPECT_EQ(context.create_calls(), 1);
  EXPECT_FALSE(context.is_running());
  EXPECT_NE(context.last_error().find("McpServer::Create returned null"),
            std::string::npos);
  EXPECT_FALSE(godot_autopilot::http_transport_is_listening());
  EXPECT_FALSE(
      godot_autopilot::ipc_transport::ipc_transport_instance().is_listening());
  EXPECT_TRUE(godot_autopilot::get_active_registry()->all_any().empty());

  EXPECT_FALSE(context.start());
  EXPECT_EQ(context.create_calls(), 2);
  EXPECT_FALSE(context.is_running());
  EXPECT_FALSE(godot_autopilot::http_transport_is_listening());
  EXPECT_FALSE(
      godot_autopilot::ipc_transport::ipc_transport_instance().is_listening());
  EXPECT_TRUE(godot_autopilot::get_active_registry()->all_any().empty());
  context.stop();
  godot_autopilot::godot_transport_reset();
  std::filesystem::remove_all(home);
}

TEST(PortRecoveryTest, SameProjectPathIgnoresSeparatorStyle) {
  EXPECT_TRUE(godot_autopilot::port_recovery::same_project_path(
      "C:/proj/app", "C:\\proj\\app"));
  EXPECT_TRUE(godot_autopilot::port_recovery::same_project_path(
      "C:/proj/app/", "C:/proj/app"));
  EXPECT_FALSE(godot_autopilot::port_recovery::same_project_path(
      "C:/proj/app", "C:/proj/other"));
  EXPECT_FALSE(godot_autopilot::port_recovery::same_project_path("", "C:/proj"));
}

TEST(PortRecoveryTest, KeepsOnlyRecordsOfTheSameProject) {
  const std::vector<InstanceRecord> records = {
      make_record("gda-a", 9527, 111, "C:/proj"),
      make_record("gda-b", 9528, 222, "C:/other"),
  };
  const RecoveryPlan plan = godot_autopilot::port_recovery::analyze(
      records, "C:/proj", 999, [](int) { return false; });
  ASSERT_EQ(plan.stale.size(), 1u);
  EXPECT_EQ(plan.stale.front().instance_id, "gda-a");
  EXPECT_TRUE(plan.live.empty());
}

TEST(PortRecoveryTest, SplitsStaleAndLiveRegistrations) {
  const std::vector<InstanceRecord> records = {
      make_record("gda-dead", 9527, 111, "C:/proj"),
      make_record("gda-live", 9528, 222, "C:/proj"),
  };
  const RecoveryPlan plan = godot_autopilot::port_recovery::analyze(
      records, "C:/proj", 999, [](int pid) { return pid == 222; });
  ASSERT_EQ(plan.stale.size(), 1u);
  EXPECT_EQ(plan.stale.front().instance_id, "gda-dead");
  ASSERT_EQ(plan.live.size(), 1u);
  EXPECT_EQ(plan.live.front().instance_id, "gda-live");
  EXPECT_TRUE(plan.live.front().alive);
}

TEST(PortRecoveryTest, SkipsTheCurrentProcessRecord) {
  const std::vector<InstanceRecord> records = {
      make_record("gda-self", 9527, 999, "C:/proj"),
  };
  const RecoveryPlan plan = godot_autopilot::port_recovery::analyze(
      records, "C:/proj", 999, [](int) { return true; });
  EXPECT_TRUE(plan.stale.empty());
  EXPECT_TRUE(plan.live.empty());
}

TEST(PortRecoveryTest, HintStaysEmptyWithoutResidualRecords) {
  const RecoveryPlan plan;
  EXPECT_TRUE(build_hint(plan, 9527).empty());
}

TEST(PortRecoveryTest, HintNamesResidualRecordsWithoutLeakingTokens) {
  const std::vector<InstanceRecord> records = {
      make_record("gda-dead", 9527, 111, "C:/proj"),
      make_record("gda-live", 9528, 222, "C:/proj"),
  };
  const RecoveryPlan plan = godot_autopilot::port_recovery::analyze(
      records, "C:/proj", 999, [](int pid) { return pid == 222; });
  const std::string hint = build_hint(plan, 9527);
  EXPECT_NE(hint.find("gda-dead"), std::string::npos);
  EXPECT_NE(hint.find("gda-live"), std::string::npos);
  EXPECT_NE(hint.find("9527"), std::string::npos);
  EXPECT_EQ(hint.find("super-secret-token"), std::string::npos);
  EXPECT_EQ(hint.find("another-secret-token"), std::string::npos);
}

TEST(PortRecoveryTest, DescribesResidualInstanceWithoutToken) {
  ResidualInstance residual;
  residual.instance_id = "gda-x";
  residual.pid = 42;
  residual.port = 9527;
  const std::string text = describe(residual);
  EXPECT_NE(text.find("gda-x"), std::string::npos);
  EXPECT_NE(text.find("42"), std::string::npos);
  EXPECT_NE(text.find("9527"), std::string::npos);
}
