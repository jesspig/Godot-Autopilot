#include "core/control/invocation_service.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "support/gcp_dispatcher_fixture.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace gda;
using namespace gda::control;
using namespace gda::control::fixture;

InvocationRequest make_request(const std::string &tool) {
  InvocationRequest request;
  request.request_id = "req-map-1";
  request.instance_id = "inst-map-1";
  request.tool = tool;
  request.params = {{"action", "probe"}};
  return request;
}

TEST(NativeErrorMappingTest, ExportProbeMapsToExportInProgress) {
  StaticExportProbe probe;
  probe.set(true);
  RecordingSink sink;
  InvocationService service(
      InvocationDeps{nullptr, &probe, nullptr, &sink, nullptr, nullptr,
                     nullptr, nullptr, true, 60000});

  const InvocationResult result = service.invoke(make_request("search_tools"));
  EXPECT_EQ(result.status, gcp::Status::error);
  EXPECT_EQ(result.error, gcp::ErrorCode::export_in_progress);
  EXPECT_NE(result.error_message.find("export in progress"), std::string::npos);
  EXPECT_EQ(sink.call_count(), 0u);
  EXPECT_EQ(service.consume_new_errors("req-map-1"), 1);
}

TEST(NativeErrorMappingTest, MissingSinkMapsToResultUnknown) {
  InvocationService service;
  const InvocationResult result = service.invoke(make_request("search_tools"));
  EXPECT_EQ(result.status, gcp::Status::error);
  EXPECT_EQ(result.error, gcp::ErrorCode::result_unknown);
  EXPECT_NE(result.error_message.find("no dispatch sink"), std::string::npos);
}

TEST(NativeErrorMappingTest, HighRiskToolWithoutConfirmationMapsToChallenge) {
  StaticRiskProbe probe;
  probe.add("danger_tool");
  RecordingSink sink;
  InvocationService service(
      InvocationDeps{nullptr, nullptr, &probe, &sink, nullptr, nullptr,
                     nullptr, nullptr, true, 60000});

  const InvocationResult result = service.invoke(make_request("danger_tool"));
  EXPECT_EQ(result.status, gcp::Status::needs_confirmation);
  EXPECT_EQ(result.error, gcp::ErrorCode::confirmation_required);
  EXPECT_GT(result.confirmation.ttl_ms, 0);
  EXPECT_FALSE(result.confirmation.param_digest.empty());
  EXPECT_EQ(sink.call_count(), 0u);
}

TEST(NativeErrorMappingTest, ErrorCodesKeepStableNumericAndSymbolContract) {
  EXPECT_EQ(gcp::error_numeric(gcp::ErrorCode::export_in_progress), 11);
  EXPECT_EQ(gcp::error_numeric(gcp::ErrorCode::main_thread_timeout), 12);
  EXPECT_EQ(gcp::error_numeric(gcp::ErrorCode::confirmation_required), 13);
  EXPECT_STREQ(gcp::to_string(gcp::ErrorCode::export_in_progress),
               "export_in_progress");
  EXPECT_EQ(gcp::error_from_numeric(11), gcp::ErrorCode::export_in_progress);
  EXPECT_EQ(gcp::error_from_numeric(13),
            gcp::ErrorCode::confirmation_required);
}

} // namespace
