#ifndef GODOT_AUTOPILOT_EDITOR_COORDS_HPP
#define GODOT_AUTOPILOT_EDITOR_COORDS_HPP

#include <cstdint>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot_autopilot {
namespace coords {

godot::Vector2 viewport_point_to_window(const godot::Transform2D &screen_transform,
                                        const godot::Vector2 &viewport_point);
godot::Vector2 viewport_rect_center_to_window(const godot::Transform2D &screen_transform,
                                              const godot::Rect2 &viewport_rect);

struct ImageSize { int width = 0; int height = 0; };
ImageSize fit_within(ImageSize src, int max_dimension);
ImageSize scale_size(ImageSize src, int scale);

struct Rect { double x = 0.0; double y = 0.0; double w = 0.0; double h = 0.0; };
struct DiffResult { bool comparable = false; double changed_ratio = 0.0; bool has_bbox = false; Rect bbox{}; };
DiffResult diff_sample(const uint8_t *a, const uint8_t *b, int width, int height, int channels,
                       int sample_step, int channel_threshold);

} // namespace coords
} // namespace godot_autopilot

#endif
