#ifndef GODOT_AUTOPILOT_EDITOR_COORDS_HPP
#define GODOT_AUTOPILOT_EDITOR_COORDS_HPP

#include <cstdint>
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

struct ImageSize { int width = 0; int height = 0; };
ImageSize fit_within(ImageSize src, int max_dimension);

struct DiffResult { bool comparable = false; double changed_ratio = 0.0; bool has_bbox = false; Rect bbox{}; };
DiffResult diff_sample(const uint8_t *a, const uint8_t *b, int width, int height, int channels,
                       int sample_step, int channel_threshold);

struct Mark { double x = 0.0; double y = 0.0; };
std::vector<Mark> layout_marks(const std::vector<Rect> &rects);

} // namespace coords
} // namespace godot_autopilot

#endif
