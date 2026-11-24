#ifndef GODOT_AUTOPILOT_RENDER_BUDGET_HPP
#define GODOT_AUTOPILOT_RENDER_BUDGET_HPP

#include <cstdint>

namespace godot_autopilot {
namespace render_budget {

inline constexpr int64_t kMinViewportDimension = 1;
inline constexpr int64_t kMaxViewportDimension = 8192;
inline constexpr int64_t kMaxViewportPixels = 16777216;

enum class ViewportSizeFailure {
  kNone,
  kInvalidDimension,
  kDimensionLimit,
  kPixelLimit,
};

struct ViewportSizeVerdict {
  ViewportSizeFailure failure = ViewportSizeFailure::kNone;
  int32_t width = 0;
  int32_t height = 0;
};

inline ViewportSizeVerdict validate_viewport_size(int64_t width,
                                                  int64_t height) {
  ViewportSizeVerdict verdict;
  if (width < kMinViewportDimension || height < kMinViewportDimension) {
    verdict.failure = ViewportSizeFailure::kInvalidDimension;
    return verdict;
  }
  if (width > kMaxViewportDimension || height > kMaxViewportDimension) {
    verdict.failure = ViewportSizeFailure::kDimensionLimit;
    return verdict;
  }
  const uint64_t pixels = static_cast<uint64_t>(width) *
                          static_cast<uint64_t>(height);
  if (pixels > static_cast<uint64_t>(kMaxViewportPixels)) {
    verdict.failure = ViewportSizeFailure::kPixelLimit;
    return verdict;
  }
  verdict.width = static_cast<int32_t>(width);
  verdict.height = static_cast<int32_t>(height);
  return verdict;
}

} // namespace render_budget
} // namespace godot_autopilot

#endif
