#include "tools/dispatch.hpp"
#include "util/gdscript_wrap.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::dispatch::exec_timeout_result;
using godot_autopilot::gdscript_wrap::deadline_check_line;
using godot_autopilot::gdscript_wrap::deadline_declaration;
using godot_autopilot::gdscript_wrap::inject_deadline_checks;
using godot_autopilot::gdscript_wrap::insert_entry_check;
using godot_autopilot::gdscript_wrap::is_timeout_sentinel_text;
using godot_autopilot::gdscript_wrap::timeout_sentinel_declaration;

namespace {

bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

} // namespace

TEST(ExecBudgetTest, TimeoutSentinelTextMatchesDeclaredConstant) {
  EXPECT_TRUE(is_timeout_sentinel_text("__GDA_TIMEOUT__"));
  EXPECT_FALSE(is_timeout_sentinel_text("__GDA_TIMEOUT"));
  EXPECT_FALSE(is_timeout_sentinel_text(""));
}

TEST(ExecBudgetTest, ExecTimeoutResultCarriesStructuredContract) {
  const mcp::JsonValue result = exec_timeout_result(300, 351);
  ASSERT_TRUE(result.Find("error") != nullptr);
  EXPECT_EQ(result["error"].GetString(),
            "execution timed out after 300 ms");
  EXPECT_EQ(result["code"].GetString(), "exec_timeout");
  ASSERT_TRUE(result.Find("retryable") != nullptr);
  EXPECT_FALSE(result["retryable"].GetBool());
  ASSERT_TRUE(result.Find("execution_time_ms") != nullptr);
  EXPECT_EQ(result["execution_time_ms"].GetInt(), 351);
  ASSERT_TRUE(result.Find("timeout_ms") != nullptr);
  EXPECT_EQ(result["timeout_ms"].GetInt(), 300);
}

TEST(ExecBudgetTest, DeadlineDeclarationUsesScriptConstant) {
  const std::string declaration = deadline_declaration(1234567);
  EXPECT_TRUE(contains(declaration, "const __gda_deadline_ms := 1234567"));
  const std::string sentinel = timeout_sentinel_declaration();
  EXPECT_TRUE(contains(sentinel, "const __GDA_TIMEOUT__ :="));
  EXPECT_TRUE(contains(sentinel, "\"__GDA_TIMEOUT__\""));
  const std::string check = deadline_check_line();
  EXPECT_TRUE(contains(check, "Time.get_ticks_msec() > __gda_deadline_ms"));
  EXPECT_TRUE(contains(check, "return __GDA_TIMEOUT__"));
}

TEST(ExecBudgetTest, SingleLineWhileLoopGetsDeadlineCondition) {
  const godot_autopilot::gdscript_wrap::DeadlineTransform transform =
      inject_deadline_checks("while true: pass");
  ASSERT_TRUE(transform.ok);
  EXPECT_TRUE(contains(transform.source, "while (true) and "));
  EXPECT_TRUE(contains(transform.source, "Time.get_ticks_msec() <= __gda_deadline_ms"));
  EXPECT_TRUE(contains(transform.source, "pass"));
}

TEST(ExecBudgetTest, BlockWhileLoopGetsDeadlineCheckpoint) {
  const godot_autopilot::gdscript_wrap::DeadlineTransform transform =
      inject_deadline_checks("while true:\n\tpass\n");
  ASSERT_TRUE(transform.ok);
  EXPECT_TRUE(contains(transform.source, "if Time.get_ticks_msec() > __gda_deadline_ms: return __GDA_TIMEOUT__"));
  EXPECT_TRUE(contains(transform.source, "\tpass"));
}

TEST(ExecBudgetTest, ForLoopBodyGetsDeadlineCheckpoint) {
  const godot_autopilot::gdscript_wrap::DeadlineTransform transform =
      inject_deadline_checks("for i in range(3):\n\tprint(i)\n");
  ASSERT_TRUE(transform.ok);
  EXPECT_TRUE(contains(transform.source, "if Time.get_ticks_msec() > __gda_deadline_ms: return __GDA_TIMEOUT__"));
  EXPECT_TRUE(contains(transform.source, "for i in range(3):"));
}

TEST(ExecBudgetTest, EntryCheckIsInjectedIntoEntryFunction) {
  const std::string transformed = insert_entry_check(
      "func _run():\n\treturn 1\n", "_run",
      deadline_check_line());
  EXPECT_TRUE(contains(transformed,
                        "if Time.get_ticks_msec() > __gda_deadline_ms: return __GDA_TIMEOUT__"));
  EXPECT_TRUE(contains(transformed, "\treturn 1"));
}

TEST(ExecBudgetTest, EntryCheckIsSkippedWhenEntryFunctionAbsent) {
  const std::string transformed = insert_entry_check(
      "func other():\n\treturn 1\n", "_run",
      deadline_check_line());
  EXPECT_FALSE(contains(transformed, "__gda_deadline_ms"));
}
