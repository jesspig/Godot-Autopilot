#ifndef GODOT_AUTOPILOT_GAME_MONITOR_HPP
#define GODOT_AUTOPILOT_GAME_MONITOR_HPP

#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace game_monitor {

struct Status {
  bool connected = false;
  bool active = false;
  std::string session_id;
  std::string path;
  uint64_t frames = 0;
  uint64_t custom_count = 0;
};

Status status();

void ensure_connected();

void stop();

std::string sessions_dir();

} // namespace game_monitor
} // namespace godot_autopilot

#endif
