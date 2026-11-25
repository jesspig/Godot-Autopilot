#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/protocol/gcp_envelope.hpp"
#include "support/gcp_contract_fixture.hpp"

namespace {

using namespace gda::gcp;

} // namespace

TEST(GcpContractTest, RequestResponseCorrelationAndStatusContract) {
  GcpEnvelope request = fixture::make_request("catalog.list", "req-1", "inst-1");
  ASSERT_TRUE(validate_envelope(request).empty());
  GcpEnvelope response = fixture::make_response(request, Status::ok, "resp-1");
  ASSERT_TRUE(validate_envelope(response).empty());
  EXPECT_EQ(response.request_id, request.id);
  EXPECT_EQ(response.instance_id, request.instance_id);
  EXPECT_EQ(response.method, request.method);
  EXPECT_EQ(response.v, request.v);
  EXPECT_EQ(response.status, Status::ok);
  EXPECT_STREQ(to_string(response.status), "ok");
  EXPECT_TRUE(is_terminal_status(response.status));
  GcpEnvelope pending = fixture::make_response(request, Status::pending, "resp-2");
  EXPECT_FALSE(is_terminal_status(pending.status));
  EXPECT_TRUE(validate_envelope(pending).empty());
}

TEST(GcpContractTest, ConfirmationRoundTripUsesChallengeDigest) {
  GcpEnvelope request = fixture::make_request("call.start", "req-2", "inst-2");
  GcpEnvelope challenge =
      fixture::make_response(request, Status::needs_confirmation, "resp-3");
  ASSERT_TRUE(validate_envelope(challenge).empty());
  EXPECT_EQ(challenge.status, Status::needs_confirmation);
  EXPECT_GT(challenge.confirmation.ttl_ms, 0);
  EXPECT_EQ(challenge.confirmation.param_digest, param_digest(request.params));
  EXPECT_TRUE(challenge_matches(challenge.confirmation, request.params));
  EXPECT_TRUE(challenge_fresh(
      challenge.confirmation, challenge.confirmation.issued_at_ms + 1));
  GcpEnvelope confirm = fixture::make_request("call.confirm", "req-3", "inst-2");
  ASSERT_TRUE(is_known_method(confirm.method));
  ASSERT_TRUE(validate_envelope(confirm).empty());
  GcpEnvelope confirmed = fixture::make_response(confirm, Status::ok, "resp-4");
  EXPECT_TRUE(validate_envelope(confirmed).empty());
  EXPECT_EQ(error_numeric(ErrorCode::confirmation_required), 13);
}

TEST(GcpContractTest, TaskLifecycleAndCancellationStagesAreMachineAssertable) {
  const std::vector<TaskState> visited = fixture::drive_task_lifecycle();
  ASSERT_FALSE(visited.empty());
  EXPECT_EQ(visited.front(), TaskState::pending);
  bool has_cancelled = false;
  bool has_succeeded = false;
  bool has_failed = false;
  for (TaskState state : visited) {
    has_cancelled = has_cancelled || state == TaskState::cancelled;
    has_succeeded = has_succeeded || state == TaskState::succeeded;
    has_failed = has_failed || state == TaskState::failed;
  }
  EXPECT_TRUE(has_cancelled);
  EXPECT_TRUE(has_succeeded);
  EXPECT_TRUE(has_failed);
  for (TaskState state : visited) {
    if (!is_terminal_task(state)) {
      continue;
    }
    for (int index = 0; index <= 6; ++index) {
      EXPECT_FALSE(can_transition(state, static_cast<TaskState>(index)));
    }
  }
  const std::vector<CancelStage> stages = fixture::drive_cancel_stages();
  ASSERT_EQ(stages.size(), 4u);
  for (std::size_t index = 1; index < stages.size(); ++index) {
    EXPECT_TRUE(can_advance(stages[index - 1], stages[index]));
  }
  EXPECT_STREQ(to_string(stages.back()), "completed");
}

TEST(GcpContractTest, EventStreamSeqMonotonicAcrossResume) {
  const std::vector<std::uint64_t> seq = fixture::drive_event_seq(1, 5);
  ASSERT_EQ(seq.size(), 5u);
  for (std::size_t index = 1; index < seq.size(); ++index) {
    EXPECT_GT(seq[index], seq[index - 1]);
  }
  GcpEnvelope event = fixture::make_event("inst-3", seq.back(), "task.updated");
  ASSERT_TRUE(validate_envelope(event).empty());
  GcpEnvelope task_event =
      fixture::make_event("inst-3", seq.back() + 1, "task.cancelled");
  task_event.task_id = "task-1";
  ASSERT_TRUE(validate_envelope(task_event).empty());
  EventCursor cursor{"inst-3", seq[2]};
  EXPECT_TRUE(fixture::cursor_valid_for(cursor, seq.back()));
  GcpEnvelope resumed =
      fixture::make_event("inst-3", seq.back() + 1, "events.resumed");
  EXPECT_EQ(resumed.seq, seq.back() + 1u);
  EXPECT_TRUE(validate_envelope(resumed).empty());
  EXPECT_TRUE(is_known_method("events.resume"));
  EXPECT_EQ(error_numeric(ErrorCode::cursor_expired), 10);
}

TEST(GcpContractTest, TracePropagatesAcrossEnvelopeKinds) {
  GcpEnvelope request = fixture::make_request("trace.get", "req-4", "inst-4");
  GcpEnvelope response = fixture::make_response(request, Status::ok, "resp-5");
  GcpEnvelope event = fixture::make_event("inst-4", 1, "trace.sampled");
  EXPECT_EQ(request.trace.traceparent, response.trace.traceparent);
  EXPECT_TRUE(valid_traceparent(request.trace.traceparent));
  EXPECT_TRUE(valid_traceparent(event.trace.traceparent));
  EXPECT_TRUE(is_known_method("trace.get"));
  EXPECT_TRUE(has_feature(default_capabilities(), "trace"));
}

TEST(GcpContractTest, BackpressureAndTaskNotFoundAreMapped) {
  EXPECT_EQ(classify_backpressure(9, 10, 20), Backpressure::none);
  EXPECT_EQ(classify_backpressure(10, 10, 20), Backpressure::soft);
  EXPECT_EQ(classify_backpressure(20, 10, 20), Backpressure::hard);
  EXPECT_EQ(error_numeric(ErrorCode::backpressure_exceeded), 15);
  EXPECT_EQ(error_numeric(ErrorCode::task_not_found), 14);
  GcpEnvelope request = fixture::make_request("task.get", "req-5", "inst-5");
  GcpEnvelope response = fixture::make_response(request, Status::error, "resp-6");
  EXPECT_EQ(response.error, ErrorCode::result_unknown);
  response.error = ErrorCode::task_not_found;
  EXPECT_STREQ(to_string(response.error), "task_not_found");
  ASSERT_TRUE(validate_envelope(response).empty());
}
