#include "support/gcp_dispatcher_fixture.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {
namespace fixture {

std::string make_traceparent(unsigned int seed) {
  static const char hex[] = "0123456789abcdef";
  unsigned int state = seed == 0 ? 1u : seed;
  std::string out = "00-";
  for (int index = 0; index < 32; ++index) {
    state = state * 1664525u + 1013904223u;
    out.push_back(hex[(state >> 28) & 0xFu]);
  }
  out.push_back('-');
  for (int index = 0; index < 16; ++index) {
    state = state * 1664525u + 1013904223u;
    out.push_back(hex[(state >> 28) & 0xFu]);
  }
  out += "-01";
  return out;
}

std::string make_request_json(const std::string &method, const std::string &id,
                              const std::string &instance_id,
                              const gcp::Params &params) {
  gcp::GcpEnvelope request;
  request.v = gcp::kEnvelopeVersion;
  request.kind = gcp::Kind::request;
  request.id = id;
  request.instance_id = instance_id;
  request.method = method;
  request.params = params;
  request.trace.traceparent = make_traceparent(23);
  request.deadline_ms = gcp::kDefaultDeadlineMs;
  std::string out;
  GcpDispatcher::encode(request, out);
  return out;
}

std::string make_invalid_json() { return "{not-json"; }

gcp::GcpEnvelope parse_response(const std::string &text, bool &ok) {
  gcp::GcpEnvelope out;
  std::string error;
  ok = GcpDispatcher::decode(text, out, error);
  return out;
}

InvocationResult RecordingSink::dispatch(const InvocationRequest &request) {
  requests_.push_back(request);
  InvocationResult result;
  result.status = gcp::Status::ok;
  result.payload = "recorded-ok";
  result.artifact = make_artifact(result.payload, request.request_id);
  return result;
}

std::size_t RecordingSink::call_count() const { return requests_.size(); }

const std::vector<InvocationRequest> &RecordingSink::requests() const {
  return requests_;
}

FixedSink::FixedSink(InvocationResult result) : result_(std::move(result)) {}

InvocationResult FixedSink::dispatch(const InvocationRequest &) {
  ++call_count_;
  return result_;
}

std::size_t FixedSink::call_count() const { return call_count_; }

} // namespace fixture
} // namespace control
} // namespace gda
