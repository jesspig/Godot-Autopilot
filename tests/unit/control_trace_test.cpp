#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "core/control/trace_service.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "support/control_fixture.hpp"

namespace {

using namespace gda::control;
using namespace gda::control::fixture;
namespace gcp = gda::gcp;

TEST(ControlTraceTest, ParseAndRebuildTraceparent) {
  const std::string parent = fixture::traceparent(7);
  ASSERT_TRUE(gcp::valid_traceparent(parent));
  const SpanContext parsed = TraceService::parse(parent);
  EXPECT_EQ(parsed.traceparent, parent);
  EXPECT_EQ(parsed.trace_id, parent.substr(3, 32));
  EXPECT_EQ(parsed.span_id, parent.substr(36, 16));
  EXPECT_EQ(parsed.parent_span, parsed.span_id);
  EXPECT_EQ(parsed.flags, "01");
  EXPECT_EQ(
      TraceService::make_traceparent(parsed.trace_id, parsed.span_id, parsed.flags),
      parent);
  EXPECT_TRUE(TraceService::parse("bad").traceparent.empty());
}

TEST(ControlTraceTest, BeginEndPropagatesTraceAndSpan) {
  TraceService service;
  const std::string incoming = fixture::traceparent(3);
  const SpanContext context = service.begin(incoming);
  EXPECT_EQ(context.trace_id, incoming.substr(3, 32));
  EXPECT_EQ(context.parent_span, incoming.substr(36, 16));
  EXPECT_NE(context.span_id, context.parent_span);
  EXPECT_TRUE(gcp::valid_traceparent(context.traceparent));
  EXPECT_EQ(service.depth(), 1u);
  EXPECT_EQ(service.current().traceparent, context.traceparent);
  EXPECT_FALSE(service.end("wrong-span"));
  EXPECT_TRUE(service.end(context.span_id));
  EXPECT_EQ(service.depth(), 0u);
}

TEST(ControlTraceTest, InvalidIncomingStartsNewTrace) {
  TraceService service;
  const SpanContext context = service.begin("bad", "0123456789abcdef");
  EXPECT_FALSE(context.trace_id.empty());
  EXPECT_EQ(context.span_id, "0123456789abcdef");
  EXPECT_TRUE(context.parent_span.empty());
  EXPECT_TRUE(gcp::valid_traceparent(context.traceparent));
  EXPECT_EQ(TraceService::new_trace_id().size(), 32u);
  EXPECT_EQ(TraceService::new_span_id().size(), 16u);
  service.clear();
  EXPECT_TRUE(service.stack().empty());
}

} // namespace
