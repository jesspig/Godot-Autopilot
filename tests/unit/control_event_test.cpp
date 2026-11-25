#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/control/event_service.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "support/control_fixture.hpp"

namespace {

using namespace gda::control;
using namespace gda::control::fixture;
namespace gcp = gda::gcp;

TEST(ControlEventTest, PublishSeqIsMonotonicAndCursorValidation) {
  EventService service;
  const EventRecord first =
      service.publish("inst-1", "task.updated", "a", fixture::traceparent(1));
  const EventRecord second = service.publish(
      "inst-1", "task.updated", "b", fixture::traceparent(2), "task-1");
  EXPECT_EQ(first.seq, 1u);
  EXPECT_EQ(second.seq, 2u);
  EXPECT_EQ(service.next_seq(), 3u);
  EXPECT_EQ(service.highest_seq("inst-1"), 2u);
  EXPECT_EQ(service.highest_seq("nope"), 0u);
  EXPECT_TRUE(service.cursor_valid("inst-1", 0));
  EXPECT_TRUE(service.cursor_valid("inst-1", 2));
  EXPECT_FALSE(service.cursor_valid("inst-1", 3));
  EXPECT_FALSE(service.cursor_valid("", 0));
  EXPECT_EQ(service.size(), 2u);
}

TEST(ControlEventTest, SubscribePollAdvancesCursor) {
  EventService service(100, 1000);
  service.publish("inst-1", "task.updated", "1");
  service.publish("inst-1", "task.updated", "2");
  service.publish("inst-1", "log", "3");
  const std::string id = service.subscribe("inst-1", "task.updated", 0);
  ASSERT_FALSE(id.empty());
  EXPECT_EQ(service.pending_for(id), 2u);
  gcp::Backpressure pressure = gcp::Backpressure::none;
  const std::vector<EventRecord> batch = service.poll(id, 10, &pressure);
  ASSERT_EQ(batch.size(), 2u);
  EXPECT_EQ(batch[0].seq, 1u);
  EXPECT_EQ(batch[1].seq, 2u);
  EXPECT_EQ(pressure, gcp::Backpressure::none);
  EXPECT_EQ(service.pending_for(id), 0u);
  EXPECT_TRUE(service.poll(id, 10).empty());
  EXPECT_TRUE(service.unsubscribe(id));
  EXPECT_FALSE(service.unsubscribe(id));
}

TEST(ControlEventTest, BackpressureHardBlocksPoll) {
  EventService service(1, 2);
  service.publish("inst-1", "task.updated", "1");
  service.publish("inst-1", "task.updated", "2");
  const std::string id = service.subscribe("inst-1", "", 0);
  ASSERT_FALSE(id.empty());
  EXPECT_EQ(service.backpressure(id), gcp::Backpressure::hard);
  gcp::Backpressure pressure = gcp::Backpressure::none;
  EXPECT_TRUE(service.poll(id, 10, &pressure).empty());
  EXPECT_EQ(pressure, gcp::Backpressure::hard);
  EXPECT_EQ(service.pending_for(id), 2u);
}

TEST(ControlEventTest, SinceAndFloorExpiresCursor) {
  EventService service;
  service.publish("inst-1", "a", "1");
  service.publish("inst-1", "a", "2");
  const std::vector<EventRecord> batch = service.since("inst-1", 1, 10);
  ASSERT_EQ(batch.size(), 1u);
  EXPECT_EQ(batch[0].seq, 2u);
  service.set_floor("inst-1", 2);
  EXPECT_FALSE(service.cursor_valid("inst-1", 1));
  EXPECT_TRUE(service.cursor_valid("inst-1", 2));
  EXPECT_EQ(service.subscribe("inst-1", "", 1), "");
  service.clear();
  EXPECT_EQ(service.next_seq(), 1u);
}

} // namespace
