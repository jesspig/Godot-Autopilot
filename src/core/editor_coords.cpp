#include "editor_coords.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace godot_autopilot {
namespace coords {

Affine make_affine(double xx, double xy, double yx, double yy, double ox, double oy) {
  Affine a;
  a.xx = xx;
  a.xy = xy;
  a.yx = yx;
  a.yy = yy;
  a.ox = ox;
  a.oy = oy;
  return a;
}

Affine compose(const Affine &outer, const Affine &inner) {
  Affine result;
  result.xx = outer.xx * inner.xx + outer.xy * inner.yx;
  result.xy = outer.xx * inner.xy + outer.xy * inner.yy;
  result.yx = outer.yx * inner.xx + outer.yy * inner.yx;
  result.yy = outer.yx * inner.xy + outer.yy * inner.yy;
  result.ox = outer.xx * inner.ox + outer.xy * inner.oy + outer.ox;
  result.oy = outer.yx * inner.ox + outer.yy * inner.oy + outer.oy;
  return result;
}

Point transform_point(const Affine &a, const Point &p) {
  Point result;
  result.x = a.xx * p.x + a.xy * p.y + a.ox;
  result.y = a.yx * p.x + a.yy * p.y + a.oy;
  return result;
}

bool invert(const Affine &a, Affine &out) {
  const double det = a.xx * a.yy - a.xy * a.yx;
  if (std::fabs(det) < 1e-9)
    return false;
  out.xx = a.yy / det;
  out.xy = -a.xy / det;
  out.yx = -a.yx / det;
  out.yy = a.xx / det;
  out.ox = (a.xy * a.oy - a.yy * a.ox) / det;
  out.oy = (a.yx * a.ox - a.xx * a.oy) / det;
  return true;
}

Rect transform_rect(const Affine &a, const Rect &r) {
  const Point corners[4] = {
      transform_point(a, Point{r.x, r.y}),
      transform_point(a, Point{r.x + r.w, r.y}),
      transform_point(a, Point{r.x, r.y + r.h}),
      transform_point(a, Point{r.x + r.w, r.y + r.h}),
  };
  double min_x = corners[0].x;
  double max_x = corners[0].x;
  double min_y = corners[0].y;
  double max_y = corners[0].y;
  for (int i = 1; i < 4; ++i) {
    min_x = std::min(min_x, corners[i].x);
    max_x = std::max(max_x, corners[i].x);
    min_y = std::min(min_y, corners[i].y);
    max_y = std::max(max_y, corners[i].y);
  }
  Rect result;
  result.x = min_x;
  result.y = min_y;
  result.w = max_x - min_x;
  result.h = max_y - min_y;
  return result;
}

bool rect_contains(const Rect &r, const Point &p) {
  return p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
}

Rect rect_intersect(const Rect &a, const Rect &b) {
  const double x0 = std::max(a.x, b.x);
  const double y0 = std::max(a.y, b.y);
  const double x1 = std::min(a.x + a.w, b.x + b.w);
  const double y1 = std::min(a.y + a.h, b.y + b.h);
  if (x1 <= x0 || y1 <= y0)
    return Rect{};
  Rect result;
  result.x = x0;
  result.y = y0;
  result.w = x1 - x0;
  result.h = y1 - y0;
  return result;
}

ImageSize fit_within(ImageSize src, int max_dimension) {
  if (max_dimension <= 0)
    return src;
  if (src.width <= 0 || src.height <= 0)
    return src;
  const int longest = std::max(src.width, src.height);
  if (longest <= max_dimension)
    return src;
  ImageSize result;
  result.width = static_cast<int>(static_cast<int64_t>(src.width) * max_dimension / longest);
  result.height = static_cast<int>(static_cast<int64_t>(src.height) * max_dimension / longest);
  if (result.width < 1)
    result.width = 1;
  if (result.height < 1)
    result.height = 1;
  return result;
}

DiffResult diff_sample(const uint8_t *a, const uint8_t *b, int width, int height, int channels,
                       int sample_step, int channel_threshold) {
  DiffResult result;
  if (!a || !b || width <= 0 || height <= 0 || channels <= 0)
    return result;
  if (sample_step <= 0)
    sample_step = 1;
  const int64_t stride = static_cast<int64_t>(width) * channels;
  int64_t sampled_count = 0;
  int64_t changed_count = 0;
  int min_x = width;
  int min_y = height;
  int max_x = -1;
  int max_y = -1;
  for (int y = 0; y < height; y += sample_step) {
    for (int x = 0; x < width; x += sample_step) {
      ++sampled_count;
      const int64_t offset =
          static_cast<int64_t>(y) * stride + static_cast<int64_t>(x) * channels;
      bool changed = false;
      for (int c = 0; c < channels; ++c) {
        const int diff =
            std::abs(static_cast<int>(a[offset + c]) - static_cast<int>(b[offset + c]));
        if (diff > channel_threshold) {
          changed = true;
          break;
        }
      }
      if (!changed)
        continue;
      ++changed_count;
      min_x = std::min(min_x, x);
      max_x = std::max(max_x, x);
      min_y = std::min(min_y, y);
      max_y = std::max(max_y, y);
    }
  }
  if (sampled_count == 0)
    return result;
  result.comparable = true;
  result.changed_ratio = static_cast<double>(changed_count) / static_cast<double>(sampled_count);
  if (changed_count > 0) {
    result.has_bbox = true;
    result.bbox.x = static_cast<double>(min_x);
    result.bbox.y = static_cast<double>(min_y);
    result.bbox.w = static_cast<double>(max_x - min_x + 1);
    result.bbox.h = static_cast<double>(max_y - min_y + 1);
  }
  return result;
}

std::vector<Mark> layout_marks(const std::vector<Rect> &rects) {
  std::vector<Mark> marks;
  marks.reserve(rects.size());
  for (const Rect &r : rects) {
    Mark mark;
    mark.x = r.x + 2.0;
    mark.y = r.y + 8.0;
    int shifts = 0;
    while (shifts < 50) {
      bool conflict = false;
      for (const Mark &placed : marks) {
        if (std::fabs(mark.x - placed.x) < 10.0 && std::fabs(mark.y - placed.y) < 10.0) {
          conflict = true;
          break;
        }
      }
      if (!conflict)
        break;
      mark.y += 10.0;
      ++shifts;
    }
    marks.push_back(mark);
  }
  return marks;
}

} // namespace coords
} // namespace godot_autopilot
