#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "core/protocol/gcp_envelope.hpp"
#include "support/gcp_contract_fixture.hpp"

namespace {

using namespace gda::gcp;

} // namespace

TEST(GcpEnvelopeTest, EnvelopeVersionAndKindMapping) {
  GcpEnvelope envelope;
  EXPECT_EQ(envelope.v, 1);
  EXPECT_EQ(envelope.kind, Kind::request);
  EXPECT_EQ(kEnvelopeVersion, 1);
  EXPECT_STREQ(to_string(Kind::request), "request");
  EXPECT_STREQ(to_string(Kind::response), "response");
  EXPECT_STREQ(to_string(Kind::event), "event");
  Kind kind = Kind::event;
  ASSERT_TRUE(parse_kind("request", kind));
  EXPECT_EQ(kind, Kind::request);
  ASSERT_TRUE(parse_kind("response", kind));
  EXPECT_EQ(kind, Kind::response);
  ASSERT_TRUE(parse_kind("event", kind));
  EXPECT_EQ(kind, Kind::event);
  EXPECT_FALSE(parse_kind("notification", kind));
}

TEST(GcpEnvelopeTest, ResponseStatusSetAndTerminality) {
  EXPECT_STREQ(to_string(Status::ok), "ok");
  EXPECT_STREQ(to_string(Status::error), "error");
  EXPECT_STREQ(to_string(Status::pending), "pending");
  EXPECT_STREQ(to_string(Status::needs_confirmation), "needs_confirmation");
  Status status = Status::pending;
  ASSERT_TRUE(parse_status("needs_confirmation", status));
  EXPECT_EQ(status, Status::needs_confirmation);
  EXPECT_FALSE(parse_status("rejected", status));
  EXPECT_TRUE(is_terminal_status(Status::ok));
  EXPECT_TRUE(is_terminal_status(Status::error));
  EXPECT_FALSE(is_terminal_status(Status::pending));
  EXPECT_FALSE(is_terminal_status(Status::needs_confirmation));
}

TEST(GcpEnvelopeTest, ErrorCodesE06ThroughE15) {
  const std::vector<ErrorEntry> &table = error_table();
  ASSERT_EQ(table.size(), 10u);
  const char *expected[] = {
      "unsupported_version",
      "auth_failed",
      "instance_mismatch",
      "result_unknown",
      "cursor_expired",
      "export_in_progress",
      "main_thread_timeout",
      "confirmation_required",
      "task_not_found",
      "backpressure_exceeded",
  };
  for (std::size_t index = 0; index < table.size(); ++index) {
    EXPECT_EQ(table[index].numeric, static_cast<int>(index + 6));
    EXPECT_STREQ(table[index].symbol, expected[index]);
    EXPECT_EQ(error_numeric(table[index].code), table[index].numeric);
    EXPECT_EQ(error_from_numeric(table[index].numeric), table[index].code);
    EXPECT_STREQ(to_string(table[index].code), table[index].symbol);
    ErrorCode parsed = ErrorCode::none;
    ASSERT_TRUE(parse_error_symbol(table[index].symbol, parsed));
    EXPECT_EQ(parsed, table[index].code);
  }
  EXPECT_EQ(error_numeric(ErrorCode::none), 0);
  EXPECT_EQ(error_from_numeric(5), ErrorCode::none);
  ErrorCode parsed = ErrorCode::none;
  EXPECT_FALSE(parse_error_symbol("unknown_code", parsed));
  EXPECT_EQ(parsed, ErrorCode::none);
}

TEST(GcpEnvelopeTest, MethodSetCoversAllContractFamilies) {
  const std::vector<std::string> &methods = all_methods();
  ASSERT_EQ(methods.size(), 14u);
  const char *expected[] = {
      "hello",
      "capabilities.get",
      "catalog.list",
      "catalog.search",
      "catalog.describe",
      "call.start",
      "call.get",
      "call.confirm",
      "task.list",
      "task.get",
      "task.cancel",
      "events.subscribe",
      "events.resume",
      "trace.get",
  };
  for (std::size_t index = 0; index < methods.size(); ++index) {
    EXPECT_EQ(methods[index], expected[index]);
    EXPECT_TRUE(is_known_method(expected[index]));
  }
  EXPECT_FALSE(is_known_method("tools.call"));
  EXPECT_FALSE(is_known_method(""));
}

