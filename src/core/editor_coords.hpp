#ifndef GODOT_AUTOPILOT_EDITOR_COORDS_HPP
#define GODOT_AUTOPILOT_EDITOR_COORDS_HPP

#include <cstdint>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <vector>

namespace godot_autopilot {
namespace coords {

struct Point { double x = 0.0; double y = 0.0; };
struct Size { double w = 0.0; double h = 0.0; };
struct Rect { double x = 0.0; double y = 0.0; double w = 0.0; double h = 0.0; };
struct Affine { double xx = 1.0, xy = 0.0, yx = 0.0, yy = 1.0, ox = 0.0, oy = 0.0; };

Affine make_affine(double xx, double xy, double yx, double yy, double ox, double oy);
Affine compose(const Affine &outer, const Affine &inner);
Point transform_point(const Affine &a, const Point &p);
bool invert(const Affine &a, Affine &out);
Rect transform_rect(const Affine &a, const Rect &r);
bool rect_contains(const Rect &r, const Point &p);
Rect rect_intersect(const Rect &a, const Rect &b);

// 画布空间（Control::get_global_rect()/get_global_transform() 所在空间）→ 窗口
// 客户区坐标。screen_transform = Viewport::get_screen_transform()（拉伸 +
// letterbox 边距）复合 CanvasItem::get_canvas_transform()（默认画布的 Camera2D
// 或 CanvasLayer 层变换）；注入的 InputEventMouseButton.position 正是窗口客户区
// 坐标（引擎用 viewport.cpp:_make_input_local 的 get_final_transform() 反变换）。
// 纯函数，无副作用：恒等变换原样返回。
godot::Vector2 viewport_point_to_window(const godot::Transform2D &screen_transform,
                                        const godot::Vector2 &viewport_point);
godot::Vector2 viewport_rect_center_to_window(const godot::Transform2D &screen_transform,
                                              const godot::Rect2 &viewport_rect);

struct ImageSize { int width = 0; int height = 0; };
ImageSize fit_within(ImageSize src, int max_dimension);
ImageSize scale_size(ImageSize src, int scale);

struct DiffResult { bool comparable = false; double changed_ratio = 0.0; bool has_bbox = false; Rect bbox{}; };
DiffResult diff_sample(const uint8_t *a, const uint8_t *b, int width, int height, int channels,
                       int sample_step, int channel_threshold);

struct Mark { double x = 0.0; double y = 0.0; };
std::vector<Mark> layout_marks(const std::vector<Rect> &rects);

} // namespace coords
} // namespace godot_autopilot

#endif
