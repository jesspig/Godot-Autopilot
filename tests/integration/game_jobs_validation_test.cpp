#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class GameJobsSuite : public IntegrationHarnessWithAllow {
 public:
  GameJobsSuite() : IntegrationHarnessWithAllow("game_runtime") {}
};

mcp::JsonValue make_args() {
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

mcp::JsonValue start_args() {
  mcp::JsonValue args = make_args();
  args["confirm"] = mcp::JsonValue(true);
  return args;
}

void assert_exact_error(const mcp::JsonValue &result,
                        const std::string &expected) {
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString()) << result.Dump();
  EXPECT_EQ(v->GetString(), expected);
}

TEST_F(GameJobsSuite, StartGameJobMissingOp) {
  mcp::JsonValue result = call("start_game_job", start_args());
  assert_exact_error(
      result, "missing required parameter: op (currently only \"eval\")");
}

TEST_F(GameJobsSuite, StartGameJobUnsupportedOp) {
  mcp::JsonValue args = start_args();
  args["op"] = mcp::JsonValue("status");
  args["params"] = make_args();
  mcp::JsonValue result = call("start_game_job", args);
  assert_exact_error(
      result,
      "unsupported op: status — start_game_job currently supports op \"eval\" only");
}

TEST_F(GameJobsSuite, StartGameJobMissingParams) {
  mcp::JsonValue args = start_args();
  args["op"] = mcp::JsonValue("eval");
  mcp::JsonValue result = call("start_game_job", args);
  assert_exact_error(
      result,
      "missing required parameter: params (object passed to the wrapped op "
      "verbatim, e.g. {\"action\":\"script\",\"source_code\":\"...\"})");
}

TEST_F(GameJobsSuite, StartGameJobTimeoutOutOfRange) {
  mcp::JsonValue args = start_args();
  args["op"] = mcp::JsonValue("eval");
  mcp::JsonValue params = make_args();
  params["action"] = mcp::JsonValue("script");
  params["source_code"] = mcp::JsonValue("func _run() -> void: pass");
  args["params"] = std::move(params);
  args["timeout_ms"] = mcp::JsonValue(25001);
  mcp::JsonValue result = call("start_game_job", args);
  assert_exact_error(result,
                     "timeout_ms must be an integer between 1 and 25000");
}

TEST_F(GameJobsSuite, StartGameJobValidArgsNoGameReturnsRuntimeError) {
  mcp::JsonValue args = start_args();
  args["op"] = mcp::JsonValue("eval");
  mcp::JsonValue params = make_args();
  params["action"] = mcp::JsonValue("script");
  params["source_code"] = mcp::JsonValue("func _run() -> void: pass");
  args["params"] = std::move(params);
  mcp::JsonValue result = call("start_game_job", args);
  const auto *error = result.Find("error");
  ASSERT_NE(error, nullptr) << result.Dump();
  ASSERT_TRUE(error->IsString()) << result.Dump();
  EXPECT_FALSE(error->GetString().empty());
  const auto *hint = result.Find("hint");
  ASSERT_NE(hint, nullptr) << result.Dump();
  ASSERT_TRUE(hint->IsString()) << result.Dump();
  EXPECT_FALSE(hint->GetString().empty());
}

TEST_F(GameJobsSuite, GetGameJobMissingJobId) {
  mcp::JsonValue result = call("get_game_job", make_args());
  assert_exact_error(
      result,
      "missing required parameter: job_id (integer returned by start_game_job)");
}

TEST_F(GameJobsSuite, GetGameJobUnknownJobIdFourPartError) {
  mcp::JsonValue args = make_args();
  args["job_id"] = mcp::JsonValue(999999);
  mcp::JsonValue result = call("get_game_job", args);
  assert_exact_error(
      result,
      "unknown or already collected game job: 999999"
      " — position: get_game_job"
      " — expected: a job_id returned by start_game_job that is still tracked"
      " — action: re-check the start_game_job response for the correct job_id; "
      "jobs are removed once collected or after they expire timeout_ms + 2000 "
      "ms after start");
}

TEST_F(GameJobsSuite, GetGameJobCancelWrongType) {
  mcp::JsonValue args = make_args();
  args["job_id"] = mcp::JsonValue(1);
  args["cancel"] = mcp::JsonValue("yes");
  mcp::JsonValue result = call("get_game_job", args);
  assert_exact_error(result, "cancel must be a boolean");
}

TEST_F(GameJobsSuite, GetGameJobTimeoutNegative) {
  mcp::JsonValue args = make_args();
  args["job_id"] = mcp::JsonValue(1);
  args["timeout_ms"] = mcp::JsonValue(-1);
  mcp::JsonValue result = call("get_game_job", args);
  assert_exact_error(result,
                     "timeout_ms must be an integer between 0 and 25000");
}

} // namespace
