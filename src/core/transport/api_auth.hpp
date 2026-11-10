#ifndef GODOT_AUTOPILOT_API_AUTH_HPP
#define GODOT_AUTOPILOT_API_AUTH_HPP

#include <string>
#include <unordered_map>

namespace godot_autopilot {

class ApiAuth {
public:
  std::string load_token() const;
  std::string issue_token() const;
  bool verify(const std::unordered_map<std::string, std::string> &headers) const;
};

} // namespace godot_autopilot

#endif
