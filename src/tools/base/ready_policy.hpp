#ifndef GODOT_AUTOPILOT_READY_POLICY_HPP
#define GODOT_AUTOPILOT_READY_POLICY_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace godot_autopilot {

enum class ReadyDecision {
  SendNow,
  QueueForReady,
  FailNotReady,
  FailNotRunning,
};

inline ReadyDecision decide_game_request(int ready_session_count,
                                         int scene_ready_session_count,
                                         int active_session_count,
                                         bool wait_ready) {
  if (ready_session_count > 0 && scene_ready_session_count > 0)
    return ReadyDecision::SendNow;
  if (active_session_count <= 0)
    return ReadyDecision::FailNotRunning;
  return wait_ready ? ReadyDecision::QueueForReady
                    : ReadyDecision::FailNotReady;
}

inline bool should_queue_for_ready(ReadyDecision decision, bool wait_ready,
                                   bool editor_playing_scene) {
  if (decision == ReadyDecision::QueueForReady)
    return true;
  return decision == ReadyDecision::FailNotRunning && wait_ready &&
         editor_playing_scene;
}

inline bool is_broadcast_eligible(bool session_ready, bool scene_ready) {
  return session_ready && scene_ready;
}

enum class SessionSelection {
  Selected,
  Ambiguous,
  InvalidSession,
  NotReady,
  NoneReady,
};

struct SessionSelectionResult {
  SessionSelection kind = SessionSelection::NoneReady;
  int32_t session_id = -1;
};

inline SessionSelectionResult select_game_session(
    const std::vector<int32_t> &ready_session_ids,
    const std::vector<int32_t> &active_session_ids,
    std::optional<int32_t> requested_session_id) {
  if (requested_session_id.has_value()) {
    const int32_t requested = *requested_session_id;
    if (std::find(ready_session_ids.begin(), ready_session_ids.end(),
                  requested) != ready_session_ids.end()) {
      return {SessionSelection::Selected, requested};
    }
    if (std::find(active_session_ids.begin(), active_session_ids.end(),
                  requested) != active_session_ids.end()) {
      return {SessionSelection::NotReady, -1};
    }
    return {SessionSelection::InvalidSession, -1};
  }
  if (ready_session_ids.size() == 1) {
    return {SessionSelection::Selected, ready_session_ids.front()};
  }
  if (ready_session_ids.size() > 1) {
    return {SessionSelection::Ambiguous, -1};
  }
  return {SessionSelection::NoneReady, -1};
}

} // namespace godot_autopilot

#endif
