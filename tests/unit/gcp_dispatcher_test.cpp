#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/control/catalog_service.hpp"
#include "core/control/control_service.hpp"
#include "core/control/gcp_dispatcher.hpp"
#include "core/control/invocation_service.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "support/gcp_dispatcher_fixture.hpp"

namespace {

using namespace gda;
using namespace gda::control;
using namespace gda::control::fixture;

} // namespace

TEST(GcpDispatcherTest, EncodeDecodeRoundTripPreservesEnvelope) {
  gcp::GcpEnvelope request;
  request.v = gcp::kEnvelopeVersion;
  request.kind = gcp::Kind::request;
  request.id = "req-1";
  request.instance_id = "inst-1";
  request.method = "catalog.list";
  request.params = {{"category", "scene"}, {"limit", "5"}};
  request.trace.traceparent = fixture::make_traceparent(3);
  request.deadline_ms = gcp::kDefaultDeadlineMs;

  std::string text;
  ASSERT_TRUE(GcpDispatcher::encode(request, text));
  gcp::GcpEnvelope decoded;
  std::string error;
  ASSERT_TRUE(GcpDispatcher::decode(text, decoded, error)) << error;
  EXPECT_EQ(decoded.kind, gcp::Kind::request);
  EXPECT_EQ(decoded.id, "req-1");
  EXPECT_EQ(decoded.instance_id, "inst-1");
  EXPECT_EQ(decoded.method, "catalog.list");
  ASSERT_EQ(decoded.params.size(), 2u);
  ASSERT_NE(gcp::find_param(decoded.params, "category"), nullptr);
  EXPECT_EQ(*gcp::find_param(decoded.params, "category"), "scene");
  EXPECT_EQ(decoded.trace.traceparent, request.trace.traceparent);
  EXPECT_TRUE(gcp::validate_envelope(decoded).empty());
}

