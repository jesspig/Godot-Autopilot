#ifndef GODOT_AUTOPILOT_RUNTIME_ERROR_CURSOR_HPP
#define GODOT_AUTOPILOT_RUNTIME_ERROR_CURSOR_HPP

#include <cstdint>

namespace godot_autopilot {
namespace runtime {
namespace error_cursor {

inline bool error_seq_after(int64_t seq, int64_t since) {
  return seq > since;
}

inline int64_t next_error_seq(int64_t max_returned, int64_t current) {
  return max_returned > 0 ? max_returned : current;
}

} // namespace error_cursor
} // namespace runtime
} // namespace godot_autopilot

#endif
