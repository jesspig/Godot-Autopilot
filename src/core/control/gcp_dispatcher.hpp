#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/control/control_service.hpp"
#include "core/control/invocation_service.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {

enum class DispatchState {
  idle,
  auth_ignored,
  decode_failed,
  validation_failed,
  duplicate_rejected,
  dispatched,
  awaiting,
  terminal,
};

struct OutboxEntry {
  std::uint64_t connection_id = 0;
  std::string frame;
};

struct DispatchResult {
  DispatchState state = DispatchState::idle;
  std::uint64_t connection_id = 0;
  std::string response_text;
  gcp::Status status = gcp::Status::error;
  gcp::ErrorCode error = gcp::ErrorCode::none;
  std::string error_message;
  std::string request_id;
  std::string task_id;
  bool close_connection = false;
};

class GcpDispatcher {
public:
  static constexpr std::size_t kEventOutboxSoftLimit = 64;
  static constexpr std::size_t kEventOutboxHardLimit = 256;

  GcpDispatcher(ControlService &service, const std::string &instance_id,
                InvocationSink *sink = nullptr);

  void set_sink(InvocationSink *sink);
  void set_instance_id(const std::string &instance_id);

  static bool looks_like_auth_frame(const std::string &text);
  static bool looks_like_gcp(const std::string &text);

  static bool encode(const gcp::GcpEnvelope &envelope, std::string &out);
  static bool decode(const std::string &text, gcp::GcpEnvelope &out,
                     std::string &error);

  DispatchResult handle_frame(std::uint64_t connection_id,
                              const std::string &text);

  bool publish_event(const std::string &event_type,
                     const std::string &payload, std::uint64_t connection_id,
                     const std::string &traceparent = std::string(),
                     const std::string &task_id = std::string(),
                     const std::string &request_id = std::string());
  bool enqueue_event(const gcp::GcpEnvelope &event,
                     std::uint64_t connection_id);
  std::size_t pending_events() const;
  gcp::Backpressure event_pressure() const;
  std::vector<OutboxEntry> drain_events();

  void mark_written(const std::string &request_id);
  bool was_written(const std::string &request_id) const;
  std::size_t completed_count() const;
  DispatchState state() const;
  void reset();

  ControlService &service();
  const ControlService &service() const;

private:
  DispatchResult fail(const gcp::GcpEnvelope &request, gcp::ErrorCode code,
                      const std::string &message, DispatchState state);
  DispatchResult complete(const gcp::GcpEnvelope &request, gcp::Status status,
                          const std::string &result_json,
                          gcp::ErrorCode error = gcp::ErrorCode::none,
                          const std::string &message = std::string(),
                          const gcp::Challenge &confirmation = {},
                          const std::string &task_id = std::string());
  DispatchResult dispatch_request(std::uint64_t connection_id,
                                  const gcp::GcpEnvelope &request);
  gcp::GcpEnvelope response_envelope(const gcp::GcpEnvelope &request,
                                     gcp::Status status) const;

  ControlService &service_;
  std::string instance_id_;
  InvocationSink *sink_;
  std::map<std::string, InvocationRequest> originals_;
  std::set<std::string> completed_;
  std::vector<OutboxEntry> outbox_;
  DispatchState state_ = DispatchState::idle;
};

} // namespace control
} // namespace gda
