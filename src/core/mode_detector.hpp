#ifndef GODOT_SELF_DRIVING_MODE_DETECTOR_HPP
#define GODOT_SELF_DRIVING_MODE_DETECTOR_HPP

#include <godot_cpp/godot.hpp>

namespace godot_self_driving {

enum class RuntimeMode { Editor, Game, Unknown };

class ModeDetector {
public:
  static RuntimeMode detect();
  static bool is_editor();
};

} // namespace godot_self_driving

#endif
