#include "core/control/invocation_service.hpp"
#include "support/gcp_dispatcher_fixture.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace gda;
using namespace gda::control;
using namespace gda::control::fixture;

const char *const kHighRiskTools[] = {
    "delete_scene_node", "os_process", "execute_script", "game_script"};

InvocationRequest make_request(const std::string &tool, bool confirm = false) {
  InvocationRequest request;
  request.request_id = "req-confirm-1";
  request.instance_id = "inst-confirm-1";
  request.tool = tool;
  request.params = {{"action", "probe"}};
  if (confirm) {
    request.params.push_back({"confirm", "true"});
  }
  return request;
}

TEST(NativeConfirmationTest, HighRiskToolsRequireConfirmation) {
  StaticRiskProbe probe;
  for (const char *tool : kHighRiskTools) {
    probe.add(tool);
  }
  RecordingSink sink;
  InvocationService service(
      InvocationDeps{nullptr, nullptr, &probe, &sink, nullptr, nullptr,
                     nullptr, nullptr, true, 60000});

  for (const char *tool : kHighRiskTools) {
    EXPECT_TRUE(probe.high_risk(tool)) << tool;
    const InvocationResult result = service.invoke(make_request(tool));
    EXPECT_EQ(result.status, gcp::Status::needs_confirmation) << tool;
    EXPECT_EQ(result.error, gcp::ErrorCode::confirmation_required) << tool;
    EXPECT_GT(result.confirmation.ttl_ms, 0) << tool;
  }
  EXPECT_EQ(sink.call_count(), 0u);
}

TEST(NativeConfirmationTest, ConfirmTrueDispatchesHighRiskTool) {
  StaticRiskProbe probe;
  probe.add("delete_scene_node");
  RecordingSink sink;
  InvocationService service(
      InvocationDeps{nullptr, nullptr, &probe, &sink, nullptr, nullptr,
                     nullptr, nullptr, true, 60000});

  const InvocationResult result =
      service.invoke(make_request("delete_scene_node", true));
  EXPECT_EQ(result.status, gcp::Status::ok);
  EXPECT_EQ(result.error, gcp::ErrorCode::none);
  EXPECT_EQ(sink.call_count(), 1u);
  ASSERT_FALSE(sink.requests().empty());
  EXPECT_EQ(sink.requests().back().tool, "delete_scene_node");
}

TEST(NativeConfirmationTest, LowRiskToolsDispatchWithoutConfirmation) {
  StaticRiskProbe probe;
  probe.add("delete_scene_node");
  RecordingSink sink;
  InvocationService service(
      InvocationDeps{nullptr, nullptr, &probe, &sink, nullptr, nullptr,
                     nullptr, nullptr, true, 60000});

  const InvocationResult result = service.invoke(make_request("search_tools"));
  EXPECT_EQ(result.status, gcp::Status::ok);
  EXPECT_EQ(sink.call_count(), 1u);
}

} // namespace
