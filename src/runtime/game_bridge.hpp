#ifndef GODOT_SELF_DRIVING_RUNTIME_GAME_BRIDGE_HPP
#define GODOT_SELF_DRIVING_RUNTIME_GAME_BRIDGE_HPP

namespace godot_self_driving {
namespace runtime {
namespace game_bridge {

// Register the in-game message capture ("gsd") so the editor can reach a running
// game process through the engine debugger channel. Only meaningful in the
// player process (editor process uses DebugCapturePlugin instead).
void register_listener();

// Unregister the in-game message capture. Safe to call even if not registered.
void unregister_listener();

} // namespace game_bridge
} // namespace runtime
} // namespace godot_self_driving

#endif
