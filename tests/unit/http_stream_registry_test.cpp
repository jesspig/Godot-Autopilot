#include "core/http_stream_registry.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using godot_autopilot::http_stream::Sink;
using godot_autopilot::http_stream::acquire;
using godot_autopilot::http_stream::broadcast;
using godot_autopilot::http_stream::kSinkMaxBacklogBytes;
using godot_autopilot::http_stream::release;

TEST(HttpStreamSinkTest, PushRejectsWhenOverLimit) {
  const auto sink = std::make_shared<Sink>();
  const std::string frame(1024, 'x');
  const size_t full_frames = kSinkMaxBacklogBytes / frame.size();
  for (size_t i = 0; i < full_frames; ++i) {
    EXPECT_TRUE(sink->push(frame));
  }
  EXPECT_FALSE(sink->closed());
  EXPECT_FALSE(sink->push("y"));
  EXPECT_TRUE(sink->closed());
  const std::vector<std::string> drained = sink->drain();
  EXPECT_EQ(drained.size(), full_frames);
}

TEST(HttpStreamSinkTest, CloseStopsAcceptingFrames) {
  const auto sink = std::make_shared<Sink>();
  ASSERT_TRUE(sink->push("a"));
  sink->close();
  EXPECT_TRUE(sink->closed());
  EXPECT_FALSE(sink->push("b"));
  EXPECT_TRUE(sink->drain().empty());
}

TEST(HttpStreamRegistryTest, BroadcastCountsRejections) {
  const size_t baseline = broadcast("warm-up");
  const std::shared_ptr<Sink> open = acquire("broadcast-counts-open");
  ASSERT_TRUE(open != nullptr);
  EXPECT_EQ(broadcast("probe"), baseline + 1);
  const std::shared_ptr<Sink> shut = acquire("broadcast-counts-closed");
  ASSERT_TRUE(shut != nullptr);
  shut->close();
  EXPECT_EQ(broadcast("probe"), baseline + 1);
  release("broadcast-counts-open");
  release("broadcast-counts-closed");
  EXPECT_EQ(broadcast("probe"), baseline);
}
