#include "tools/domains/capture/capture_ops.hpp"

#include <gtest/gtest.h>

#include "core/config.hpp"

namespace {

using godot_autopilot::capture_ops::viewport_capture_is_degenerate;

constexpr int kNormalViewportWidth = 1280;
constexpr int kNormalViewportHeight = 704;
constexpr int kNormalImageWidth = 1280;
constexpr int kNormalImageHeight = 704;

struct SizeCase {
  int width;
  int height;
  bool degenerate;
};

TEST(CaptureViewportDegenerateTest, ThresholdConstantIsEight) {
  EXPECT_EQ(godot_autopilot::GDA_CAPTURE_MIN_DIMENSION, 8);
}

TEST(CaptureViewportDegenerateTest, ImageSizeMatrix) {
  const SizeCase cases[] = {
      {2, 2, true},        {1, 1, true},        {8, 8, true},
      {8, 9, true},        {9, 8, true},        {0, 0, true},
      {9, 9, false},       {9, 100, false},     {100, 9, false},
      {1280, 704, false},
  };
  for (const SizeCase &entry : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "image " << entry.width << "x" << entry.height);
    EXPECT_EQ(viewport_capture_is_degenerate(entry.width, entry.height,
                                             kNormalViewportWidth,
                                             kNormalViewportHeight),
              entry.degenerate);
  }
}

TEST(CaptureViewportDegenerateTest, EightPixelImageIsRejected) {
  EXPECT_TRUE(viewport_capture_is_degenerate(8, 8, kNormalViewportWidth,
                                             kNormalViewportHeight));
}

TEST(CaptureViewportDegenerateTest, NinePixelImageIsAccepted) {
  EXPECT_FALSE(viewport_capture_is_degenerate(9, 9, kNormalViewportWidth,
                                              kNormalViewportHeight));
}

TEST(CaptureViewportDegenerateTest, NinePixelViewportIsAccepted) {
  EXPECT_FALSE(viewport_capture_is_degenerate(9, 9, 9, 9));
}

TEST(CaptureViewportDegenerateTest, ZeroSizeViewportMatrix) {
  const SizeCase cases[] = {
      {0, 0, true},        {0, 704, true},      {1280, 0, true},
      {-1, -1, true},      {1, 1, false},       {1280, 704, false},
  };
  for (const SizeCase &entry : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "viewport " << entry.width << "x" << entry.height);
    EXPECT_EQ(viewport_capture_is_degenerate(kNormalImageWidth, kNormalImageHeight,
                                             entry.width, entry.height),
              entry.degenerate);
  }
}

TEST(CaptureViewportDegenerateTest, NormalCaptureIsAccepted) {
  EXPECT_FALSE(viewport_capture_is_degenerate(
      kNormalImageWidth, kNormalImageHeight, kNormalViewportWidth,
      kNormalViewportHeight));
}

} // namespace