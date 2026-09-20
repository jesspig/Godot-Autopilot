#include "core/editor_coords.hpp"

#include <gtest/gtest.h>

#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

using godot_autopilot::coords::viewport_point_to_window;
using godot_autopilot::coords::viewport_rect_center_to_window;


namespace {

godot::Transform2D stretch_transform(godot::real_t scale, godot::real_t margin_x,
                                     godot::real_t margin_y) {
  return godot::Transform2D(0.0, godot::Size2(scale, scale), 0.0,
                            godot::Vector2(margin_x, margin_y));
}

} // namespace

TEST(GameUiCoordsTest, StretchScaleMapsViewportPointToWindow) {
  const godot::Transform2D screen = stretch_transform(4.0, 0.0, 0.0);
  const godot::Vector2 window =
      viewport_point_to_window(screen, godot::Vector2(160, 105.5));
  EXPECT_NEAR(640.0, window.x, 1e-3);
  EXPECT_NEAR(422.0, window.y, 1e-3);
}

TEST(GameUiCoordsTest, IdentityTransformKeepsViewportPoint) {
  const godot::Vector2 point(123.25, -7.5);
  const godot::Vector2 window = viewport_point_to_window(godot::Transform2D(), point);
  EXPECT_NEAR(point.x, window.x, 1e-3);
  EXPECT_NEAR(point.y, window.y, 1e-3);
}

TEST(GameUiCoordsTest, LetterboxMarginAppliesAfterStretch) {
  const godot::Transform2D screen = stretch_transform(4.0, 10.0, 20.0);
  const godot::Vector2 window =
      viewport_point_to_window(screen, godot::Vector2(160, 105.5));
  EXPECT_NEAR(650.0, window.x, 1e-3);
  EXPECT_NEAR(442.0, window.y, 1e-3);
}

TEST(GameUiCoordsTest, OriginPointMapsToWindowOrigin) {
  EXPECT_NEAR(0.0, viewport_point_to_window(godot::Transform2D(),
                                            godot::Vector2(0, 0)).x,
              1e-3);
  const godot::Vector2 margined =
      viewport_point_to_window(stretch_transform(4.0, 10.0, 20.0), godot::Vector2(0, 0));
  EXPECT_NEAR(10.0, margined.x, 1e-3);
  EXPECT_NEAR(20.0, margined.y, 1e-3);
}

TEST(GameUiCoordsTest, CanvasTransformComposesBeforeScreenTransform) {
  const godot::Transform2D canvas = godot::Transform2D(0.0, godot::Size2(1, 1), 0.0,
                                                       godot::Vector2(10, 20));
  const godot::Transform2D screen = stretch_transform(4.0, 0.0, 0.0) * canvas;
  const godot::Vector2 window =
      viewport_point_to_window(screen, godot::Vector2(160, 105.5));
  EXPECT_NEAR(680.0, window.x, 1e-3);
  EXPECT_NEAR(502.0, window.y, 1e-3);
}

TEST(GameUiCoordsTest, RectCenterUsesViewportRectCenter) {
  const godot::Rect2 rect(130, 80, 60, 20);
  const godot::Transform2D screen = stretch_transform(4.0, 0.0, 0.0);
  const godot::Vector2 window = viewport_rect_center_to_window(screen, rect);
  EXPECT_NEAR(640.0, window.x, 1e-3);
  EXPECT_NEAR(360.0, window.y, 1e-3);
  const godot::Vector2 center = viewport_rect_center_to_window(godot::Transform2D(), rect);
  EXPECT_NEAR(160.0, center.x, 1e-3);
  EXPECT_NEAR(90.0, center.y, 1e-3);
}
