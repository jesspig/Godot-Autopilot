
//

#include "core/command_queue.hpp"
#include "tools/runtime_ops.hpp"

namespace godot_self_driving {

CommandQueue &get_editor_queue() {
  static CommandQueue stub_queue;
  return stub_queue;
}

} // namespace godot_self_driving
