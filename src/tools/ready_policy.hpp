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
                                         int active_session_count,
                                         bool wait_ready) {
  if (ready_session_count > 0)
    return ReadyDecision::SendNow;
  if (active_session_count <= 0)
    return ReadyDecision::FailNotRunning;
  return wait_ready ? ReadyDecision::QueueForReady
                    : ReadyDecision::FailNotReady;
}

} // namespace godot_autopilot

#endif
