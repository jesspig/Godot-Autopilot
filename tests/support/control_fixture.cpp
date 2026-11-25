#include "support/control_fixture.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/control/catalog_service.hpp"
#include "core/control/invocation_service.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {
namespace fixture {

InvocationResult FakeSink::dispatch(const InvocationRequest &request) {
  ++calls;
  last = request;
  return result;
}

bool FakeWaiter::wait(const std::string &task_id, std::int64_t deadline_ms,
                      InvocationResult &out) {
  ++calls;
  last_task_id = task_id;
  if (!completed) {
    return false;
  }
  out = result;
  return true;
}

std::string traceparent(unsigned int seed) {
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

gcp::Params sample_params() {
  return gcp::Params{{"b", "2"}, {"a", "1"}};
}

InvocationRequest make_request(const std::string &request_id,
                               const std::string &tool,
                               const std::string &instance_id,
                               std::int64_t generation) {
  InvocationRequest request;
  request.request_id = request_id;
  request.instance_id = instance_id;
  request.generation = generation;
  request.tool = tool;
  request.params = sample_params();
  request.traceparent = traceparent(11);
  request.deadline_ms = 30000;
  request.deferred = false;
  return request;
}

CatalogService sample_catalog() {
  CatalogService catalog;
  CatalogEntry scene;
  scene.name = "scene.inspect";
  scene.description = "Inspect the current scene tree";
  scene.category = "scene";
  scene.tags = {"view", "read"};
  scene.aliases = {"inspect_scene"};
  catalog.add(scene);
  CatalogEntry open;
  open.name = "project.open";
  open.description = "Open a project on disk";
  open.category = "project";
  open.tags = {"io"};
  open.mutating = true;
  open.high_risk = true;
  catalog.add(open);
  CatalogEntry task;
  task.name = "task.run";
  task.description = "Run a background task";
  task.category = "task";
  task.tags = {"async"};
  catalog.add(task);
  return catalog;
}

} // namespace fixture
} // namespace control
} // namespace gda
