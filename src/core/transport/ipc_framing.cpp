#include "core/transport/ipc_framing.hpp"

#include <cstdint>

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

ExtractedFrames extract_length_prefixed(const std::string &buffer,
                                        std::size_t max_frame_bytes) {
  ExtractedFrames result;
  std::size_t offset = 0;
  while (buffer.size() - offset >= 4) {
    const std::uint32_t size =
        (static_cast<std::uint32_t>(
             static_cast<unsigned char>(buffer[offset])) << 24) |
        (static_cast<std::uint32_t>(
             static_cast<unsigned char>(buffer[offset + 1])) << 16) |
        (static_cast<std::uint32_t>(
             static_cast<unsigned char>(buffer[offset + 2])) << 8) |
        static_cast<std::uint32_t>(
            static_cast<unsigned char>(buffer[offset + 3]));
    if (size > max_frame_bytes) {
      result.overflow = true;
      result.remainder.clear();
      return result;
    }
    if (buffer.size() - offset - 4 < size) {
      break;
    }
    result.frames.emplace_back(buffer, offset + 4, size);
    offset += 4 + size;
  }
  result.remainder = buffer.substr(offset);
  return result;
}

std::string encode_length_prefixed(const std::string &payload) {
  const std::uint32_t size = static_cast<std::uint32_t>(payload.size());
  std::string frame;
  frame.reserve(4 + payload.size());
  frame.push_back(static_cast<char>((size >> 24) & 0xFFu));
  frame.push_back(static_cast<char>((size >> 16) & 0xFFu));
  frame.push_back(static_cast<char>((size >> 8) & 0xFFu));
  frame.push_back(static_cast<char>(size & 0xFFu));
  frame.append(payload);
  return frame;
}

} // namespace ipc_framing
} // namespace godot_autopilot