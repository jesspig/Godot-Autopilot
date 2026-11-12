#ifndef GODOT_AUTOPILOT_PORT_RESOLVER_HPP
#define GODOT_AUTOPILOT_PORT_RESOLVER_HPP

#include <string>
#include <vector>

namespace godot_autopilot {

enum class PortSource {
  kEnvExplicit,
  kConfigExplicit,
  kUserApplied,
  kFirstRandom,
  kAutoReassigned
};

const char *port_source_name(PortSource source);

struct PortAssignment {
  int port{0};
  PortSource source{PortSource::kFirstRandom};
  std::string note;
  int requested_port{0};
};

class PortResolver {
public:
  static PortAssignment resolve(int explicit_port, int preferred_port, int pid);
  static std::vector<int> candidate_sequence(int pid);
};

} // namespace godot_autopilot

#endif
