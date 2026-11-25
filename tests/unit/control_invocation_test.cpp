#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/control/catalog_service.hpp"
#include "core/control/confirmation_service.hpp"
#include "core/control/control_types.hpp"
#include "core/control/invocation_service.hpp"
#include "core/control/job_service.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "support/control_fixture.hpp"

namespace {

using namespace gda::control;
using namespace gda::control::fixture;
namespace gcp = gda::gcp;

TEST(ControlInvocationTest, NormalizeParamsAndAliasResolution) {
  FixedClock clock(1000);
  StaticExportProbe probe;
  CatalogService catalog = fixture::sample_catalog();
  FakeSink sink;
  sink.result.status = gcp::Status::ok;
  sink.result.payload = "value";
  InvocationService service(
      InvocationDeps{&clock, &probe, nullptr, &sink, nullptr, nullptr,
                     &catalog, nullptr, true, 60000});
  InvocationRequest request = fixture::make_request("r1", "inspect_scene");
  request.params = gcp::Params{{"z", "1"},
                               {"a", "2"},
                               {"a", "1"},
                               {"confirm", "1"}};
  const InvocationResult result = service.invoke(request);
  EXPECT_EQ(result.status, gcp::Status::ok);
  EXPECT_EQ(sink.calls, 1);
  EXPECT_EQ(sink.last.tool, "scene.inspect");
  ASSERT_EQ(sink.last.params.size(), 3u);
  EXPECT_EQ(sink.last.params[0].key, "a");
  EXPECT_EQ(sink.last.params[0].value, "2");
  EXPECT_EQ(sink.last.params[1].key, "confirm");
  EXPECT_EQ(sink.last.params[1].value, "true");
  EXPECT_EQ(sink.last.params[2].key, "z");
  EXPECT_EQ(result.artifact.digest, fnv1a_hex("value"));
}

TEST(ControlInvocationTest, ExportTimeRejectsBeforeDispatch) {
  FixedClock clock(1000);
  StaticExportProbe probe;
  probe.set(true);
  FakeSink sink;
  InvocationService service(
      InvocationDeps{&clock, &probe, nullptr, &sink, nullptr, nullptr,
                     nullptr, nullptr, true, 60000});
  const InvocationResult result =
      service.invoke(fixture::make_request("r", "scene.inspect"));
  EXPECT_EQ(result.status, gcp::Status::error);
  EXPECT_EQ(result.error, gcp::ErrorCode::export_in_progress);
  EXPECT_EQ(sink.calls, 0);
  EXPECT_EQ(service.consume_new_errors("r"), 1);
  EXPECT_EQ(service.consume_new_errors("r"), 0);
}

TEST(ControlInvocationTest, HighRiskRequiresConfirmationAndConfirmDispatches) {
  FixedClock clock(1000);
  StaticExportProbe probe;
  StaticRiskProbe risk;
  risk.add("project.open");
  ConfirmationService confirmation;
  CatalogService catalog = fixture::sample_catalog();
  FakeSink sink;
  sink.result.status = gcp::Status::ok;
  sink.result.payload = "ok";
  InvocationService service(
      InvocationDeps{&clock, &probe, &risk, &sink, nullptr, &confirmation,
                     &catalog, nullptr, true, 60000});
  const InvocationRequest request =
      fixture::make_request("r2", "project.open");
  const InvocationResult challenged = service.invoke(request);
  EXPECT_EQ(challenged.status, gcp::Status::needs_confirmation);
  EXPECT_EQ(challenged.error, gcp::ErrorCode::confirmation_required);
  EXPECT_EQ(sink.calls, 0);
  EXPECT_EQ(challenged.confirmation.param_digest,
            gcp::param_digest(normalize_params(request.params)));
  EXPECT_TRUE(confirmation.has_pending("r2"));

  const InvocationResult accepted =
      service.confirm("r2", request, request.params);
  EXPECT_EQ(accepted.status, gcp::Status::ok);
  EXPECT_EQ(sink.calls, 1);
  EXPECT_EQ(sink.last.tool, "project.open");
  const InvocationResult replay =
      service.confirm("r2", request, request.params);
  EXPECT_EQ(replay.status, gcp::Status::error);
  EXPECT_EQ(replay.error, gcp::ErrorCode::confirmation_required);
  EXPECT_EQ(sink.calls, 1);
}

TEST(ControlInvocationTest, DeferredWaitAndDesensitizedArtifact) {
  FixedClock clock(1000);
  StaticExportProbe probe;
  FakeSink sink;
  sink.result.status = gcp::Status::pending;
  sink.result.task_id = "task-1";
  FakeWaiter waiter;
  waiter.completed = true;
  InvocationResult completed;
  completed.status = gcp::Status::ok;
  completed.task_id = "task-1";
  completed.payload = "token=abc123 done";
  waiter.result = completed;
  JobService jobs;
  InvocationService service(
      InvocationDeps{&clock, &probe, nullptr, &sink, &waiter, nullptr,
                     nullptr, &jobs, true, 60000});
  InvocationRequest request = fixture::make_request("r3", "task.run");
  request.deferred = true;
  const InvocationResult result = service.invoke(request);
  EXPECT_EQ(result.status, gcp::Status::ok);
  EXPECT_EQ(waiter.calls, 1);
  EXPECT_EQ(waiter.last_task_id, "task-1");
  EXPECT_TRUE(jobs.exists(TaskKey{"inst-1", 1, "task-1"}));
  EXPECT_EQ(result.payload, "token=[redacted] done");
  EXPECT_EQ(result.artifact.digest, fnv1a_hex("token=[redacted] done"));
  EXPECT_EQ(result.artifact.id, "task-1");
  EXPECT_EQ(service.consume_new_errors("r3"), 0);
}

TEST(ControlInvocationTest, CancelAndErrorWatermark) {
  FixedClock clock(1000);
  StaticExportProbe probe;
  JobService jobs;
  FakeSink sink;
  InvocationService service(
      InvocationDeps{&clock, &probe, nullptr, &sink, nullptr, nullptr,
                     nullptr, &jobs, true, 60000});
  const InvocationResult missing =
      service.cancel("inst-1", 1, "task-9");
  EXPECT_EQ(missing.status, gcp::Status::error);
  EXPECT_EQ(missing.error, gcp::ErrorCode::task_not_found);
  EXPECT_EQ(service.consume_new_errors("task-9"), 1);
  jobs.create("inst-1", 1, "task-9", 0, 1000);
  const InvocationResult cancelled =
      service.cancel("inst-1", 1, "task-9");
  EXPECT_EQ(cancelled.status, gcp::Status::ok);
  const std::optional<TaskRecord> record =
      jobs.get(TaskKey{"inst-1", 1, "task-9"});
  ASSERT_TRUE(record.has_value());
  EXPECT_EQ(record->state, gcp::TaskState::cancelling);
  EXPECT_EQ(record->cancel_stage, gcp::CancelStage::requested);
}

} // namespace