TEST(GcpDispatcherTest, HelloAndCapabilitiesAreServedByControlService) {
  ControlService service;
  GcpDispatcher dispatcher(service, "inst-1");

  const DispatchResult hello =
      dispatcher.handle_frame(7, fixture::make_request_json("hello", "h1", "inst-1"));
  bool ok = false;
  const gcp::GcpEnvelope response = fixture::parse_response(hello.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(hello.state, DispatchState::terminal);
  EXPECT_EQ(response.kind, gcp::Kind::response);
  ASSERT_TRUE(response.request_id.has_value());
  EXPECT_EQ(*response.request_id, "h1");
  EXPECT_EQ(response.status, gcp::Status::ok);
  const std::string *body = gcp::find_param(response.params, "result");
  ASSERT_NE(body, nullptr);
  EXPECT_NE(body->find("capabilities"), std::string::npos);
  EXPECT_NE(body->find("catalog.describe"), std::string::npos);

  const gcp::Params requested = {{"methods", "hello,catalog.list,unknown"},
                                 {"features", "catalog,trace,unknown"},
                                 {"protocol_version", "1"}};
  const DispatchResult caps = dispatcher.handle_frame(
      7, fixture::make_request_json("capabilities.get", "c1", "inst-1", requested));
  const gcp::GcpEnvelope caps_response =
      fixture::parse_response(caps.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(caps_response.status, gcp::Status::ok);
  const std::string *caps_body = gcp::find_param(caps_response.params, "result");
  ASSERT_NE(caps_body, nullptr);
  EXPECT_NE(caps_body->find("\"hello\""), std::string::npos);
  EXPECT_NE(caps_body->find("\"catalog.list\""), std::string::npos);
  EXPECT_EQ(caps_body->find("unknown"), std::string::npos);
}

TEST(GcpDispatcherTest, CatalogListSearchAndDescribe) {
  ControlService service;
  service.catalog().add(CatalogEntry{"alpha", "Scene inspector", "scene",
                                     {"scene", "inspect"}, {"a"}, false, false});
  service.catalog().add(CatalogEntry{"beta", "Delete node", "scene",
                                     {"scene", "remove"}, {"b"}, true, true});
  GcpDispatcher dispatcher(service, "inst-1");

  const DispatchResult listed = dispatcher.handle_frame(
      7, fixture::make_request_json("catalog.list", "l1", "inst-1",
                                    {{"category", "scene"}, {"limit", "2"}}));
  bool ok = false;
  const gcp::GcpEnvelope list_response =
      fixture::parse_response(listed.response_text, ok);
  ASSERT_TRUE(ok);
  const std::string *list_body = gcp::find_param(list_response.params, "result");
  ASSERT_NE(list_body, nullptr);
  EXPECT_NE(list_body->find("alpha"), std::string::npos);
  EXPECT_NE(list_body->find("beta"), std::string::npos);

  const DispatchResult searched = dispatcher.handle_frame(
      7, fixture::make_request_json("catalog.search", "s1", "inst-1",
                                    {{"text", "inspector"}}));
  const gcp::GcpEnvelope search_response =
      fixture::parse_response(searched.response_text, ok);
  ASSERT_TRUE(ok);
  const std::string *search_body =
      gcp::find_param(search_response.params, "result");
  ASSERT_NE(search_body, nullptr);
  EXPECT_NE(search_body->find("alpha"), std::string::npos);

  const DispatchResult described = dispatcher.handle_frame(
      7, fixture::make_request_json("catalog.describe", "d1", "inst-1",
                                    {{"name", "b"}}));
  const gcp::GcpEnvelope describe_response =
      fixture::parse_response(described.response_text, ok);
  ASSERT_TRUE(ok);
  const std::string *describe_body =
      gcp::find_param(describe_response.params, "result");
  ASSERT_NE(describe_body, nullptr);
  EXPECT_NE(describe_body->find("beta"), std::string::npos);
  EXPECT_NE(describe_body->find("true"), std::string::npos);
}

TEST(GcpDispatcherTest, HighRiskCallNeedsConfirmationThenDispatches) {
  ControlService service;
  RecordingSink sink;
  GcpDispatcher dispatcher(service, "inst-1", &sink);
  StaticRiskProbe probe;
  probe.add("danger");
  service.invocation().set_risk_probe(&probe);

  const gcp::Params params = {{"action", "drop"}, {"tool", "danger"}};
  const DispatchResult first = dispatcher.handle_frame(
      9, fixture::make_request_json("call.start", "call-1", "inst-1", params));
  bool ok = false;
  const gcp::GcpEnvelope challenge_response =
      fixture::parse_response(first.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(challenge_response.status, gcp::Status::needs_confirmation);
  EXPECT_EQ(challenge_response.error, gcp::ErrorCode::confirmation_required);
  EXPECT_GT(challenge_response.confirmation.ttl_ms, 0);
  EXPECT_FALSE(challenge_response.confirmation.param_digest.empty());
  EXPECT_EQ(sink.call_count(), 0u);

  const gcp::Params confirm_params = {{"action", "drop"},
                                      {"confirm", "true"},
                                      {"request_id", "call-1"},
                                      {"tool", "danger"}};
  const DispatchResult second = dispatcher.handle_frame(
      9, fixture::make_request_json("call.confirm", "confirm-1", "inst-1",
                                    confirm_params));
  const gcp::GcpEnvelope accepted = fixture::parse_response(second.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(accepted.status, gcp::Status::ok);
  EXPECT_EQ(sink.call_count(), 1u);
  ASSERT_FALSE(sink.requests().empty());
  EXPECT_EQ(sink.requests().back().tool, "danger");
}

TEST(GcpDispatcherTest, DeferredCallReportsPendingAndCreatesTask) {
  ControlService service;
  InvocationResult pending;
  pending.status = gcp::Status::pending;
  pending.task_id = "task-7";
  pending.payload = "pending";
  FixedSink sink(pending);
  GcpDispatcher dispatcher(service, "inst-1", &sink);

  const DispatchResult started = dispatcher.handle_frame(
      9, fixture::make_request_json("call.start", "call-2", "inst-1",
                                    {{"tool", "scene"}}));
  bool ok = false;
  const gcp::GcpEnvelope pending_response =
      fixture::parse_response(started.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(pending_response.status, gcp::Status::pending);
  EXPECT_EQ(started.state, DispatchState::awaiting);
  ASSERT_TRUE(pending_response.task_id.has_value());
  EXPECT_EQ(*pending_response.task_id, "task-7");
  EXPECT_EQ(sink.call_count(), 1u);

  const DispatchResult fetched = dispatcher.handle_frame(
      9, fixture::make_request_json("call.get", "get-1", "inst-1",
                                    {{"task_id", "task-7"}}));
  const gcp::GcpEnvelope task_response =
      fixture::parse_response(fetched.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(task_response.status, gcp::Status::ok);
  const std::string *body = gcp::find_param(task_response.params, "result");
  ASSERT_NE(body, nullptr);
  EXPECT_NE(body->find("task-7"), std::string::npos);
}

TEST(GcpDispatcherTest, DuplicateRequestIsResultUnknownWithoutRedispatch) {
  ControlService service;
  RecordingSink sink;
  GcpDispatcher dispatcher(service, "inst-1", &sink);
  const std::string request =
      fixture::make_request_json("call.start", "call-3", "inst-1",
                                 {{"tool", "scene"}});

  const DispatchResult first = dispatcher.handle_frame(9, request);
  bool ok = false;
  const gcp::GcpEnvelope first_response =
      fixture::parse_response(first.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(first_response.status, gcp::Status::ok);
  EXPECT_EQ(sink.call_count(), 1u);

  const DispatchResult retry = dispatcher.handle_frame(9, request);
  const gcp::GcpEnvelope retry_response =
      fixture::parse_response(retry.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(retry.state, DispatchState::duplicate_rejected);
  EXPECT_EQ(retry_response.status, gcp::Status::error);
  EXPECT_EQ(retry_response.error, gcp::ErrorCode::result_unknown);
  EXPECT_EQ(sink.call_count(), 1u);
  EXPECT_TRUE(dispatcher.was_written("call-3"));
}

TEST(GcpDispatcherTest, TokenStaysOutOfEnvelopeTraceAndErrors) {
  ControlService service;
  RecordingSink sink;
  GcpDispatcher dispatcher(service, "inst-1", &sink);

  const std::string auth_frame =
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"_meta":{"gda_token":"super-secret-token"}}})";
  const DispatchResult ignored = dispatcher.handle_frame(11, auth_frame);
  EXPECT_EQ(ignored.state, DispatchState::auth_ignored);
  EXPECT_TRUE(ignored.response_text.empty());

  const gcp::Params params = {{"gda_token", "super-secret-token"},
                              {"tool", "scene"}};
  const DispatchResult result = dispatcher.handle_frame(
      11, fixture::make_request_json("catalog.list", "l2", "inst-1", params));
  bool ok = false;
  const gcp::GcpEnvelope response = fixture::parse_response(result.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(response.status, gcp::Status::ok);
  EXPECT_EQ(result.response_text.find("super-secret-token"), std::string::npos);
  EXPECT_NE(response.trace.traceparent.find("00-"), std::string::npos);
}

TEST(GcpDispatcherTest, EventOutboxEnforcesHardLimitAndBackpressure) {
  ControlService service;
  GcpDispatcher dispatcher(service, "inst-1");
  for (std::size_t index = 0; index < GcpDispatcher::kEventOutboxHardLimit;
       ++index) {
    ASSERT_TRUE(dispatcher.publish_event("task.updated", "payload", 13));
  }
  EXPECT_EQ(dispatcher.pending_events(), GcpDispatcher::kEventOutboxHardLimit);
  EXPECT_EQ(dispatcher.event_pressure(), gcp::Backpressure::hard);
  EXPECT_FALSE(dispatcher.publish_event("task.updated", "overflow", 13));
  EXPECT_EQ(dispatcher.pending_events(), GcpDispatcher::kEventOutboxHardLimit);

  const std::vector<OutboxEntry> events = dispatcher.drain_events();
  ASSERT_EQ(events.size(), GcpDispatcher::kEventOutboxHardLimit);
  EXPECT_EQ(dispatcher.event_pressure(), gcp::Backpressure::none);
  bool ok = false;
  const gcp::GcpEnvelope event = fixture::parse_response(events.front().frame, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(event.kind, gcp::Kind::event);
  EXPECT_EQ(event.seq, 1u);
}

TEST(GcpDispatcherTest, EventsSubscribeAndResumeDeliverEnvelopes) {
  ControlService service;
  GcpDispatcher dispatcher(service, "inst-1");
  ASSERT_TRUE(dispatcher.publish_event("task.updated", "first", 21));
  ASSERT_TRUE(dispatcher.publish_event("task.updated", "second", 21));

  const DispatchResult subscribed = dispatcher.handle_frame(
      21, fixture::make_request_json("events.subscribe", "sub-1", "inst-1",
                                     {{"cursor", "0"}, {"event_type", "task.updated"}}));
  bool ok = false;
  const gcp::GcpEnvelope sub_response =
      fixture::parse_response(subscribed.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(sub_response.status, gcp::Status::ok);
  const std::string *sub_body = gcp::find_param(sub_response.params, "result");
  ASSERT_NE(sub_body, nullptr);
  EXPECT_NE(sub_body->find("sub-"), std::string::npos);
  dispatcher.drain_events();

  std::string subscription_id;
  const std::size_t marker = sub_body->find("sub-");
  if (marker != std::string::npos) {
    const std::size_t end = sub_body->find('"', marker);
    subscription_id = sub_body->substr(marker, end == std::string::npos
                                                   ? std::string::npos
                                                   : end - marker);
  }
  ASSERT_FALSE(subscription_id.empty());
  const DispatchResult resumed = dispatcher.handle_frame(
      21, fixture::make_request_json(
              "events.resume", "resume-1", "inst-1",
              {{"limit", "10"}, {"subscription_id", subscription_id}}));
  const gcp::GcpEnvelope resume_response =
      fixture::parse_response(resumed.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(resume_response.status, gcp::Status::ok);
  EXPECT_EQ(dispatcher.pending_events(), 2u);
}

TEST(GcpDispatcherTest, ValidationRejectsVersionInstanceAndMalformedFrames) {
  ControlService service;
  GcpDispatcher dispatcher(service, "inst-1");

  gcp::GcpEnvelope wrong_version;
  std::string error;
  ASSERT_TRUE(GcpDispatcher::decode(
      fixture::make_request_json("hello", "v1", "inst-1"), wrong_version, error));
  wrong_version.v = 2;
  std::string wrong_text;
  ASSERT_TRUE(GcpDispatcher::encode(wrong_version, wrong_text));
  const DispatchResult version_result = dispatcher.handle_frame(5, wrong_text);
  bool ok = false;
  const gcp::GcpEnvelope version_response =
      fixture::parse_response(version_result.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(version_response.error, gcp::ErrorCode::unsupported_version);

  const DispatchResult instance_result = dispatcher.handle_frame(
      5, fixture::make_request_json("hello", "i1", "other-instance"));
  const gcp::GcpEnvelope instance_response =
      fixture::parse_response(instance_result.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(instance_response.error, gcp::ErrorCode::instance_mismatch);

  const DispatchResult malformed =
      dispatcher.handle_frame(5, fixture::make_invalid_json());
  const gcp::GcpEnvelope malformed_response =
      fixture::parse_response(malformed.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(malformed.state, DispatchState::decode_failed);
  EXPECT_EQ(malformed_response.error, gcp::ErrorCode::result_unknown);
  EXPECT_FALSE(malformed_response.trace.traceparent.empty());
}
