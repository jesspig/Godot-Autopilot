#include <gtest/gtest.h>

#include <string>

#include "core/transport/ipc_framing.hpp"

TEST(IpcFramingTest, SingleLineProducesOneFrame) {
  const auto frames = godot_autopilot::ipc_framing::extract_frames("{\"id\":1}\n");
  ASSERT_EQ(frames.frames.size(), 1u);
  EXPECT_EQ(frames.frames[0], "{\"id\":1}");
  EXPECT_TRUE(frames.remainder.empty());
  EXPECT_FALSE(frames.overflow);
}

TEST(IpcFramingTest, StickyLinesProduceMultipleFrames) {
  const auto frames =
      godot_autopilot::ipc_framing::extract_frames("{\"a\":1}\n{\"b\":2}\n{\"c\":3}\n");
  ASSERT_EQ(frames.frames.size(), 3u);
  EXPECT_EQ(frames.frames[1], "{\"b\":2}");
  EXPECT_TRUE(frames.remainder.empty());
}

TEST(IpcFramingTest, PartialLineStaysInRemainder) {
  const auto frames = godot_autopilot::ipc_framing::extract_frames("{\"a\":1}\n{\"b\":");
  ASSERT_EQ(frames.frames.size(), 1u);
  EXPECT_EQ(frames.frames[0], "{\"a\":1}");
  EXPECT_EQ(frames.remainder, "{\"b\":");
  EXPECT_FALSE(frames.overflow);
}

TEST(IpcFramingTest, OversizedLineSetsOverflow) {
  const std::string big(64, 'x');
  const auto frames = godot_autopilot::ipc_framing::extract_frames(big + "\n", 32);
  EXPECT_TRUE(frames.overflow);
  EXPECT_TRUE(frames.frames.empty());
}

TEST(IpcFramingTest, CrlfAndEmptyLinesAreTrimmed) {
  const auto frames = godot_autopilot::ipc_framing::extract_frames("\r\n{\"a\":1}\r\n\n");
  ASSERT_EQ(frames.frames.size(), 1u);
  EXPECT_EQ(frames.frames[0], "{\"a\":1}");
}

TEST(IpcFramingTest, OversizedRemainderSetsOverflow) {
  const std::string big(64, 'y');
  const auto frames = godot_autopilot::ipc_framing::extract_frames(big, 32);
  EXPECT_TRUE(frames.overflow);
  EXPECT_TRUE(frames.remainder.empty());
}