TEST(GcpEnvelopeTest, CapabilityNegotiationIsIntersection) {
  CapabilitySet offered = default_capabilities();
  EXPECT_EQ(offered.protocol_version, 1);
  EXPECT_EQ(offered.methods.size(), 14u);
  EXPECT_TRUE(has_feature(offered, "confirmation"));
  EXPECT_TRUE(has_feature(offered, "tasks"));
  CapabilitySet requested;
  requested.protocol_version = 1;
  requested.methods = {"hello", "call.start", "unknown.method"};
  requested.features = {"trace", "unknown.feature"};
  CapabilitySet out;
  ASSERT_TRUE(negotiate_capabilities(offered, requested, out));
  EXPECT_EQ(out.protocol_version, 1);
  ASSERT_EQ(out.methods.size(), 2u);
  EXPECT_EQ(out.methods[0], "hello");
  EXPECT_EQ(out.methods[1], "call.start");
  ASSERT_EQ(out.features.size(), 1u);
  EXPECT_TRUE(has_feature(out, "trace"));
  EXPECT_FALSE(has_feature(out, "unknown.feature"));
  CapabilitySet unsupported;
  unsupported.protocol_version = 0;
  EXPECT_FALSE(negotiate_capabilities(unsupported, requested, out));
}

TEST(GcpEnvelopeTest, ChallengeCarriesTtlAndParamDigest) {
  Params params = {{"b", "2"}, {"a", "1"}};
  Params reordered = {{"a", "1"}, {"b", "2"}};
  EXPECT_EQ(param_digest(params), param_digest(reordered));
  EXPECT_EQ(param_digest(params).size(), 16u);
  Challenge challenge = make_challenge(params, 100, 1000, "nonce-1");
  EXPECT_EQ(challenge.ttl_ms, 100);
  EXPECT_EQ(challenge.issued_at_ms, 1000);
  EXPECT_EQ(challenge.nonce, "nonce-1");
  EXPECT_EQ(challenge.param_digest, param_digest(params));
  EXPECT_TRUE(challenge_matches(challenge, reordered));
  EXPECT_FALSE(challenge_matches(challenge, Params{{"a", "9"}}));
  EXPECT_TRUE(challenge_fresh(challenge, 1099));
  EXPECT_FALSE(challenge_fresh(challenge, 1100));
  EXPECT_FALSE(challenge_fresh(challenge, 999));
  const std::string *value = find_param(params, "a");
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, "1");
  EXPECT_EQ(find_param(params, "missing"), nullptr);
}

TEST(GcpEnvelopeTest, TaskStateMachineTransitions) {
  EXPECT_TRUE(can_transition(TaskState::pending, TaskState::running));
  EXPECT_TRUE(can_transition(TaskState::running, TaskState::cancelling));
  EXPECT_TRUE(can_transition(TaskState::cancelling, TaskState::cancelled));
  EXPECT_TRUE(can_transition(TaskState::blocked, TaskState::running));
  EXPECT_FALSE(can_transition(TaskState::pending, TaskState::succeeded));
  EXPECT_FALSE(can_transition(TaskState::cancelled, TaskState::running));
  EXPECT_FALSE(can_transition(TaskState::succeeded, TaskState::failed));
  EXPECT_TRUE(is_terminal_task(TaskState::cancelled));
  EXPECT_TRUE(is_terminal_task(TaskState::succeeded));
  EXPECT_TRUE(is_terminal_task(TaskState::failed));
  EXPECT_FALSE(is_terminal_task(TaskState::cancelling));
  EXPECT_STREQ(to_string(TaskState::cancelling), "cancelling");
}

TEST(GcpEnvelopeTest, CancelStagesAdvanceMonotonically) {
  EXPECT_TRUE(can_advance(CancelStage::requested, CancelStage::acknowledged));
  EXPECT_TRUE(can_advance(CancelStage::acknowledged, CancelStage::draining));
  EXPECT_TRUE(can_advance(CancelStage::draining, CancelStage::completed));
  EXPECT_FALSE(can_advance(CancelStage::requested, CancelStage::draining));
  EXPECT_FALSE(can_advance(CancelStage::acknowledged, CancelStage::completed));
  EXPECT_TRUE(can_advance(CancelStage::completed, CancelStage::completed));
  EXPECT_FALSE(can_advance(CancelStage::completed, CancelStage::requested));
  EXPECT_STREQ(to_string(CancelStage::draining), "draining");
}

