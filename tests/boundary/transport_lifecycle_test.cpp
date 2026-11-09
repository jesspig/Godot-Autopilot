#include "core/port_recovery.hpp"
#include "core/transport_lifecycle.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
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

InstanceRecord make_record(const std::string &instance_id, int port, int pid,
                            const std::string &project_path,
                            const std::string &token) {
  InstanceRecord record;
  record.instance_id = instance_id;
  record.port = port;
  record.pid = pid;
  record.project_path = project_path;
  record.api_token = token;
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
      make_record("gda-a", 9527, 111, "C:/proj", "token-a"),
      make_record("gda-b", 9528, 222, "C:/other", "token-b"),
  };
  const RecoveryPlan plan = godot_autopilot::port_recovery::analyze(
      records, "C:/proj", 999, [](int) { return false; });
  ASSERT_EQ(plan.stale.size(), 1u);
  EXPECT_EQ(plan.stale.front().instance_id, "gda-a");
  EXPECT_TRUE(plan.live.empty());
}

TEST(PortRecoveryTest, SplitsStaleAndLiveRegistrations) {
  const std::vector<InstanceRecord> records = {
      make_record("gda-dead", 9527, 111, "C:/proj", "token-dead"),
      make_record("gda-live", 9528, 222, "C:/proj", "token-live"),
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
      make_record("gda-self", 9527, 999, "C:/proj", "token-self"),
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
      make_record("gda-dead", 9527, 111, "C:/proj", "super-secret-token"),
      make_record("gda-live", 9528, 222, "C:/proj", "another-secret-token"),
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
