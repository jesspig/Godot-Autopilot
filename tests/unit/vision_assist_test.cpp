#include "core/editor_coords.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

using godot_autopilot::coords::DiffResult;
using godot_autopilot::coords::ImageSize;
using godot_autopilot::coords::diff_sample;
using godot_autopilot::coords::fit_within;
using godot_autopilot::coords::scale_size;


namespace {

struct CropRect { double x = 0.0; double y = 0.0; double w = 0.0; double h = 0.0; };

CropRect region_intersect(const CropRect &a, const CropRect &b) {
  const double x0 = std::max(a.x, b.x);
  const double y0 = std::max(a.y, b.y);
  const double x1 = std::min(a.x + a.w, b.x + b.w);
  const double y1 = std::min(a.y + a.h, b.y + b.h);
  if (x1 <= x0 || y1 <= y0)
    return CropRect{};
  return CropRect{x0, y0, x1 - x0, y1 - y0};
}

} // namespace

TEST(VisionAssistTest, ScaleThenMaxDimensionCapsLongestSide) {
  const ImageSize scaled = scale_size(ImageSize{640, 480}, 2);
  ASSERT_EQ(1280, scaled.width);
  ASSERT_EQ(960, scaled.height);
  const ImageSize fitted = fit_within(scaled, 1024);
  EXPECT_EQ(1024, fitted.width);
  EXPECT_EQ(768, fitted.height);
}

TEST(VisionAssistTest, MaxDimensionNeverUpscalesSmallOverlay) {
  const ImageSize tiny = fit_within(ImageSize{24, 24}, 1024);
  EXPECT_EQ(24, tiny.width);
  EXPECT_EQ(24, tiny.height);
}

TEST(VisionAssistTest, ScaleRejectsOutOfRangeFactor) {
  EXPECT_EQ(0, scale_size(ImageSize{640, 480}, 0).width);
  EXPECT_EQ(0, scale_size(ImageSize{640, 480}, 9).width);
}

TEST(VisionAssistTest, RegionCropIsTopLeftIntersection) {
  const CropRect shared =
      region_intersect(CropRect{0.0, 0.0, 800.0, 600.0}, CropRect{700.0, 500.0, 200.0, 200.0});
  EXPECT_DOUBLE_EQ(700.0, shared.x);
  EXPECT_DOUBLE_EQ(500.0, shared.y);
  EXPECT_DOUBLE_EQ(100.0, shared.w);
  EXPECT_DOUBLE_EQ(100.0, shared.h);
  const CropRect empty =
      region_intersect(CropRect{0.0, 0.0, 800.0, 600.0}, CropRect{900.0, 0.0, 50.0, 50.0});
  EXPECT_DOUBLE_EQ(0.0, empty.w);
  EXPECT_DOUBLE_EQ(0.0, empty.h);
}

TEST(VisionAssistTest, DiffImageBboxSpansChangedRegion) {
  std::vector<uint8_t> a(static_cast<size_t>(8) * 8 * 4, 10);
  std::vector<uint8_t> b = a;
  b[(static_cast<size_t>(2) * 8 + 3) * 4] = 200;
  b[(static_cast<size_t>(5) * 8 + 6) * 4 + 1] = 200;
  const DiffResult r = diff_sample(a.data(), b.data(), 8, 8, 4, 1, 16);
  EXPECT_TRUE(r.comparable);
  EXPECT_TRUE(r.has_bbox);
  EXPECT_DOUBLE_EQ(3.0, r.bbox.x);
  EXPECT_DOUBLE_EQ(2.0, r.bbox.y);
  EXPECT_DOUBLE_EQ(4.0, r.bbox.w);
  EXPECT_DOUBLE_EQ(4.0, r.bbox.h);
  EXPECT_GT(r.changed_ratio, 0.0);
}

TEST(VisionAssistTest, DiffImageIgnoresSubThresholdNoise) {
  const std::vector<uint8_t> a(static_cast<size_t>(4) * 4 * 4, 100);
  const std::vector<uint8_t> b(static_cast<size_t>(4) * 4 * 4, 105);
  const DiffResult r = diff_sample(a.data(), b.data(), 4, 4, 4, 1, 16);
  EXPECT_TRUE(r.comparable);
  EXPECT_DOUBLE_EQ(0.0, r.changed_ratio);
  EXPECT_FALSE(r.has_bbox);
}

TEST(VisionAssistTest, AnnotateNodesMaxEnforcedBeforeDraw) {
  const ImageSize budget = fit_within(ImageSize{200, 200}, 50);
  EXPECT_EQ(50, budget.width);
  EXPECT_EQ(50, budget.height);
  EXPECT_EQ(0, scale_size(ImageSize{100, 100}, 51).width);
}
