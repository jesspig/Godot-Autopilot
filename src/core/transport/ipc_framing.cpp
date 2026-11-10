#include "core/transport/ipc_framing.hpp"

namespace godot_autopilot {
namespace ipc_framing {

ExtractedFrames extract_frames(const std::string &buffer,
                               std::size_t max_frame_bytes) {
  ExtractedFrames result;
  std::size_t start = 0;
  while (true) {
    const std::size_t newline = buffer.find('\n', start);
    if (newline == std::string::npos) {
      result.remainder = buffer.substr(start);
      break;
    }
    const std::size_t length = newline - start;
    if (length > max_frame_bytes) {
      result.overflow = true;
      result.remainder.clear();
      return result;
    }
    std::string frame = buffer.substr(start, length);
    if (!frame.empty() && frame.back() == '\r') {
      frame.pop_back();
    }
    if (!frame.empty()) {
      result.frames.push_back(std::move(frame));
    }
    start = newline + 1;
  }
  if (result.remainder.size() > max_frame_bytes) {
    result.overflow = true;
    result.remainder.clear();
  }
  return result;
}

} // namespace ipc_framing
} // namespace godot_autopilot