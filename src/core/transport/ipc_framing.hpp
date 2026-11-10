#ifndef GODOT_AUTOPILOT_IPC_FRAMING_HPP
#define GODOT_AUTOPILOT_IPC_FRAMING_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace ipc_framing {

constexpr std::size_t kMaxFrameBytes = 4 * 1024 * 1024;

struct ExtractedFrames {
  std::vector<std::string> frames;
  std::string remainder;
  bool overflow{false};
};

ExtractedFrames extract_frames(
    const std::string &buffer,
    std::size_t max_frame_bytes = kMaxFrameBytes);

} // namespace ipc_framing
} // namespace godot_autopilot

#endif