#include "core/transport/port_resolver.hpp"

#include <algorithm>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/transport/port_resolver_ext.hpp"

namespace godot_autopilot {

int port_auto_candidate_count() {
  return GDA_PORT_CANDIDATE_LIMIT - GDA_PORT_CANDIDATE_BASE + 1;
}

const char *port_source_name(PortSource source) {
  switch (source) {
    case PortSource::kEnvExplicit:
      return "env";
    case PortSource::kConfigExplicit:
      return "persisted";
    case PortSource::kUserApplied:
      return "persisted";
    case PortSource::kFirstRandom:
      return "first_random";
    case PortSource::kAutoReassigned:
      return "auto_reassigned";
  }
  return "first_random";
}

std::vector<int> PortResolver::candidate_sequence(int pid) {
  const int count = port_auto_candidate_count();
  std::vector<int> sequence;
  sequence.reserve(static_cast<std::size_t>(count));
  for (int offset = 0; offset < count; ++offset) {
    sequence.push_back(GDA_PORT_CANDIDATE_BASE + offset);
  }
  std::random_device device;
  const unsigned normalized_pid =
      pid > 0 ? static_cast<unsigned>(pid) : 0u;
  std::seed_seq seed{device(), device(), normalized_pid};
  std::mt19937 generator(seed);
  std::shuffle(sequence.begin(), sequence.end(), generator);
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
  assignment.source = PortSource::kFirstRandom;
  assignment.port = candidate_sequence(pid).front();
  assignment.requested_port = 0;
  assignment.note = "auto assigned: random candidate in " +
                    std::to_string(GDA_PORT_CANDIDATE_BASE) + "-" +
                    std::to_string(GDA_PORT_CANDIDATE_LIMIT);
  return assignment;
}

} // namespace godot_autopilot
