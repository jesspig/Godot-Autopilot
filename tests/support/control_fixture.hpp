#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/control/catalog_service.hpp"
#include "core/control/invocation_service.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {
namespace fixture {

class FakeSink : public InvocationSink {
public:
  InvocationResult result;
  InvocationRequest last;
  int calls = 0;

  InvocationResult dispatch(const InvocationRequest &request) override;
};

class FakeWaiter : public DeferredWaiter {
public:
  bool completed = false;
  InvocationResult result;
  int calls = 0;
  std::string last_task_id;

  bool wait(const std::string &task_id, std::int64_t deadline_ms,
            InvocationResult &out) override;
};

std::string traceparent(unsigned int seed = 11);

gcp::Params sample_params();

InvocationRequest make_request(const std::string &request_id,
                               const std::string &tool,
                               const std::string &instance_id = "inst-1",
                               std::int64_t generation = 1);

CatalogService sample_catalog();

} // namespace fixture
} // namespace control
} // namespace gda