TEST(GcpEnvelopeTest, EventSeqCursorAndBackpressure) {
  EXPECT_EQ(classify_backpressure(5, 10, 20), Backpressure::none);
  EXPECT_EQ(classify_backpressure(10, 10, 20), Backpressure::soft);
  EXPECT_EQ(classify_backpressure(20, 10, 20), Backpressure::hard);
  EXPECT_EQ(classify_backpressure(3, 0, 0), Backpressure::none);
  EXPECT_STREQ(to_string(Backpressure::hard), "hard");
  EventCursor cursor{"instance-a", 7};
  EXPECT_TRUE(fixture::cursor_valid_for(cursor, 7));
  EXPECT_TRUE(fixture::cursor_valid_for(cursor, 8));
  EXPECT_FALSE(fixture::cursor_valid_for(cursor, 6));
  EXPECT_FALSE(fixture::cursor_valid_for(EventCursor{"", 7}, 8));
}

TEST(GcpEnvelopeTest, TraceParentFormatValidation) {
  const std::string valid =
      "00-" + std::string(32, 'a') + "-" + std::string(16, 'b') + "-01";
  ASSERT_EQ(valid.size(), kTraceparentLength);
  EXPECT_TRUE(valid_traceparent(valid));
  EXPECT_TRUE(valid_traceparent(fixture::make_traceparent(9)));
  EXPECT_FALSE(valid_traceparent(valid.substr(0, valid.size() - 1)));
  EXPECT_FALSE(valid_traceparent(
      "00-" + std::string(32, 'a') + "-" + std::string(16, 'b') + "-0z"));
  EXPECT_FALSE(valid_traceparent(std::string(kTraceparentLength, '0')));
}

TEST(GcpEnvelopeTest, ValidationByKind) {
  GcpEnvelope request;
  request.kind = Kind::request;
  request.id = "req-1";
  request.instance_id = "inst-1";
  request.method = "call.start";
  request.trace.traceparent = fixture::make_traceparent(1);
  EXPECT_TRUE(validate_envelope(request).empty());

  GcpEnvelope wrong_version = request;
  wrong_version.v = 2;
  EXPECT_FALSE(validate_envelope(wrong_version).empty());
  GcpEnvelope bad_deadline = request;
  bad_deadline.deadline_ms = 0;
  EXPECT_FALSE(validate_envelope(bad_deadline).empty());
  GcpEnvelope bad_method = request;
  bad_method.method = "tools.call";
  EXPECT_FALSE(validate_envelope(bad_method).empty());

  GcpEnvelope response = request;
  response.kind = Kind::response;
  response.id = "resp-1";
  response.request_id = "req-1";
  response.status = Status::ok;
  EXPECT_TRUE(validate_envelope(response).empty());
  GcpEnvelope missing_correlation = response;
  missing_correlation.request_id.reset();
  EXPECT_FALSE(validate_envelope(missing_correlation).empty());
  GcpEnvelope error_response = response;
  error_response.status = Status::error;
  EXPECT_FALSE(validate_envelope(error_response).empty());
  error_response.error = ErrorCode::result_unknown;
  error_response.error_message = "result_unknown";
  EXPECT_TRUE(validate_envelope(error_response).empty());
  GcpEnvelope confirm_response = response;
  confirm_response.status = Status::needs_confirmation;
  confirm_response.confirmation =
      make_challenge(request.params, 1000, 0, "nonce");
  EXPECT_TRUE(validate_envelope(confirm_response).empty());

  GcpEnvelope event;
  event.kind = Kind::event;
  event.id = "evt-1";
  event.instance_id = "inst-1";
  event.seq = 1;
  event.event_type = "task.updated";
  event.trace.traceparent = fixture::make_traceparent(3);
  EXPECT_TRUE(validate_envelope(event).empty());
  GcpEnvelope bad_seq = event;
  bad_seq.seq = 0;
  EXPECT_FALSE(validate_envelope(bad_seq).empty());
  GcpEnvelope bad_task_id = event;
  bad_task_id.task_id = std::string();
  EXPECT_FALSE(validate_envelope(bad_task_id).empty());
}
