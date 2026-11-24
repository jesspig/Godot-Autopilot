#ifndef GODOT_AUTOPILOT_PERSISTENCE_INTEGRITY_HPP
#define GODOT_AUTOPILOT_PERSISTENCE_INTEGRITY_HPP

namespace godot_autopilot {
namespace persistence {

enum class PersistenceState { kNotApplied, kMemoryOnly, kPersisted };

inline PersistenceState persistence_state(bool applied_in_memory,
                                          bool save_succeeded) {
  if (!applied_in_memory)
    return PersistenceState::kNotApplied;
  return save_succeeded ? PersistenceState::kPersisted
                        : PersistenceState::kMemoryOnly;
}

inline bool is_false_confirmation(PersistenceState state) {
  return state == PersistenceState::kMemoryOnly;
}

} // namespace persistence
} // namespace godot_autopilot

#endif
