#ifndef GODOT_AUTOPILOT_RUNTIME_BUDGET_HPP
#define GODOT_AUTOPILOT_RUNTIME_BUDGET_HPP

#include <cstddef>
#include <cstdint>

namespace godot_autopilot {
namespace runtime_budget {

inline int64_t remaining_timeout_ms(int64_t budget_ms, int64_t elapsed_ms) {
  if (budget_ms <= 0)
    return 0;
  if (elapsed_ms <= 0)
    return budget_ms;
  if (elapsed_ms >= budget_ms)
    return 0;
  return budget_ms - elapsed_ms;
}

enum class SequencePlanFailure {
  kNone,
  kTooManyItems,
  kStepTooLong,
  kTotalTooLong,
};

struct SequencePlanVerdict {
  SequencePlanFailure failure = SequencePlanFailure::kNone;
  int64_t timeout_ms = 0;
};

inline SequencePlanVerdict validate_sequence_plan(
    size_t item_count, int64_t max_step_ms, int64_t computed_timeout_ms,
    size_t max_items, int64_t max_step_limit_ms, int64_t max_timeout_ms) {
  SequencePlanVerdict verdict;
  if (item_count == 0 || item_count > max_items) {
    verdict.failure = SequencePlanFailure::kTooManyItems;
    return verdict;
  }
  if (max_step_ms > max_step_limit_ms) {
    verdict.failure = SequencePlanFailure::kStepTooLong;
    return verdict;
  }
  if (computed_timeout_ms <= 0 || computed_timeout_ms > max_timeout_ms) {
    verdict.failure = SequencePlanFailure::kTotalTooLong;
    return verdict;
  }
  verdict.timeout_ms = computed_timeout_ms;
  return verdict;
}

inline bool tween_capacity_available(size_t current, size_t limit) {
  return current < limit;
}

} // namespace runtime_budget
} // namespace godot_autopilot

#endif
