#ifndef GODOT_AUTOPILOT_MODE_DETECTOR_HPP
#define GODOT_AUTOPILOT_MODE_DETECTOR_HPP

#include <godot_cpp/godot.hpp>

namespace godot_autopilot {

enum class RuntimeMode { Editor, Game, Unknown };

class ModeDetector {
public:
  static RuntimeMode detect();
  static bool is_editor();
};

} // namespace godot_autopilot

#endif
