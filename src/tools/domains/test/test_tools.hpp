#ifndef GODOT_AUTOPILOT_TEST_TOOLS_HPP
#define GODOT_AUTOPILOT_TEST_TOOLS_HPP

#include <tools/base/tool_spec.hpp>
#include "tools/domains/test/test_ops.hpp"

#include <memory>
#include <string>
#include <vector>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace test_tools {

namespace {

const std::vector<ParamSpec> kRunGdscriptTestsParams = {
    {"tests", "array", "Inline mode: array of test cases {\"name\": string, \"source\": string}; each source runs as plain sequential statements (top-level func/static/class declarations rejected) with check/check_equal/check_almost_equal/fail/fatal assertions and SceneRoot exposed. Omit tests and pass directory and/or pattern to run test files from disk instead", false},
    {"directory", "string", "File mode (used when tests is absent or directory/pattern is given): project directory scanned non-recursively for test files (string, default: res://tests); a path without the res:// prefix is normalized automatically", false},
    {"pattern", "string", "File mode: filename filter supporting * and ? wildcards (string, default: *.gda_test.gd)", false},
    {"timeout_ms", "integer", "Timeout budget for the whole run in milliseconds (integer, default: 10000, max: 30000); once exhausted remaining cases are skipped and the running case is flagged running_over_budget", false},
};

inline mcp::JsonValue test_files_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *directory = args.Find("directory");
  out["directory"] = directory != nullptr && directory->IsString()
                         ? *directory
                         : mcp::JsonValue("res://tests");
  const mcp::JsonValue *pattern = args.Find("pattern");
  out["pattern"] = pattern != nullptr && pattern->IsString()
                       ? *pattern
                       : mcp::JsonValue("*.gda_test.gd");
  const mcp::JsonValue *timeout = args.Find("timeout_ms");
  if (timeout != nullptr) {
    out["timeout_ms"] = *timeout;
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(1);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "run_gdscript_tests",
      "Run GDScript tests with the built-in assertion framework and return a structured report. Two modes: (1) inline mode — pass tests, an array of objects {\"name\": string, \"source\": string}; each source must be plain sequential statements (top-level func/static/class declarations are rejected), is wrapped into a temporary @tool Node script, executed serially in order, and the node is removed afterwards so the scene tree stays clean. (2) file mode — omit tests and pass directory and/or pattern; the directory (default \"res://tests\"; a path without the res:// prefix is normalized automatically) is scanned non-recursively, files matching pattern (default \"*.gda_test.gd\"; supports * and ? wildcards) are sorted by filename, and each file's content runs as one case named by its full res:// path sharing the same timeout budget; if neither tests nor directory/pattern is given the tool errors. Assertions available in both modes: check(condition, msg=\"\"), check_equal(actual, expected) (uses Variant ==), check_almost_equal(actual, expected, epsilon=0.001) (numeric abs-diff; non-numeric falls back to check_equal semantics), fail(msg) (records a failure and keeps executing), fatal(msg) (records a failure and immediately stops this case); a failed assertion does not abort the case, every check appends one row to checks, and the edited scene root is exposed as SceneRoot. Optional timeout_ms (default 10000, hard maximum 30000) budgets the whole run: once exhausted remaining cases get status skip with reason 'timeout budget exhausted'; the currently running case cannot be interrupted because GDScript execution is synchronous, so it is flagged running_over_budget:true. Compilation failures return status fail with remapped line numbers. Response: {summary: {total, passed, failed, skipped}, cases: [{name, status pass|fail|skip, checks: [{ok, message}], error, elapsed_ms}]}; file mode additionally returns files (every executed path) and, when nothing matched, a note. Example: {\"tests\": [{\"name\": \"math\", \"source\": \"check_equal(1 + 1, 2)\"}]} or {\"directory\": \"res://tests\", \"pattern\": \"*.gda_test.gd\"}.",
      "Testing", {"test", "gdscript", "assert", "suite", "files", "directory"},
      SideEffect::None, tool_flags::kNone,
      kRunGdscriptTestsParams, ::godot_autopilot::test_ops::handle_run_gdscript_tests,
      {},
      {},
      {"{\"tests\": [{\"name\": \"math\", \"source\": \"check_equal(1 + 1, 2)\"}]}",
       "{\"directory\": \"res://tests\", \"pattern\": \"*.gda_test.gd\"}"}},
      {
          ToolAlias{
              "run_gdscript_test_files", "run_gdscript_tests",
              test_files_args, {}, "0.4.0", "0.6.0"},
      })));
  return v;
}

} // namespace test_tools
} // namespace godot_autopilot
#endif
