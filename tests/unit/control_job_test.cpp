#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/control/job_service.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace {

using namespace gda::control;
namespace gcp = gda::gcp;

TEST(ControlJobTest, CreateGetAndGenerationIsolation) {
  JobService service(1000);
  const TaskRecord record = service.create("inst-1", 1, "task-1", 100, 1000);
  EXPECT_EQ(record.state, gcp::TaskState::pending);
  EXPECT_EQ(record.ttl_ms, 100);
  EXPECT_EQ(record.created_at_ms, 1000);

  const TaskKey key{"inst-1", 1, "task-1"};
  ASSERT_TRUE(service.exists(key));
  const std::optional<TaskRecord> fetched = service.get(key);
  ASSERT_TRUE(fetched.has_value());
  EXPECT_EQ(fetched->key.task_id, "task-1");

  const TaskKey stale{"inst-1", 0, "task-1"};
  EXPECT_FALSE(service.exists(stale));
  EXPECT_FALSE(service.get(stale).has_value());
  const TaskKey wrong_instance{"inst-2", 1, "task-1"};
  EXPECT_FALSE(service.exists(wrong_instance));

  service.set_generation("inst-1", 2);
  EXPECT_EQ(service.generation("inst-1"), 2);
  EXPECT_EQ(service.generation("other"), 0);
  EXPECT_EQ(service.list("inst-1", 1).size(), 1u);
  EXPECT_TRUE(service.list("inst-1", 2).empty());
}

TEST(ControlJobTest, ProgressResultAndTerminalGuard) {
  JobService service;
  const std::string id = service.make_task_id();
  const TaskKey key{"inst-1", 1, id};
  service.create("inst-1", 1, id, 0, 0);
  EXPECT_TRUE(service.set_progress(key, 250, "working", 10));
  const std::optional<TaskRecord> progressed = service.get(key);
  ASSERT_TRUE(progressed.has_value());
  EXPECT_EQ(progressed->progress, 100);
  EXPECT_EQ(progressed->progress_message, "working");
  EXPECT_TRUE(service.advance(key, gcp::TaskState::running, 20));
  EXPECT_TRUE(service.set_result(key, "done", 30));
  const std::optional<TaskRecord> finished = service.get(key);
  ASSERT_TRUE(finished.has_value());
  EXPECT_EQ(finished->state, gcp::TaskState::succeeded);
  EXPECT_EQ(finished->result, "done");
  EXPECT_FALSE(service.set_progress(key, 5, "late", 40));
  EXPECT_FALSE(service.advance(key, gcp::TaskState::running, 40));
  EXPECT_FALSE(service.fail(key, gcp::ErrorCode::result_unknown, "late", 40));

  const std::string failed_id = service.make_task_id();
  const TaskKey failed_key{"inst-1", 1, failed_id};
  service.create("inst-1", 1, failed_id, 0, 0);
  EXPECT_TRUE(service.fail(failed_key, gcp::ErrorCode::result_unknown, "boom", 5));
  const std::optional<TaskRecord> failed = service.get(failed_key);
  ASSERT_TRUE(failed.has_value());
  EXPECT_EQ(failed->state, gcp::TaskState::failed);
  EXPECT_EQ(failed->error, gcp::ErrorCode::result_unknown);
}

TEST(ControlJobTest, CancellationStagesAdvanceMonotonically) {
  JobService service;
  const std::string id = service.make_task_id();
  const TaskKey key{"inst-1", 1, id};
  service.create("inst-1", 1, id, 0, 0);
  EXPECT_TRUE(service.advance(key, gcp::TaskState::running, 10));
  EXPECT_TRUE(service.request_cancel(key, 11));
  EXPECT_EQ(service.get(key)->state, gcp::TaskState::cancelling);
  EXPECT_EQ(service.get(key)->cancel_stage, gcp::CancelStage::requested);
  EXPECT_FALSE(service.advance_cancel(key, gcp::CancelStage::draining, 12));
  EXPECT_TRUE(
      service.advance_cancel(key, gcp::CancelStage::acknowledged, 12));
  EXPECT_TRUE(service.advance_cancel(key, gcp::CancelStage::draining, 13));
  EXPECT_TRUE(service.advance_cancel(key, gcp::CancelStage::completed, 14));
  EXPECT_EQ(service.get(key)->state, gcp::TaskState::cancelled);
  EXPECT_FALSE(service.request_cancel(key, 15));
}

TEST(ControlJobTest, TtlExpiryAndSnapshotRestore) {
  JobService source;
  const std::string id = source.make_task_id();
  const TaskKey key{"inst-1", 1, id};
  source.create("inst-1", 1, id, 100, 1000);
  EXPECT_TRUE(source.advance(key, gcp::TaskState::running, 1010));
  EXPECT_TRUE(source.set_progress(key, 40, "half", 1015));
  const std::string text = source.snapshot();
  EXPECT_FALSE(text.empty());

  JobService restored;
  ASSERT_TRUE(restored.restore(text));
  const std::optional<TaskRecord> recovered = restored.get(key);
  ASSERT_TRUE(recovered.has_value());
  EXPECT_EQ(recovered->state, gcp::TaskState::running);
  EXPECT_EQ(recovered->progress, 40);
  EXPECT_EQ(recovered->progress_message, "half");
  EXPECT_TRUE(restored.fail(key, gcp::ErrorCode::result_unknown, "boom", 1020));
  EXPECT_EQ(restored.expire(1119), 0u);
  EXPECT_EQ(restored.expire(1120), 1u);
  EXPECT_EQ(restored.size(), 0u);
  EXPECT_FALSE(restored.restore("broken\nline"));
}

} // namespace
