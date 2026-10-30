#include "core/editor_coords.hpp"

#include <gtest/gtest.h>

#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

using godot_autopilot::coords::viewport_point_to_window;
using godot_autopilot::coords::viewport_rect_center_to_window;
using godot_autopilot::coords::window_point_to_viewport;

static godot::Transform2D stretch_transform(godot::real_t scale, godot::real_t margin_x,
                                            godot::real_t margin_y) {
  return godot::Transform2D(0.0, godot::Size2(scale, scale), 0.0,
                            godot::Vector2(margin_x, margin_y));
}

TEST(CoordsInverseTest, IdentityRoundtripKeepsPoint) {
  const godot::Vector2 point(123.25, -7.5);
  const godot::Vector2 window = viewport_point_to_window(godot::Transform2D(), point);
  const godot::Vector2 back = window_point_to_viewport(godot::Transform2D(), window);
  EXPECT_NEAR(point.x, back.x, 1e-3);
  EXPECT_NEAR(point.y, back.y, 1e-3);
}

TEST(CoordsInverseTest, StretchRoundtripRecoversViewportPoint) {
  const godot::Transform2D screen = stretch_transform(4.0, 0.0, 0.0);
  const godot::Vector2 viewport_point(160, 105.5);
  const godot::Vector2 window = viewport_point_to_window(screen, viewport_point);
  EXPECT_NEAR(640.0, window.x, 1e-3);
  EXPECT_NEAR(422.0, window.y, 1e-3);
  const godot::Vector2 back = window_point_to_viewport(screen, window);
  EXPECT_NEAR(viewport_point.x, back.x, 1e-3);
  EXPECT_NEAR(viewport_point.y, back.y, 1e-3);
}

TEST(CoordsInverseTest, LetterboxMarginSubtractsBeforeUnscaling) {
  const godot::Transform2D screen = stretch_transform(4.0, 10.0, 20.0);
  const godot::Vector2 origin_back = window_point_to_viewport(screen, godot::Vector2(10, 20));
  EXPECT_NEAR(0.0, origin_back.x, 1e-3);
  EXPECT_NEAR(0.0, origin_back.y, 1e-3);
  const godot::Vector2 center_back =
      window_point_to_viewport(screen, godot::Vector2(650, 442));
  EXPECT_NEAR(160.0, center_back.x, 1e-3);
  EXPECT_NEAR(105.5, center_back.y, 1e-3);
}

TEST(CoordsInverseTest, RectCenterRoundtripRecoversCenter) {
  const godot::Rect2 rect(130, 80, 60, 20);
  const godot::Transform2D screen = stretch_transform(4.0, 0.0, 0.0);
  const godot::Vector2 window = viewport_rect_center_to_window(screen, rect);
  EXPECT_NEAR(640.0, window.x, 1e-3);
  EXPECT_NEAR(360.0, window.y, 1e-3);
  const godot::Vector2 back = window_point_to_viewport(screen, window);
  EXPECT_NEAR(160.0, back.x, 1e-3);
  EXPECT_NEAR(90.0, back.y, 1e-3);
}

TEST(CoordsInverseTest, SingularTransformFallsBackToInput) {
  const godot::Transform2D singular(0.0, godot::Size2(0, 0), 0.0, godot::Vector2(0, 0));
  const godot::Vector2 point(640, 360);
  const godot::Vector2 back = window_point_to_viewport(singular, point);
  EXPECT_NEAR(point.x, back.x, 1e-3);
  EXPECT_NEAR(point.y, back.y, 1e-3);
}

TEST(CoordsInverseTest, NearSingularTransformFallsBackToInput) {
  const godot::Transform2D near_singular =
      stretch_transform(static_cast<godot::real_t>(1e-5), 0.0, 0.0);
  const godot::Vector2 point(640, 360);
  const godot::Vector2 back = window_point_to_viewport(near_singular, point);
  EXPECT_NEAR(point.x, back.x, 1e-3);
  EXPECT_NEAR(point.y, back.y, 1e-3);
}
