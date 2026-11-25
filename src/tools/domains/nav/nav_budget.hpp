#ifndef GODOT_AUTOPILOT_NAV_BUDGET_HPP
#define GODOT_AUTOPILOT_NAV_BUDGET_HPP

#include <cmath>
#include <cstdint>

namespace godot_autopilot {
namespace nav_budget {

inline constexpr double kMaxGridDimension = 2048.0;
inline constexpr double kMaxGridCells = 1048576.0;

enum class GridExtentFailure {
  kNone,
  kNonFinite,
  kNotIntegral,
  kOutOfRange,
  kDimensionLimit,
  kAreaLimit,
};

struct GridExtentVerdict {
  GridExtentFailure failure = GridExtentFailure::kNone;
  int32_t width = 0;
  int32_t height = 0;
};

inline GridExtentVerdict validate_grid_extent(double width, double height) {
  GridExtentVerdict verdict;
  if (!std::isfinite(width) || !std::isfinite(height)) {
    verdict.failure = GridExtentFailure::kNonFinite;
    return verdict;
  }
  if (width < 1.0 || height < 1.0) {
    verdict.failure = GridExtentFailure::kOutOfRange;
    return verdict;
  }
  if (width > kMaxGridDimension || height > kMaxGridDimension) {
    verdict.failure = GridExtentFailure::kDimensionLimit;
    return verdict;
  }
  if (std::floor(width) != width || std::floor(height) != height) {
    verdict.failure = GridExtentFailure::kNotIntegral;
    return verdict;
  }
  if (width * height > kMaxGridCells) {
    verdict.failure = GridExtentFailure::kAreaLimit;
    return verdict;
  }
  verdict.width = static_cast<int32_t>(width);
  verdict.height = static_cast<int32_t>(height);
  return verdict;
}

} // namespace nav_budget
} // namespace godot_autopilot

#endif
