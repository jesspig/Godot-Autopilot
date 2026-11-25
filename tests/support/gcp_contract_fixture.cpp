#include "support/gcp_contract_fixture.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gda {
namespace gcp {
namespace fixture {

std::string make_traceparent(unsigned int seed) {
  static const char hex[] = "0123456789abcdef";
  unsigned int state = seed == 0 ? 1u : seed;
  std::string out = "00-";
  for (int i = 0; i < 32; ++i) {
    state = state * 1664525u + 1013904223u;
    out.push_back(hex[(state >> 28) & 0xFu]);
  }
  out.push_back('-');
  for (int i = 0; i < 16; ++i) {
    state = state * 1664525u + 1013904223u;
    out.push_back(hex[(state >> 28) & 0xFu]);
  }
  out += "-01";
  return out;
}

GcpEnvelope make_request(const std::string &method, const std::string &id,
                         const std::string &instance_id) {
  GcpEnvelope envelope;
  envelope.v = kEnvelopeVersion;
  envelope.kind = Kind::request;
  envelope.id = id;
  envelope.instance_id = instance_id;
  envelope.method = method;
  envelope.params = {{"tool", "scene"}, {"action", "inspect"}};
  envelope.trace.traceparent = make_traceparent(42);
  envelope.deadline_ms = kDefaultDeadlineMs;
  return envelope;
}

GcpEnvelope make_response(const GcpEnvelope &request, Status status,
                          const std::string &response_id) {
  GcpEnvelope envelope;
  envelope.v = request.v;
  envelope.kind = Kind::response;
  envelope.id = response_id;
  envelope.instance_id = request.instance_id;
  envelope.method = request.method;
  envelope.params = request.params;
  envelope.trace = request.trace;
  envelope.deadline_ms = request.deadline_ms;
  envelope.request_id = request.id;
  envelope.status = status;
  if (status == Status::error) {
    envelope.error = ErrorCode::result_unknown;
    envelope.error_message = "result_unknown";
  }
  if (status == Status::needs_confirmation) {
    envelope.confirmation = make_challenge(request.params, 60000, 1000, "nonce-1");
  }
  return envelope;
}

GcpEnvelope make_event(const std::string &instance_id, std::uint64_t seq,
                       const std::string &type) {
  GcpEnvelope envelope;
  envelope.v = kEnvelopeVersion;
  envelope.kind = Kind::event;
  envelope.id = "event-" + std::to_string(seq);
  envelope.instance_id = instance_id;
  envelope.event_type = type;
  envelope.seq = seq;
  envelope.trace.traceparent = make_traceparent(7);
  return envelope;
}

std::vector<TaskState> drive_task_lifecycle() {
  std::vector<TaskState> visited;
  std::vector<TaskState> queue = {TaskState::pending};
  while (!queue.empty()) {
    const TaskState current = queue.front();
    queue.erase(queue.begin());
    bool present = false;
    for (TaskState state : visited) {
      if (state == current) {
        present = true;
        break;
      }
    }
    if (!present) {
      visited.push_back(current);
    }
    for (int index = 0; index <= 6; ++index) {
      const TaskState next = static_cast<TaskState>(index);
      if (!can_transition(current, next)) {
        continue;
      }
      bool seen = false;
      for (TaskState state : visited) {
        if (state == next) {
          seen = true;
          break;
        }
      }
      for (TaskState state : queue) {
        if (state == next) {
          seen = true;
          break;
        }
      }
      if (!seen) {
        queue.push_back(next);
      }
    }
  }
  return visited;
}

std::vector<CancelStage> drive_cancel_stages() {
  std::vector<CancelStage> stages = {CancelStage::requested,
                                     CancelStage::acknowledged,
                                     CancelStage::draining,
                                     CancelStage::completed};
  return stages;
}

std::vector<std::uint64_t> drive_event_seq(std::uint64_t start,
                                           std::size_t count) {
  std::vector<std::uint64_t> sequence;
  sequence.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    sequence.push_back(start + static_cast<std::uint64_t>(index));
  }
  return sequence;
}

bool cursor_valid_for(const EventCursor &cursor, std::uint64_t current_seq) {
  return !cursor.instance_id.empty() && cursor.seq <= current_seq;
}

} // namespace fixture
} // namespace gcp
} // namespace gda
