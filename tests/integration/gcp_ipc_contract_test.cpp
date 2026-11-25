#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/control/control_service.hpp"
#include "core/control/gcp_dispatcher.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "core/transport/ipc_framing.hpp"
#include "core/transport/ipc_transport.hpp"
#include "support/gcp_dispatcher_fixture.hpp"

namespace {

using namespace gda;
using namespace gda::control::fixture;
namespace ipc_framing = godot_autopilot::ipc_framing;
namespace ipc_transport = godot_autopilot::ipc_transport;

} // namespace

TEST(GcpIpcContractTest, LengthPrefixCarriesGcpEnvelopeRoundTrip) {
  const std::string request_text =
      control::fixture::make_request_json("hello", "ipc-1", "inst-1");
  const std::string wire = ipc_framing::encode_length_prefixed(request_text);
  const ipc_framing::ExtractedFrames extracted =
      ipc_framing::extract_length_prefixed(wire, ipc_framing::kMaxFrameBytes);
  ASSERT_EQ(extracted.frames.size(), 1u);
  EXPECT_FALSE(extracted.overflow);
  EXPECT_TRUE(extracted.remainder.empty());

  control::ControlService service;
  control::GcpDispatcher dispatcher(service, "inst-1");
  const control::DispatchResult result =
      dispatcher.handle_frame(1, extracted.frames.front());
  ASSERT_FALSE(result.response_text.empty());

  const std::string response_wire =
      ipc_framing::encode_length_prefixed(result.response_text);
  const ipc_framing::ExtractedFrames response_frames =
      ipc_framing::extract_length_prefixed(response_wire,
                                           ipc_framing::kMaxFrameBytes);
  ASSERT_EQ(response_frames.frames.size(), 1u);
  bool ok = false;
  const gcp::GcpEnvelope response =
      control::fixture::parse_response(response_frames.frames.front(), ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(response.kind, gcp::Kind::response);
  ASSERT_TRUE(response.request_id.has_value());
  EXPECT_EQ(*response.request_id, "ipc-1");
  EXPECT_EQ(response.status, gcp::Status::ok);
  EXPECT_TRUE(gcp::validate_envelope(response).empty());
}

TEST(GcpIpcContractTest, IpcBudgetsAndFrameLimitsArePreserved) {
  EXPECT_EQ(ipc_framing::kLengthPrefixBytes, 4u);
  EXPECT_EQ(ipc_framing::kMaxFrameBytes, 4u * 1024u * 1024u);
  EXPECT_EQ(ipc_framing::max_wire_buffer_bytes(),
            ipc_framing::kMaxFrameBytes + ipc_framing::kLengthPrefixBytes);
  EXPECT_EQ(ipc_transport::kMaxIpcConnections, 8u);
  EXPECT_EQ(ipc_transport::kMaxIpcInFlight, 16u);
  EXPECT_EQ(
      std::chrono::duration_cast<std::chrono::minutes>(
          ipc_transport::kIpcIdleTimeout)
          .count(),
      10);
}

TEST(GcpIpcContractTest, AuthFrameIsSeparatedFromGcpEnvelope) {
  const std::string auth_frame =
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"_meta":{"gda_token":"ipc-secret-token"}}})";
  const std::string wire = ipc_framing::encode_length_prefixed(auth_frame);
  const ipc_framing::ExtractedFrames extracted =
      ipc_framing::extract_length_prefixed(wire, ipc_framing::kMaxFrameBytes);
  ASSERT_EQ(extracted.frames.size(), 1u);

  control::ControlService service;
  control::fixture::RecordingSink sink;
  control::GcpDispatcher dispatcher(service, "inst-1", &sink);
  const control::DispatchResult result =
      dispatcher.handle_frame(2, extracted.frames.front());
  EXPECT_EQ(result.state, control::DispatchState::auth_ignored);
  EXPECT_TRUE(result.response_text.empty());
  EXPECT_EQ(result.response_text.find("ipc-secret-token"), std::string::npos);
}

TEST(GcpIpcContractTest, WrittenRequestIsNotRetriedAndMarksResultUnknown) {
  control::ControlService service;
  control::fixture::RecordingSink sink;
  control::GcpDispatcher dispatcher(service, "inst-1", &sink);
  const std::string request_text = control::fixture::make_request_json(
      "call.start", "ipc-call-1", "inst-1", {{"tool", "scene"}});
  const std::string wire = ipc_framing::encode_length_prefixed(request_text);
  const ipc_framing::ExtractedFrames first_extracted =
      ipc_framing::extract_length_prefixed(wire, ipc_framing::kMaxFrameBytes);
  ASSERT_EQ(first_extracted.frames.size(), 1u);

  const control::DispatchResult first =
      dispatcher.handle_frame(3, first_extracted.frames.front());
  bool ok = false;
  const gcp::GcpEnvelope first_response =
      control::fixture::parse_response(first.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(first_response.status, gcp::Status::ok);
  dispatcher.mark_written("ipc-call-1");

  const ipc_framing::ExtractedFrames retry_extracted =
      ipc_framing::extract_length_prefixed(wire, ipc_framing::kMaxFrameBytes);
  ASSERT_EQ(retry_extracted.frames.size(), 1u);
  const control::DispatchResult retry =
      dispatcher.handle_frame(3, retry_extracted.frames.front());
  const gcp::GcpEnvelope retry_response =
      control::fixture::parse_response(retry.response_text, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(retry.state, control::DispatchState::duplicate_rejected);
  EXPECT_EQ(retry_response.error, gcp::ErrorCode::result_unknown);
  EXPECT_EQ(sink.call_count(), 1u);
}

TEST(GcpIpcContractTest, EventOutboxFramesAreLengthPrefixedGcpEvents) {
  control::ControlService service;
  control::GcpDispatcher dispatcher(service, "inst-1");
  ASSERT_TRUE(dispatcher.publish_event("task.updated", "one", 4,
                                       control::fixture::make_traceparent(1)));
  ASSERT_TRUE(dispatcher.publish_event("task.updated", "two", 4,
                                       control::fixture::make_traceparent(2)));

  std::string wire;
  const std::vector<control::OutboxEntry> events = dispatcher.drain_events();
  ASSERT_EQ(events.size(), 2u);
  for (const control::OutboxEntry &event : events) {
    EXPECT_EQ(event.connection_id, 4u);
    wire += ipc_framing::encode_length_prefixed(event.frame);
  }
  const ipc_framing::ExtractedFrames extracted =
      ipc_framing::extract_length_prefixed(wire, ipc_framing::kMaxFrameBytes);
  ASSERT_EQ(extracted.frames.size(), 2u);
  bool ok = false;
  const gcp::GcpEnvelope first =
      control::fixture::parse_response(extracted.frames[0], ok);
  ASSERT_TRUE(ok);
  const gcp::GcpEnvelope second =
      control::fixture::parse_response(extracted.frames[1], ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(first.kind, gcp::Kind::event);
  EXPECT_EQ(second.kind, gcp::Kind::event);
  EXPECT_EQ(first.seq, 1u);
  EXPECT_EQ(second.seq, 2u);
  EXPECT_TRUE(gcp::validate_envelope(first).empty());
  EXPECT_TRUE(gcp::validate_envelope(second).empty());
}
