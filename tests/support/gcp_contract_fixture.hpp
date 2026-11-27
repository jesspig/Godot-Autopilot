#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace gcp {
namespace fixture {

std::string make_traceparent(unsigned int seed);

GcpEnvelope make_request(const std::string &method, const std::string &id,
                         const std::string &instance_id);

GcpEnvelope make_response(const GcpEnvelope &request, Status status,
                          const std::string &response_id);

GcpEnvelope make_event(const std::string &instance_id, std::uint64_t seq,
                       const std::string &type);

std::vector<TaskState> drive_task_lifecycle();

std::vector<CancelStage> drive_cancel_stages();

std::vector<std::uint64_t> drive_event_seq(std::uint64_t start,
                                           std::size_t count);

bool cursor_valid_for(const EventCursor &cursor, std::uint64_t current_seq);

} // namespace fixture
} // namespace gcp
} // namespace gda
