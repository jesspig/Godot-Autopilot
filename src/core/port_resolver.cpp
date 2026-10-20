#include "core/port_resolver.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/port_resolver_ext.hpp"

namespace godot_autopilot {

int port_auto_candidate_count() {
  return GDA_PORT_CANDIDATE_LIMIT - GDA_PORT_CANDIDATE_BASE + 1;
}

int port_auto_candidate_start(int pid) {
  const int count = port_auto_candidate_count();
  if (count <= 0) {
    return GDA_PORT_CANDIDATE_BASE;
  }
  const int normalized = pid > 0 ? pid : 0;
  return GDA_PORT_CANDIDATE_BASE + (normalized % count);
}

std::vector<int> PortResolver::candidate_sequence(int pid) {
  const int count = port_auto_candidate_count();
  std::vector<int> sequence;
  sequence.reserve(static_cast<size_t>(count) + 1);
  sequence.push_back(GDA_DEFAULT_PORT);
  const int start = port_auto_candidate_start(pid);
  for (int offset = 0; offset < count; ++offset) {
    int index = start - GDA_PORT_CANDIDATE_BASE + offset;
    if (index >= count) {
      index -= count;
    }
    sequence.push_back(GDA_PORT_CANDIDATE_BASE + index);
  }
  return sequence;
}

PortAssignment PortResolver::resolve(int explicit_port, int preferred_port,
                                     int pid) {
  PortAssignment assignment;
  if (explicit_port > 0) {
    assignment.port = explicit_port;
    assignment.source = PortSource::kEnvExplicit;
    assignment.requested_port = explicit_port;
    assignment.note.clear();
    return assignment;
  }
  if (preferred_port > 0) {
    assignment.port = preferred_port;
    assignment.source = PortSource::kConfigExplicit;
    assignment.requested_port = preferred_port;
    assignment.note.clear();
    return assignment;
  }
  assignment.source = PortSource::kAutoAssigned;
  assignment.port = candidate_sequence(pid).front();
  assignment.requested_port = 0;
  assignment.note = "auto assigned: preferred " + std::to_string(GDA_DEFAULT_PORT) +
                    ", fallback candidates " +
                    std::to_string(GDA_PORT_CANDIDATE_BASE) + "-" +
                    std::to_string(GDA_PORT_CANDIDATE_LIMIT);
  return assignment;
}

} // namespace godot_autopilot
