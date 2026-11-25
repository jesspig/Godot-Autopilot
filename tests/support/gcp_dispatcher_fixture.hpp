#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/control/gcp_dispatcher.hpp"
#include "core/control/invocation_service.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {
namespace fixture {

std::string make_traceparent(unsigned int seed);

std::string make_request_json(const std::string &method, const std::string &id,
                              const std::string &instance_id,
                              const gcp::Params &params = gcp::Params());

std::string make_invalid_json();

gcp::GcpEnvelope parse_response(const std::string &text, bool &ok);

class RecordingSink : public InvocationSink {
public:
  InvocationResult dispatch(const InvocationRequest &request) override;
  std::size_t call_count() const;
  const std::vector<InvocationRequest> &requests() const;

private:
  std::vector<InvocationRequest> requests_;
};

class FixedSink : public InvocationSink {
public:
  explicit FixedSink(InvocationResult result);
  InvocationResult dispatch(const InvocationRequest &request) override;
  std::size_t call_count() const;

private:
  InvocationResult result_;
  std::size_t call_count_ = 0;
};

} // namespace fixture
} // namespace control
} // namespace gda
