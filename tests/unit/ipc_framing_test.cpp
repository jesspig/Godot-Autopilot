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

TEST(IpcFramingTest, LengthPrefixedSingleFrameRoundTrips) {
  const std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":1}";
  const std::string encoded =
      godot_autopilot::ipc_framing::encode_length_prefixed(payload);
  const auto frames =
      godot_autopilot::ipc_framing::extract_length_prefixed(encoded);
  ASSERT_EQ(frames.frames.size(), 1u);
  EXPECT_EQ(frames.frames[0], payload);
  EXPECT_TRUE(frames.remainder.empty());
  EXPECT_FALSE(frames.overflow);
}

TEST(IpcFramingTest, LengthPrefixedStickyFramesAreSplit) {
  const std::string a = godot_autopilot::ipc_framing::encode_length_prefixed("a");
  const std::string b =
      godot_autopilot::ipc_framing::encode_length_prefixed("{\"b\":2}");
  const auto frames =
      godot_autopilot::ipc_framing::extract_length_prefixed(a + b);
  ASSERT_EQ(frames.frames.size(), 2u);
  EXPECT_EQ(frames.frames[0], "a");
  EXPECT_EQ(frames.frames[1], "{\"b\":2}");
  EXPECT_TRUE(frames.remainder.empty());
}

TEST(IpcFramingTest, LengthPrefixedPartialFrameStaysInRemainder) {
  const std::string encoded =
      godot_autopilot::ipc_framing::encode_length_prefixed("hello");
  const auto frames =
      godot_autopilot::ipc_framing::extract_length_prefixed(
          encoded.substr(0, encoded.size() - 2));
  EXPECT_TRUE(frames.frames.empty());
  EXPECT_EQ(frames.remainder.size(), encoded.size() - 2);
  EXPECT_FALSE(frames.overflow);
}

TEST(IpcFramingTest, LengthPrefixedOversizedFrameSetsOverflow) {
  const std::string encoded =
      godot_autopilot::ipc_framing::encode_length_prefixed(std::string(64, 'z'));
  const auto frames =
      godot_autopilot::ipc_framing::extract_length_prefixed(encoded, 32);
  EXPECT_TRUE(frames.overflow);
  EXPECT_TRUE(frames.frames.empty());
}

TEST(IpcFramingTest, LengthPrefixedFourMebibyteFrameRoundTrips) {
  const std::string payload(4 * 1024 * 1024, 'q');
  const std::string encoded =
      godot_autopilot::ipc_framing::encode_length_prefixed(payload);
  const auto frames =
      godot_autopilot::ipc_framing::extract_length_prefixed(encoded);
  ASSERT_EQ(frames.frames.size(), 1u);
  EXPECT_EQ(frames.frames[0].size(), payload.size());
  EXPECT_TRUE(frames.frames[0] == payload);
}