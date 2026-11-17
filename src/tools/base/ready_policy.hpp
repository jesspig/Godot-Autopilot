#ifndef GODOT_AUTOPILOT_READY_POLICY_HPP
#define GODOT_AUTOPILOT_READY_POLICY_HPP

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

} // namespace godot_autopilot

#endif
