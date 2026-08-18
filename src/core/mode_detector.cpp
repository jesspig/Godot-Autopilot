#include "mode_detector.hpp"

#include <godot_cpp/classes/engine.hpp>

namespace godot_autopilot {

RuntimeMode ModeDetector::detect() {
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    return RuntimeMode::Unknown;
  }
  return engine->is_editor_hint() ? RuntimeMode::Editor : RuntimeMode::Game;
}

bool ModeDetector::is_editor() { return detect() == RuntimeMode::Editor; }

} // namespace godot_autopilot
