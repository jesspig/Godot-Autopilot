#include "core/editor_coords.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

using godot_autopilot::coords::Affine;
using godot_autopilot::coords::DiffResult;
using godot_autopilot::coords::ImageSize;
using godot_autopilot::coords::Mark;
using godot_autopilot::coords::Point;
using godot_autopilot::coords::Rect;
using godot_autopilot::coords::compose;
using godot_autopilot::coords::diff_sample;
using godot_autopilot::coords::fit_within;
using godot_autopilot::coords::invert;
using godot_autopilot::coords::layout_marks;
using godot_autopilot::coords::make_affine;
using godot_autopilot::coords::rect_contains;
using godot_autopilot::coords::rect_intersect;
using godot_autopilot::coords::transform_point;
using godot_autopilot::coords::transform_rect;

namespace {

std::vector<uint8_t> solid_image(int width, int height, int channels, uint8_t value) {
  return std::vector<uint8_t>(static_cast<size_t>(width) * height * channels, value);
}

} // namespace

TEST(EditorCoordsTest, TransformPointAppliesScaleThenTranslation) {
  const Affine a = make_affine(2.0, 0.0, 0.0, 3.0, 10.0, -5.0);
  const Point p = transform_point(a, Point{1.0, 2.0});
  EXPECT_DOUBLE_EQ(12.0, p.x);
  EXPECT_DOUBLE_EQ(1.0, p.y);
}

TEST(EditorCoordsTest, TransformPointRotatesNinetyDegrees) {
  const Affine rot = make_affine(0.0, -1.0, 1.0, 0.0, 0.0, 0.0);
  const Point unit_x = transform_point(rot, Point{1.0, 0.0});
  EXPECT_DOUBLE_EQ(0.0, unit_x.x);
  EXPECT_DOUBLE_EQ(1.0, unit_x.y);
  const Point unit_y = transform_point(rot, Point{0.0, 1.0});
  EXPECT_DOUBLE_EQ(-1.0, unit_y.x);
  EXPECT_DOUBLE_EQ(0.0, unit_y.y);
}

TEST(EditorCoordsTest, ComposeMatchesNestedApplication) {
  const Affine outer = make_affine(0.0, -1.0, 1.0, 0.0, 0.0, 0.0);
  const Affine inner = make_affine(2.0, 0.0, 0.0, 2.0, 3.0, 4.0);
  const Affine combined = compose(outer, inner);
  EXPECT_DOUBLE_EQ(0.0, combined.xx);
  EXPECT_DOUBLE_EQ(-2.0, combined.xy);
  EXPECT_DOUBLE_EQ(2.0, combined.yx);
  EXPECT_DOUBLE_EQ(0.0, combined.yy);
  EXPECT_DOUBLE_EQ(-4.0, combined.ox);
  EXPECT_DOUBLE_EQ(3.0, combined.oy);
  const Point nested = transform_point(outer, transform_point(inner, Point{1.0, 1.0}));
  const Point direct = transform_point(combined, Point{1.0, 1.0});
  EXPECT_DOUBLE_EQ(-6.0, nested.x);
  EXPECT_DOUBLE_EQ(5.0, nested.y);
  EXPECT_DOUBLE_EQ(nested.x, direct.x);
  EXPECT_DOUBLE_EQ(nested.y, direct.y);
}

TEST(EditorCoordsTest, ComposeOrderMatters) {
  const Affine translate = make_affine(1.0, 0.0, 0.0, 1.0, 10.0, 0.0);
  const Affine scale = make_affine(2.0, 0.0, 0.0, 2.0, 0.0, 0.0);
  const Point after_scale = transform_point(compose(translate, scale), Point{1.0, 1.0});
  EXPECT_DOUBLE_EQ(12.0, after_scale.x);
  EXPECT_DOUBLE_EQ(2.0, after_scale.y);
  const Point after_translate = transform_point(compose(scale, translate), Point{1.0, 1.0});
  EXPECT_DOUBLE_EQ(22.0, after_translate.x);
  EXPECT_DOUBLE_EQ(2.0, after_translate.y);
}

