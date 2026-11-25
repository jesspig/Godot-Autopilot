#include <gtest/gtest.h>

#include <string>

#include "core/control/catalog_service.hpp"
#include "core/control/control_service.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "support/control_fixture.hpp"

namespace {

using namespace gda::control;
using namespace gda::control::fixture;
namespace gcp = gda::gcp;

TEST(ControlServiceTest, FacadeSharesCatalogAndConfirmationWithInvocation) {
  ControlService service;
  FakeSink sink;
  sink.result.status = gcp::Status::ok;
  sink.result.payload = "done";
  service.invocation().set_sink(&sink);

  CatalogEntry entry;
  entry.name = "scene.inspect";
  entry.description = "Inspect the scene";
  entry.category = "scene";
  entry.aliases = {"inspect_scene"};
  service.catalog().add(entry);

  const InvocationResult result =
      service.invocation().invoke(fixture::make_request("r1", "inspect_scene"));
  EXPECT_EQ(result.status, gcp::Status::ok);
  EXPECT_EQ(sink.last.tool, "scene.inspect");

  StaticRiskProbe risk;
  risk.add("project.open");
  service.invocation().set_risk_probe(&risk);
  const InvocationResult challenged =
      service.invocation().invoke(fixture::make_request("r2", "project.open"));
  EXPECT_EQ(challenged.status, gcp::Status::needs_confirmation);
  EXPECT_TRUE(service.confirmation().has_pending("r2"));
  EXPECT_EQ(challenged.confirmation.param_digest,
            gcp::param_digest(normalize_params(fixture::sample_params())));
}

TEST(ControlServiceTest, FacadeExposesJobsEventsAndTrace) {
  ControlService service;
  service.jobs().create("inst-1", 1, "task-1", 1000, 100);
  EXPECT_TRUE(service.jobs().exists(TaskKey{"inst-1", 1, "task-1"}));

  const std::string subscription =
      service.events().subscribe("inst-1", "", 0);
  ASSERT_FALSE(subscription.empty());
  const EventRecord event = service.events().publish("inst-1", "task.updated", "x");
  EXPECT_EQ(event.seq, 1u);
  EXPECT_EQ(service.events().pending_for(subscription), 1u);

  const SpanContext context = service.trace().begin(fixture::traceparent(5));
  EXPECT_EQ(service.trace().depth(), 1u);
  EXPECT_TRUE(service.trace().end(context.span_id));
  EXPECT_EQ(service.trace().depth(), 0u);
}

} // namespace
