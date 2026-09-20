#include "core/editor_coords.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

using godot_autopilot::coords::DiffResult;
using godot_autopilot::coords::ImageSize;
using godot_autopilot::coords::diff_sample;
using godot_autopilot::coords::fit_within;
using godot_autopilot::coords::scale_size;

namespace {

std::vector<uint8_t> solid_image(int width, int height, int channels, uint8_t value) {
  return std::vector<uint8_t>(static_cast<size_t>(width) * height * channels, value);
}

} // namespace

TEST(EditorCoordsTest, FitWithinShrinksPreservingAspect) {
  const ImageSize a = fit_within(ImageSize{4096, 2048}, 1024);
  EXPECT_EQ(1024, a.width);
  EXPECT_EQ(512, a.height);
  const ImageSize b = fit_within(ImageSize{1920, 1080}, 1024);
  EXPECT_EQ(1024, b.width);
  EXPECT_EQ(576, b.height);
}

TEST(EditorCoordsTest, FitWithinDoesNotUpscale) {
  const ImageSize a = fit_within(ImageSize{100, 50}, 200);
  EXPECT_EQ(100, a.width);
  EXPECT_EQ(50, a.height);
  const ImageSize exact = fit_within(ImageSize{1024, 768}, 1024);
  EXPECT_EQ(1024, exact.width);
  EXPECT_EQ(768, exact.height);
}

TEST(EditorCoordsTest, FitWithinKeepsAtLeastOnePixel) {
  const ImageSize narrow = fit_within(ImageSize{1, 5000}, 100);
  EXPECT_EQ(1, narrow.width);
  EXPECT_EQ(100, narrow.height);
  const ImageSize flat = fit_within(ImageSize{5000, 1}, 100);
  EXPECT_EQ(100, flat.width);
  EXPECT_EQ(1, flat.height);
}

TEST(EditorCoordsTest, FitWithinNonPositiveMaxReturnsSource) {
  const ImageSize zero = fit_within(ImageSize{4096, 2048}, 0);
  EXPECT_EQ(4096, zero.width);
  EXPECT_EQ(2048, zero.height);
  const ImageSize negative = fit_within(ImageSize{4096, 2048}, -10);
  EXPECT_EQ(4096, negative.width);
  EXPECT_EQ(2048, negative.height);
}

TEST(EditorCoordsTest, ScaleSizeIdentityAtScaleOne) {
  const ImageSize r = scale_size(ImageSize{640, 480}, 1);
  EXPECT_EQ(640, r.width);
  EXPECT_EQ(480, r.height);
}

TEST(EditorCoordsTest, ScaleSizeUpscalesByIntegerFactor) {
  const ImageSize r = scale_size(ImageSize{160, 120}, 8);
  EXPECT_EQ(1280, r.width);
  EXPECT_EQ(960, r.height);
}

TEST(EditorCoordsTest, ScaleSizeRejectsInvalidInput) {
  EXPECT_EQ(0, scale_size(ImageSize{640, 480}, 0).width);
  EXPECT_EQ(0, scale_size(ImageSize{640, 480}, -2).width);
  EXPECT_EQ(0, scale_size(ImageSize{640, 480}, 9).width);
  EXPECT_EQ(0, scale_size(ImageSize{0, 480}, 2).width);
  EXPECT_EQ(0, scale_size(ImageSize{640, -1}, 2).height);
}

TEST(EditorCoordsTest, ScaleSizeRejectsDimensionOverflow) {
  const ImageSize pixels = scale_size(ImageSize{4096, 4096}, 8);
  EXPECT_EQ(0, pixels.width);
  EXPECT_EQ(0, pixels.height);
  const ImageSize wide = scale_size(ImageSize{100000000, 1}, 1);
  EXPECT_EQ(0, wide.width);
  EXPECT_EQ(0, wide.height);
}

TEST(EditorCoordsTest, ScaleSizeThenFitWithinCapsLongestSide) {
  const ImageSize scaled = scale_size(ImageSize{640, 480}, 2);
  ASSERT_EQ(1280, scaled.width);
  ASSERT_EQ(960, scaled.height);
  const ImageSize fitted = fit_within(scaled, 1024);
  EXPECT_EQ(1024, fitted.width);
  EXPECT_EQ(768, fitted.height);
  const ImageSize small = fit_within(scale_size(ImageSize{80, 60}, 8), 4096);
  EXPECT_EQ(640, small.width);
  EXPECT_EQ(480, small.height);
}

TEST(EditorCoordsTest, DiffSampleIdenticalImages) {
  const std::vector<uint8_t> a = solid_image(4, 4, 3, 100);
  const std::vector<uint8_t> b = solid_image(4, 4, 3, 100);
  const DiffResult r = diff_sample(a.data(), b.data(), 4, 4, 3, 1, 0);
  EXPECT_TRUE(r.comparable);
  EXPECT_DOUBLE_EQ(0.0, r.changed_ratio);
  EXPECT_FALSE(r.has_bbox);
}

