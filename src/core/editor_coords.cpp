#include "editor_coords.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace godot_autopilot {
namespace coords {

namespace {

constexpr int kScaleMin = 1;
constexpr int kScaleMax = 8;
constexpr int64_t kImageMaxSide = 16777216;
constexpr int64_t kImageMaxPixels = 268435456;

} // namespace

godot::Vector2 viewport_point_to_window(const godot::Transform2D &screen_transform,
                                        const godot::Vector2 &viewport_point) {
  return screen_transform.xform(viewport_point);
}

godot::Vector2 viewport_rect_center_to_window(const godot::Transform2D &screen_transform,
                                              const godot::Rect2 &viewport_rect) {
  return viewport_point_to_window(screen_transform, viewport_rect.get_center());
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

ImageSize scale_size(ImageSize src, int scale) {
  if (src.width <= 0 || src.height <= 0)
    return ImageSize{};
  if (scale < kScaleMin || scale > kScaleMax)
    return ImageSize{};
  const int64_t width = static_cast<int64_t>(src.width) * scale;
  const int64_t height = static_cast<int64_t>(src.height) * scale;
  if (width > kImageMaxSide || height > kImageMaxSide)
    return ImageSize{};
  if (width * height > kImageMaxPixels)
    return ImageSize{};
  return ImageSize{static_cast<int>(width), static_cast<int>(height)};
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

} // namespace coords
} // namespace godot_autopilot
