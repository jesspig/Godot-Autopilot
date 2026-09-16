#include "core/editor_coords.hpp"

#include <gtest/gtest.h>

#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

using godot_autopilot::coords::viewport_point_to_window;
using godot_autopilot::coords::viewport_rect_center_to_window;

// click_game_ui_element 的注入坐标换算覆盖：Control::get_global_rect() 是画布
// 空间坐标，注入的 InputEventMouseButton.position 必须是窗口客户区坐标，中间隔着
// stretch（Example：320x180 视口 -> 1280x720 窗口 = 4x）与 letterbox 边距。
// 只测纯函数（godot::Transform2D/Vector2/Rect2 为值类型），不触碰引擎对象、
// 不调用 handler。

namespace {

// 等价于 Viewport::get_screen_transform() 在 4x stretch 窗口上的返回值。
godot::Transform2D stretch_transform(godot::real_t scale, godot::real_t margin_x,
                                     godot::real_t margin_y) {
  return godot::Transform2D(0.0, godot::Size2(scale, scale), 0.0,
                            godot::Vector2(margin_x, margin_y));
}

} // namespace

TEST(GameUiCoordsTest, StretchScaleMapsViewportPointToWindow) {
  // 320x180 视口 -> 1280x720 窗口：视口点 (160,105.5) 落在窗口 (640,422)。
  const godot::Transform2D screen = stretch_transform(4.0, 0.0, 0.0);
  const godot::Vector2 window =
      viewport_point_to_window(screen, godot::Vector2(160, 105.5));
  EXPECT_NEAR(640.0, window.x, 1e-3);
  EXPECT_NEAR(422.0, window.y, 1e-3);
}

TEST(GameUiCoordsTest, IdentityTransformKeepsViewportPoint) {
  // 无 transform（视口尺寸 == 窗口尺寸）时保持旧行为：原样返回。
  const godot::Vector2 point(123.25, -7.5);
  const godot::Vector2 window = viewport_point_to_window(godot::Transform2D(), point);
  EXPECT_NEAR(point.x, window.x, 1e-3);
  EXPECT_NEAR(point.y, window.y, 1e-3);
}

TEST(GameUiCoordsTest, LetterboxMarginAppliesAfterStretch) {
  // aspect 保留时窗口四周的 black bar：先按 4x 拉伸，再加边距偏移
  // （Window::_update_viewport_size 的 window_transform.translate_local(margin)）。
  const godot::Transform2D screen = stretch_transform(4.0, 10.0, 20.0);
  const godot::Vector2 window =
      viewport_point_to_window(screen, godot::Vector2(160, 105.5));
  EXPECT_NEAR(650.0, window.x, 1e-3);
  EXPECT_NEAR(442.0, window.y, 1e-3);
}

TEST(GameUiCoordsTest, OriginPointMapsToWindowOrigin) {
  // (0,0) 边界：无 letterbox 时窗口原点仍是 (0,0)，有边距时落到边距原点。
  EXPECT_NEAR(0.0, viewport_point_to_window(godot::Transform2D(),
                                            godot::Vector2(0, 0)).x,
              1e-3);
  const godot::Vector2 margined =
      viewport_point_to_window(stretch_transform(4.0, 10.0, 20.0), godot::Vector2(0, 0));
  EXPECT_NEAR(10.0, margined.x, 1e-3);
  EXPECT_NEAR(20.0, margined.y, 1e-3);
}

TEST(GameUiCoordsTest, CanvasTransformComposesBeforeScreenTransform) {
  // 调用点先复合 CanvasItem::get_canvas_transform()（CanvasLayer/Camera2D），
  // 再交给 screen transform：画布平移 (10,20) 与 4x 拉伸叠加。
  const godot::Transform2D canvas = godot::Transform2D(0.0, godot::Size2(1, 1), 0.0,
                                                       godot::Vector2(10, 20));
  const godot::Transform2D screen = stretch_transform(4.0, 0.0, 0.0) * canvas;
  const godot::Vector2 window =
      viewport_point_to_window(screen, godot::Vector2(160, 105.5));
  EXPECT_NEAR(680.0, window.x, 1e-3);
  EXPECT_NEAR(502.0, window.y, 1e-3);
}

TEST(GameUiCoordsTest, RectCenterUsesViewportRectCenter) {
  // 60x20 的按钮摆在视口中心 (160,90)：rect 中心即注入点，4x 后为 (640,360)。
  const godot::Rect2 rect(130, 80, 60, 20);
  const godot::Transform2D screen = stretch_transform(4.0, 0.0, 0.0);
  const godot::Vector2 window = viewport_rect_center_to_window(screen, rect);
  EXPECT_NEAR(640.0, window.x, 1e-3);
  EXPECT_NEAR(360.0, window.y, 1e-3);
  const godot::Vector2 center = viewport_rect_center_to_window(godot::Transform2D(), rect);
  EXPECT_NEAR(160.0, center.x, 1e-3);
  EXPECT_NEAR(90.0, center.y, 1e-3);
}
