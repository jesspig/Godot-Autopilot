#ifndef GODOT_AUTOPILOT_TRANSPORT_LIFECYCLE_HPP
#define GODOT_AUTOPILOT_TRANSPORT_LIFECYCLE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace godot_autopilot {
namespace transport_lifecycle {

enum class WorkerPhase {
  kRunning,
  kExecuting,
  kFinished,
};

enum class ShutdownMode {
  kGraceful,
  kFinal,
};

enum class StopOutcome {
  kKeepWaiting,
  kJoin,
  kPark,
  kJoinBlocking,
};

inline WorkerPhase worker_phase(bool finished, bool executing) {
  if (finished) {
    return WorkerPhase::kFinished;
  }
  if (executing) {
    return WorkerPhase::kExecuting;
  }
  return WorkerPhase::kRunning;
}

inline bool worker_accepts_job(bool stop_requested) {
  return !stop_requested;
}

inline bool worker_budget_left(int64_t elapsed_ms, int64_t budget_ms) {
  return elapsed_ms < budget_ms;
}

inline StopOutcome worker_stop_outcome(ShutdownMode mode, WorkerPhase phase,
                                       int64_t elapsed_ms, int64_t budget_ms) {
  if (phase == WorkerPhase::kFinished) {
    return StopOutcome::kJoin;
  }
  if (worker_budget_left(elapsed_ms, budget_ms)) {
    return StopOutcome::kKeepWaiting;
  }
  if (mode == ShutdownMode::kFinal) {
    return StopOutcome::kJoinBlocking;
  }
  return StopOutcome::kPark;
}

inline bool should_reap(bool worker_finished) {
  return worker_finished;
}

inline size_t count_reapable(const std::vector<bool> &finished_flags) {
  size_t count = 0;
  for (bool finished : finished_flags) {
    if (finished) {
      ++count;
    }
  }
  return count;
}

inline size_t remaining_after_reap(size_t total, size_t reapable) {
  return total > reapable ? total - reapable : 0;
}

} // namespace transport_lifecycle
} // namespace godot_autopilot

#endif