TEST(EditorCoordsTest, ComposeWithIdentityIsNeutral) {
  const Affine a = make_affine(0.0, -1.0, 1.0, 0.0, 7.0, -3.0);
  const Affine id = make_affine(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
  const Affine left = compose(id, a);
  EXPECT_DOUBLE_EQ(0.0, left.xx);
  EXPECT_DOUBLE_EQ(-1.0, left.xy);
  EXPECT_DOUBLE_EQ(1.0, left.yx);
  EXPECT_DOUBLE_EQ(0.0, left.yy);
  EXPECT_DOUBLE_EQ(7.0, left.ox);
  EXPECT_DOUBLE_EQ(-3.0, left.oy);
  const Affine right = compose(a, id);
  EXPECT_DOUBLE_EQ(0.0, right.xx);
  EXPECT_DOUBLE_EQ(-1.0, right.xy);
  EXPECT_DOUBLE_EQ(1.0, right.yx);
  EXPECT_DOUBLE_EQ(0.0, right.yy);
  EXPECT_DOUBLE_EQ(7.0, right.ox);
  EXPECT_DOUBLE_EQ(-3.0, right.oy);
}

TEST(EditorCoordsTest, InvertRoundTripsScaledTranslation) {
  const Affine a = make_affine(2.0, 0.0, 0.0, 4.0, 8.0, 12.0);
  Affine inv;
  ASSERT_TRUE(invert(a, inv));
  EXPECT_DOUBLE_EQ(0.5, inv.xx);
  EXPECT_DOUBLE_EQ(0.0, inv.xy);
  EXPECT_DOUBLE_EQ(0.0, inv.yx);
  EXPECT_DOUBLE_EQ(0.25, inv.yy);
  EXPECT_DOUBLE_EQ(-4.0, inv.ox);
  EXPECT_DOUBLE_EQ(-3.0, inv.oy);
  const Point round = transform_point(inv, transform_point(a, Point{4.0, 9.0}));
  EXPECT_DOUBLE_EQ(4.0, round.x);
  EXPECT_DOUBLE_EQ(9.0, round.y);
}

TEST(EditorCoordsTest, InvertRoundTripsRotation) {
  const Affine rot = make_affine(0.0, -1.0, 1.0, 0.0, 3.0, 4.0);
  Affine inv;
  ASSERT_TRUE(invert(rot, inv));
  EXPECT_DOUBLE_EQ(0.0, inv.xx);
  EXPECT_DOUBLE_EQ(1.0, inv.xy);
  EXPECT_DOUBLE_EQ(-1.0, inv.yx);
  EXPECT_DOUBLE_EQ(0.0, inv.yy);
  EXPECT_DOUBLE_EQ(-4.0, inv.ox);
  EXPECT_DOUBLE_EQ(3.0, inv.oy);
  const Point round = transform_point(inv, transform_point(rot, Point{2.0, 5.0}));
  EXPECT_DOUBLE_EQ(2.0, round.x);
  EXPECT_DOUBLE_EQ(5.0, round.y);
}

TEST(EditorCoordsTest, InvertRejectsSingularMatrix) {
  Affine out;
  EXPECT_FALSE(invert(make_affine(0.0, 0.0, 0.0, 0.0, 0.0, 0.0), out));
  EXPECT_FALSE(invert(make_affine(1.0, 2.0, 2.0, 4.0, 0.0, 0.0), out));
  EXPECT_FALSE(invert(make_affine(1e-5, 0.0, 0.0, 1e-5, 0.0, 0.0), out));
  EXPECT_TRUE(invert(make_affine(1e-4, 0.0, 0.0, 1e-4, 0.0, 0.0), out));
}

TEST(EditorCoordsTest, TransformRectIdentity) {
  const Affine id = make_affine(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
  const Rect r = transform_rect(id, Rect{1.0, 2.0, 3.0, 4.0});
  EXPECT_DOUBLE_EQ(1.0, r.x);
  EXPECT_DOUBLE_EQ(2.0, r.y);
  EXPECT_DOUBLE_EQ(3.0, r.w);
  EXPECT_DOUBLE_EQ(4.0, r.h);
}

TEST(EditorCoordsTest, TransformRectScaleAndTranslate) {
  const Affine a = make_affine(2.0, 0.0, 0.0, 2.0, 1.0, -1.0);
  const Rect r = transform_rect(a, Rect{1.0, 1.0, 2.0, 3.0});
  EXPECT_DOUBLE_EQ(3.0, r.x);
  EXPECT_DOUBLE_EQ(1.0, r.y);
  EXPECT_DOUBLE_EQ(4.0, r.w);
  EXPECT_DOUBLE_EQ(6.0, r.h);
}

TEST(EditorCoordsTest, TransformRectRotatesNinetyDegrees) {
  const Affine rot = make_affine(0.0, -1.0, 1.0, 0.0, 0.0, 0.0);
  const Rect r = transform_rect(rot, Rect{1.0, 2.0, 3.0, 4.0});
  EXPECT_DOUBLE_EQ(-6.0, r.x);
  EXPECT_DOUBLE_EQ(1.0, r.y);
  EXPECT_DOUBLE_EQ(4.0, r.w);
  EXPECT_DOUBLE_EQ(3.0, r.h);
}

TEST(EditorCoordsTest, TransformRectMirrorsNegativeScale) {
  const Affine mirror = make_affine(-1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
  const Rect r = transform_rect(mirror, Rect{1.0, 2.0, 3.0, 4.0});
  EXPECT_DOUBLE_EQ(-4.0, r.x);
  EXPECT_DOUBLE_EQ(2.0, r.y);
  EXPECT_DOUBLE_EQ(3.0, r.w);
  EXPECT_DOUBLE_EQ(4.0, r.h);
}

TEST(EditorCoordsTest, RectContainsIncludesBoundary) {
  const Rect r{0.0, 0.0, 10.0, 5.0};
  EXPECT_TRUE(rect_contains(r, Point{0.0, 0.0}));
  EXPECT_TRUE(rect_contains(r, Point{10.0, 5.0}));
  EXPECT_TRUE(rect_contains(r, Point{5.0, 2.5}));
  EXPECT_FALSE(rect_contains(r, Point{-0.001, 0.0}));
  EXPECT_FALSE(rect_contains(r, Point{0.0, 5.001}));
}

TEST(EditorCoordsTest, RectIntersectDisjointReturnsEmpty) {
  const Rect r = rect_intersect(Rect{0.0, 0.0, 10.0, 10.0}, Rect{20.0, 20.0, 5.0, 5.0});
  EXPECT_DOUBLE_EQ(0.0, r.w);
  EXPECT_DOUBLE_EQ(0.0, r.h);
  const Rect touching = rect_intersect(Rect{0.0, 0.0, 10.0, 10.0}, Rect{10.0, 0.0, 5.0, 5.0});
  EXPECT_DOUBLE_EQ(0.0, touching.w);
  EXPECT_DOUBLE_EQ(0.0, touching.h);
}

TEST(EditorCoordsTest, RectIntersectPartialAndContained) {
  const Rect partial = rect_intersect(Rect{0.0, 0.0, 10.0, 10.0}, Rect{5.0, 5.0, 10.0, 10.0});
  EXPECT_DOUBLE_EQ(5.0, partial.x);
  EXPECT_DOUBLE_EQ(5.0, partial.y);
  EXPECT_DOUBLE_EQ(5.0, partial.w);
  EXPECT_DOUBLE_EQ(5.0, partial.h);
  const Rect contained = rect_intersect(Rect{0.0, 0.0, 10.0, 10.0}, Rect{2.0, 3.0, 4.0, 5.0});
  EXPECT_DOUBLE_EQ(2.0, contained.x);
  EXPECT_DOUBLE_EQ(3.0, contained.y);
  EXPECT_DOUBLE_EQ(4.0, contained.w);
  EXPECT_DOUBLE_EQ(5.0, contained.h);
}

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

TEST(EditorCoordsTest, LayoutMarksSeparatesOverlappingRects) {
  const std::vector<Rect> rects{Rect{10.0, 20.0, 5.0, 5.0}, Rect{10.0, 20.0, 5.0, 5.0}};
  const std::vector<Mark> marks = layout_marks(rects);
  ASSERT_EQ(2u, marks.size());
  EXPECT_DOUBLE_EQ(12.0, marks[0].x);
  EXPECT_DOUBLE_EQ(28.0, marks[0].y);
  EXPECT_DOUBLE_EQ(12.0, marks[1].x);
  EXPECT_DOUBLE_EQ(38.0, marks[1].y);
  EXPECT_GE(marks[1].y - marks[0].y, 10.0);
}

TEST(EditorCoordsTest, LayoutMarksKeepsDistantRectsAtInitialPosition) {
  const std::vector<Rect> rects{Rect{0.0, 0.0, 10.0, 10.0}, Rect{100.0, 0.0, 10.0, 10.0}};
  const std::vector<Mark> marks = layout_marks(rects);
  ASSERT_EQ(2u, marks.size());
  EXPECT_DOUBLE_EQ(2.0, marks[0].x);
  EXPECT_DOUBLE_EQ(8.0, marks[0].y);
  EXPECT_DOUBLE_EQ(102.0, marks[1].x);
  EXPECT_DOUBLE_EQ(8.0, marks[1].y);
}

TEST(EditorCoordsTest, LayoutMarksShiftsWhenXIsCloseAndYIsClose) {
  const std::vector<Rect> rects{Rect{0.0, 0.0, 10.0, 10.0}, Rect{5.0, 0.0, 10.0, 10.0}};
  const std::vector<Mark> marks = layout_marks(rects);
  ASSERT_EQ(2u, marks.size());
  EXPECT_DOUBLE_EQ(7.0, marks[1].x);
  EXPECT_DOUBLE_EQ(18.0, marks[1].y);
}

TEST(EditorCoordsTest, LayoutMarksKeepsInitialWhenYIsFar) {
  const std::vector<Rect> rects{Rect{0.0, 0.0, 10.0, 10.0}, Rect{5.0, 20.0, 10.0, 10.0}};
  const std::vector<Mark> marks = layout_marks(rects);
  ASSERT_EQ(2u, marks.size());
  EXPECT_DOUBLE_EQ(7.0, marks[1].x);
  EXPECT_DOUBLE_EQ(28.0, marks[1].y);
}

TEST(EditorCoordsTest, LayoutMarksAccumulatesShifts) {
  const std::vector<Rect> rects(3, Rect{0.0, 0.0, 4.0, 4.0});
  const std::vector<Mark> marks = layout_marks(rects);
  ASSERT_EQ(3u, marks.size());
  EXPECT_DOUBLE_EQ(8.0, marks[0].y);
  EXPECT_DOUBLE_EQ(18.0, marks[1].y);
  EXPECT_DOUBLE_EQ(28.0, marks[2].y);
}

TEST(EditorCoordsTest, LayoutMarksStopsAfterFiftyShifts) {
  const std::vector<Rect> rects(52, Rect{0.0, 0.0, 4.0, 4.0});
  const std::vector<Mark> marks = layout_marks(rects);
  ASSERT_EQ(52u, marks.size());
  EXPECT_DOUBLE_EQ(2.0, marks[50].x);
  EXPECT_DOUBLE_EQ(508.0, marks[50].y);
  EXPECT_DOUBLE_EQ(2.0, marks[51].x);
  EXPECT_DOUBLE_EQ(508.0, marks[51].y);
}

TEST(EditorCoordsTest, LayoutMarksEmptyInput) {
  EXPECT_TRUE(layout_marks({}).empty());
}