TEST(EditorCoordsTest, DiffSampleDetectsSinglePixel) {
  const std::vector<uint8_t> a = solid_image(4, 4, 3, 100);
  std::vector<uint8_t> b = a;
  b[(static_cast<size_t>(1) * 4 + 2) * 3] = 200;
  const DiffResult r = diff_sample(a.data(), b.data(), 4, 4, 3, 1, 0);
  EXPECT_TRUE(r.comparable);
  EXPECT_DOUBLE_EQ(1.0 / 16.0, r.changed_ratio);
  EXPECT_TRUE(r.has_bbox);
  EXPECT_DOUBLE_EQ(2.0, r.bbox.x);
  EXPECT_DOUBLE_EQ(1.0, r.bbox.y);
  EXPECT_DOUBLE_EQ(1.0, r.bbox.w);
  EXPECT_DOUBLE_EQ(1.0, r.bbox.h);
}

TEST(EditorCoordsTest, DiffSampleRespectsChannelThreshold) {
  const std::vector<uint8_t> a = solid_image(4, 4, 3, 100);
  const std::vector<uint8_t> b = solid_image(4, 4, 3, 105);
  const DiffResult ignored = diff_sample(a.data(), b.data(), 4, 4, 3, 1, 10);
  EXPECT_TRUE(ignored.comparable);
  EXPECT_DOUBLE_EQ(0.0, ignored.changed_ratio);
  EXPECT_FALSE(ignored.has_bbox);
  const DiffResult counted = diff_sample(a.data(), b.data(), 4, 4, 3, 1, 4);
  EXPECT_TRUE(counted.comparable);
  EXPECT_DOUBLE_EQ(1.0, counted.changed_ratio);
  EXPECT_TRUE(counted.has_bbox);
  EXPECT_DOUBLE_EQ(0.0, counted.bbox.x);
  EXPECT_DOUBLE_EQ(0.0, counted.bbox.y);
  EXPECT_DOUBLE_EQ(4.0, counted.bbox.w);
  EXPECT_DOUBLE_EQ(4.0, counted.bbox.h);
}

TEST(EditorCoordsTest, DiffSampleRejectsInvalidInput) {
  const std::vector<uint8_t> a = solid_image(2, 2, 3, 10);
  const DiffResult null_a = diff_sample(nullptr, a.data(), 2, 2, 3, 1, 0);
  EXPECT_FALSE(null_a.comparable);
  EXPECT_DOUBLE_EQ(0.0, null_a.changed_ratio);
  EXPECT_FALSE(null_a.has_bbox);
  EXPECT_FALSE(diff_sample(a.data(), nullptr, 2, 2, 3, 1, 0).comparable);
  EXPECT_FALSE(diff_sample(a.data(), a.data(), 0, 2, 3, 1, 0).comparable);
  EXPECT_FALSE(diff_sample(a.data(), a.data(), 2, 0, 3, 1, 0).comparable);
  EXPECT_FALSE(diff_sample(a.data(), a.data(), 2, 2, 0, 1, 0).comparable);
}

TEST(EditorCoordsTest, DiffSampleSamplesOnGrid) {
  const std::vector<uint8_t> a = solid_image(4, 4, 3, 0);
  std::vector<uint8_t> b = a;
  b[(static_cast<size_t>(0) * 4 + 1) * 3] = 255;
  const DiffResult off_grid = diff_sample(a.data(), b.data(), 4, 4, 3, 2, 0);
  EXPECT_TRUE(off_grid.comparable);
  EXPECT_DOUBLE_EQ(0.0, off_grid.changed_ratio);
  EXPECT_FALSE(off_grid.has_bbox);
  b[(static_cast<size_t>(0) * 4 + 2) * 3] = 255;
  const DiffResult on_grid = diff_sample(a.data(), b.data(), 4, 4, 3, 2, 0);
  EXPECT_TRUE(on_grid.comparable);
  EXPECT_DOUBLE_EQ(0.25, on_grid.changed_ratio);
  EXPECT_TRUE(on_grid.has_bbox);
  EXPECT_DOUBLE_EQ(2.0, on_grid.bbox.x);
  EXPECT_DOUBLE_EQ(0.0, on_grid.bbox.y);
  EXPECT_DOUBLE_EQ(1.0, on_grid.bbox.w);
  EXPECT_DOUBLE_EQ(1.0, on_grid.bbox.h);
}

TEST(EditorCoordsTest, DiffSampleBboxSpansChangedPixels) {
  const std::vector<uint8_t> a = solid_image(4, 4, 3, 0);
  std::vector<uint8_t> b = a;
  b[(static_cast<size_t>(1) * 4 + 1) * 3] = 50;
  b[(static_cast<size_t>(2) * 4 + 2) * 3 + 2] = 60;
  const DiffResult r = diff_sample(a.data(), b.data(), 4, 4, 3, 1, 0);
  EXPECT_TRUE(r.comparable);
  EXPECT_DOUBLE_EQ(2.0 / 16.0, r.changed_ratio);
  EXPECT_TRUE(r.has_bbox);
  EXPECT_DOUBLE_EQ(1.0, r.bbox.x);
  EXPECT_DOUBLE_EQ(1.0, r.bbox.y);
  EXPECT_DOUBLE_EQ(2.0, r.bbox.w);
  EXPECT_DOUBLE_EQ(2.0, r.bbox.h);
}
