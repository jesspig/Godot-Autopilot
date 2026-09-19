#ifndef GODOT_AUTOPILOT_TEST_TOOLS_HPP
#define GODOT_AUTOPILOT_TEST_TOOLS_HPP

#include <tools/tool_spec.hpp>
#include "tools/test_ops.hpp"

#include <memory>
#include <string>
#include <vector>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace test_tools {

namespace {

const std::vector<ParamSpec> kRunGdscriptTestsParams = {
    {"tests", "array", "Array of test cases {\"name\": string, \"source\": string}; each source runs as plain sequential statements (top-level func/static/class declarations rejected) with check/check_equal/check_almost_equal/fail/fatal assertions and SceneRoot exposed", true},
    {"timeout_ms", "integer", "Timeout budget for the whole suite in milliseconds (integer, default: 10000, max: 30000); once exhausted remaining cases are skipped and the running case is flagged running_over_budget", false},
};

const std::vector<ParamSpec> kRunGdscriptTestFilesParams = {
    {"directory", "string", "Project directory scanned non-recursively for test files (string, default: res://tests); a path without the res:// prefix is normalized automatically", false},
    {"pattern", "string", "Filename filter supporting * and ? wildcards (string, default: *.gda_test.gd)", false},
    {"timeout_ms", "integer", "Shared timeout budget for all matched files in milliseconds (integer, default: 10000, max: 30000)", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(2);
  v.push_back(make_spec_tool(ToolSpec{
      "run_gdscript_tests",
      "Run inline GDScript test cases with the built-in assertion framework and return a structured report. Required parameter 'tests' is an array of objects {\"name\": string, \"source\": string}; each source must be plain sequential statements (top-level func/static/class declarations are rejected), is wrapped into a temporary @tool Node script, executed serially in order, and the node is removed afterwards so the scene tree stays clean. Inside a source you can call the assertions check(condition, msg=\"\"), check_equal(actual, expected) (uses Variant ==), check_almost_equal(actual, expected, epsilon=0.001) (numeric abs-diff comparison; non-numeric values fall back to check_equal semantics), fail(msg) (records a failure and keeps executing) and fatal(msg) (records a failure and immediately stops this case). A failed assertion does not abort the case; every check appends one row to checks. The edited scene root is exposed as SceneRoot. Optional 'timeout_ms' (default 10000, hard maximum 30000) budgets the whole suite: once exhausted remaining cases get status skip with reason 'timeout budget exhausted'; the currently running case cannot be interrupted because GDScript execution is synchronous, so it is flagged running_over_budget:true. Compilation failures return status fail with error messages whose line numbers are remapped to your source. Response: {summary: {total, passed, failed, skipped}, cases: [{name, status pass|fail|skip, checks: [{ok, message}], error, elapsed_ms}]}. Example: {\"tests\": [{\"name\": \"math\", \"source\": \"check_equal(1 + 1, 2)\\ncheck_almost_equal(sqrt(2.0), 1.414, 0.01)\\ncheck(SceneRoot != null, \\\"scene must be open\\\")\"}]}",
      "Testing", {"test", "gdscript", "assert", "suite"}, SideEffect::None, tool_flags::kNone,
      kRunGdscriptTestsParams, ::godot_autopilot::test_ops::handle_run_gdscript_tests}));
  v.push_back(make_spec_tool(ToolSpec{
      "run_gdscript_test_files",
      "Run GDScript test files from a project directory using the same built-in assertion framework and response contract as run_gdscript_tests. Optional 'directory' (default \"res://tests\"; a path without the res:// prefix is normalized automatically) is scanned non-recursively; optional 'pattern' (default \"*.gda_test.gd\"; supports * and ? wildcards) filters files by name; matches are sorted by filename and each file's content runs as one case named by its full res:// path, sharing one optional 'timeout_ms' budget (default 10000, hard maximum 30000). Assertions available inside each file: check(condition, msg=\"\"), check_equal(actual, expected) (uses Variant ==), check_almost_equal(actual, expected, epsilon=0.001) (numeric abs-diff, non-numeric falls back to check_equal semantics), fail(msg), fatal(msg); SceneRoot exposes the edited scene root; file sources must be plain sequential statements without top-level func/class declarations. Returns {summary: {total, passed, failed, skipped}, cases: [{name, status pass|fail|skip, checks: [{ok, message}], error, elapsed_ms}], files: [...]} where files lists every executed path; an unknown directory returns an error and zero matches return an all-zero summary plus a note. Example: {\"directory\": \"res://tests\", \"pattern\": \"*.gda_test.gd\"}",
      "Testing", {"test", "gdscript", "files", "directory"}, SideEffect::None, tool_flags::kNone,
      kRunGdscriptTestFilesParams, ::godot_autopilot::test_ops::handle_run_gdscript_test_files}));
  return v;
}

} // namespace test_tools
} // namespace godot_autopilot
#endif
