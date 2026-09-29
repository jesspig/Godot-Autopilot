#include "util/gdscript_wrap.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace {

using godot_autopilot::gdscript_wrap::WrapResult;
using godot_autopilot::gdscript_wrap::WRAP_MODE_BARE;
using godot_autopilot::gdscript_wrap::WRAP_MODE_EXPRESSION;
using godot_autopilot::gdscript_wrap::WRAP_MODE_WHOLE_SCRIPT;
using godot_autopilot::gdscript_wrap::has_func_definition_statement;
using godot_autopilot::gdscript_wrap::has_top_level_func_def;
using godot_autopilot::gdscript_wrap::is_single_expression;
using godot_autopilot::gdscript_wrap::map_error_line_numbers;
using godot_autopilot::gdscript_wrap::strip_extends_lines;
using godot_autopilot::gdscript_wrap::wrap_bare_body;
using godot_autopilot::gdscript_wrap::wrap_whole_script;

constexpr const char *kInject =
    "var SceneRoot := EditorInterface.get_edited_scene_root()";

const std::string kMixedIndentError =
    "mixed tab/space indentation detected in source — position: source_code — "
    "expected: consistent indentation — action: reindent source with only "
    "tabs or only spaces; note the wrapper requires the same indentation "
    "style throughout";

const std::string kFuncDefError =
    "func definition detected in single-function mode — position: "
    "source_code — expected: no func definitions while in single-function "
    "mode — action: top-level func definitions require multi-function mode; "
    "use editor script_create or wrap in a lambda";

int count_lines(const std::string &text) {
  std::istringstream stream(text);
  std::string line;
  int lines = 0;
  while (std::getline(stream, line))
    ++lines;
  return lines;
}

int find_line(const std::string &text, const std::string &target) {
  std::istringstream stream(text);
  std::string line;
  int number = 0;
  while (std::getline(stream, line)) {
    ++number;
    if (line == target)
      return number;
  }
  return -1;
}

} // namespace

TEST(GdscriptWrapTest, BareBodyWrapsMultilineStatements) {
  WrapResult r = wrap_bare_body("var a = 1\nprint(a)", "_run", kInject, false);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.mode, WRAP_MODE_BARE);
  EXPECT_EQ(r.error, "");
  EXPECT_EQ(r.header_lines, 4);
  EXPECT_EQ(r.wrapped, std::string("@tool\nextends Node\n\n") + kInject +
                           "\nfunc _run():\n    var a = 1\n    print(a)\n");
}

TEST(GdscriptWrapTest, BareBodyKeepsTabIndentStyle) {
  WrapResult r =
      wrap_bare_body("if true:\n\tprint(1)", "_run", kInject, false);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.wrapped, std::string("@tool\nextends Node\n\n") + kInject +
                           "\nfunc _run():\n\tif true:\n\t\tprint(1)\n");
}

TEST(GdscriptWrapTest, BareBodyRejectsMixedTabSpaceIndent) {
  WrapResult r = wrap_bare_body("if true:\n\tpass\nif false:\n    pass", "_run",
                                kInject, false);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.mode, WRAP_MODE_BARE);
  EXPECT_EQ(r.error, kMixedIndentError);
  EXPECT_TRUE(r.wrapped.empty());
}

TEST(GdscriptWrapTest, BareBodyCommentsExtendsLine) {
  WrapResult r = wrap_bare_body("extends Node\nvar a = 1", "_run", "", false);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.header_lines, 3);
  EXPECT_EQ(r.wrapped, "@tool\nextends Node\n\nfunc _run():\n    # extends "
                       "Node\n    var a = 1\n");
}

TEST(GdscriptWrapTest, BareBodyRejectsTopLevelFuncDefinition) {
  const std::string source = "func helper():\n    pass";
  EXPECT_TRUE(has_top_level_func_def(source));
  WrapResult r = wrap_bare_body(source, "_run", kInject, false);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.mode, WRAP_MODE_BARE);
  EXPECT_EQ(r.error, kFuncDefError);
}

TEST(GdscriptWrapTest, BareBodyRejectsFuncLikeCallSyntax) {
  EXPECT_TRUE(has_func_definition_statement("func(x)"));
  WrapResult r = wrap_bare_body("func(x)", "_run", "", false);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.error, kFuncDefError);
}

TEST(GdscriptWrapTest, DetectsStaticFuncDefinitions) {
  EXPECT_TRUE(has_top_level_func_def("static func helper():\n    pass"));
  WrapResult r =
      wrap_bare_body("static func helper():\n    pass", "_run", "", false);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.error, kFuncDefError);
}

TEST(GdscriptWrapTest, HasTopLevelFuncDefIgnoresCommentsAndNonFuncs) {
  EXPECT_FALSE(has_top_level_func_def("# func helper():\n# static func x():"));
  EXPECT_FALSE(has_top_level_func_def("var x = 1"));
  EXPECT_FALSE(has_top_level_func_def("static var x = 1"));
  EXPECT_FALSE(has_top_level_func_def(""));
}

TEST(GdscriptWrapTest, SingleExpressionAutoReturns) {
  WrapResult r = wrap_bare_body("1 + 1", "_run", kInject, true);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.mode, WRAP_MODE_EXPRESSION);
  EXPECT_EQ(r.header_lines, 4);
  EXPECT_EQ(r.wrapped, std::string("@tool\nextends Node\n\n") + kInject +
                           "\nfunc _run():\n    return 1 + 1\n");
}

TEST(GdscriptWrapTest, ExpressionModeUsesGivenEntryName) {
  WrapResult r = wrap_bare_body("1 + 1", "custom_entry", "", true);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.wrapped,
            "@tool\nextends Node\n\nfunc custom_entry():\n    return 1 + 1\n");
  EXPECT_EQ(find_line(r.wrapped, "func custom_entry():"), r.header_lines + 1);
}

TEST(GdscriptWrapTest, AutoReturnCanBeDisabled) {
  WrapResult r = wrap_bare_body("1 + 1", "_run", kInject, false);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.mode, WRAP_MODE_BARE);
  EXPECT_EQ(r.wrapped, std::string("@tool\nextends Node\n\n") + kInject +
                           "\nfunc _run():\n    1 + 1\n");
}

TEST(GdscriptWrapTest, IsSingleExpressionRejectsMultilineAssignmentKeywords) {
  EXPECT_FALSE(is_single_expression("a = 1\nb = 2"));
  EXPECT_FALSE(is_single_expression("x = 1"));
  EXPECT_FALSE(is_single_expression("if x: pass"));
  EXPECT_FALSE(is_single_expression("return 5"));
  EXPECT_FALSE(is_single_expression("var x := 1"));
  EXPECT_FALSE(is_single_expression("await foo()"));
  EXPECT_FALSE(is_single_expression("# comment"));
  EXPECT_FALSE(is_single_expression(""));
}

TEST(GdscriptWrapTest, IsSingleExpressionAcceptsCallsAndLiterals) {
  EXPECT_TRUE(is_single_expression("1 + 1"));
  EXPECT_TRUE(is_single_expression("  print(x)  "));
  EXPECT_TRUE(is_single_expression("get_node(\"Player\").position"));
}

TEST(GdscriptWrapTest, MultilineAndAssignmentFallBackToBareBody) {
  WrapResult multiline = wrap_bare_body("a = 1\nb = 2", "_run", kInject, true);
  ASSERT_TRUE(multiline.ok);
  EXPECT_EQ(multiline.mode, WRAP_MODE_BARE);
  WrapResult assignment = wrap_bare_body("x = 1", "_run", kInject, true);
  ASSERT_TRUE(assignment.ok);
  EXPECT_EQ(assignment.mode, WRAP_MODE_BARE);
}

TEST(GdscriptWrapTest, HeaderLinesTrackFuncLineWithInject) {
  WrapResult r = wrap_bare_body("var a = 1", "_run", kInject, false);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.header_lines, 4);
  EXPECT_EQ(find_line(r.wrapped, "func _run():"), r.header_lines + 1);
  EXPECT_EQ(find_line(r.wrapped, "    var a = 1"), r.header_lines + 2);
}

TEST(GdscriptWrapTest, HeaderLinesDropOneWithoutInject) {
  WrapResult r = wrap_bare_body("var a = 1", "_run", "", false);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.header_lines, 3);
  EXPECT_EQ(find_line(r.wrapped, "func _run():"), r.header_lines + 1);
  EXPECT_EQ(find_line(r.wrapped, "    var a = 1"), r.header_lines + 2);
  EXPECT_EQ(r.wrapped, "@tool\nextends Node\n\nfunc _run():\n    var a = 1\n");
}

TEST(GdscriptWrapTest, ExpressionModeHeaderLinesMatchFuncLine) {
  WrapResult r = wrap_bare_body("1 + 1", "_run", kInject, true);
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.header_lines, 4);
  EXPECT_EQ(find_line(r.wrapped, "func _run():"), r.header_lines + 1);
  EXPECT_EQ(find_line(r.wrapped, "    return 1 + 1"), r.header_lines + 2);
}

TEST(GdscriptWrapTest, WholeScriptWithEntryPointPasses) {
  const std::string source = "extends Node\n\nfunc _run():\n    return 1";
  WrapResult r = wrap_whole_script(source, "_run");
  ASSERT_TRUE(r.ok);
  EXPECT_EQ(r.mode, WRAP_MODE_WHOLE_SCRIPT);
  EXPECT_EQ(r.header_lines, 0);
  EXPECT_EQ(r.wrapped, source);
  EXPECT_EQ(r.error, "");
}

TEST(GdscriptWrapTest, WholeScriptMissingEntryPointFails) {
  WrapResult r = wrap_whole_script("func helper():\n    pass", "_run");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.mode, WRAP_MODE_WHOLE_SCRIPT);
  EXPECT_EQ(r.header_lines, 0);
  EXPECT_TRUE(r.wrapped.empty());
  EXPECT_EQ(r.error,
            "multi-function mode requires a func _run() entry point（或将函数"
            "改为 lambda 变量）");
}

TEST(GdscriptWrapTest, MapErrorLineNumbersSubtractsOffset) {
  EXPECT_EQ(map_error_line_numbers("gdscript://x.gd:10 - parse error", 5),
            "gdscript://x.gd:5 (mapped to user source line 5) - parse error");
}

TEST(GdscriptWrapTest, MapErrorLineNumbersKeepsNumbersAtOrBelowOffset) {
  EXPECT_EQ(map_error_line_numbers("gdscript://x.gd:3", 5),
            "gdscript://x.gd:3");
}

TEST(GdscriptWrapTest, MapErrorLineNumbersReturnsInputForNonPositiveOffset) {
  const std::string text = "gdscript://x.gd:10 - parse error";
  EXPECT_EQ(map_error_line_numbers(text, 0), text);
  EXPECT_EQ(map_error_line_numbers(text, -4), text);
}

TEST(GdscriptWrapTest, MapErrorLineNumbersReplacesEveryOccurrence) {
  EXPECT_EQ(map_error_line_numbers(
                "gdscript://a.gd:10\nat: gdscript://b.gd:20 - fail", 5),
            "gdscript://a.gd:5 (mapped to user source line 5)\n"
            "at: gdscript://b.gd:15 (mapped to user source line 15) - fail");
}

TEST(GdscriptWrapTest, MapErrorLineNumbersLeavesNonGdPathsUntouched) {
  const std::string text = "gdscript://x.txt:10";
  EXPECT_EQ(map_error_line_numbers(text, 5), text);
}

TEST(GdscriptWrapTest, StripExtendsLinesKeepsLineCount) {
  const std::string source = "extends Node\nvar a = 1\n";
  const std::string stripped = strip_extends_lines(source);
  EXPECT_EQ(count_lines(stripped), count_lines(source));
  EXPECT_EQ(stripped, "# extends Node\nvar a = 1");
  EXPECT_EQ(find_line(stripped, "var a = 1"), 2);
}

TEST(GdscriptWrapTest, StripExtendsLinesOnlyCommentsExtendsPrefix) {
  EXPECT_EQ(strip_extends_lines("extends Node"), "# extends Node");
  EXPECT_EQ(strip_extends_lines("  extends Node"), "#   extends Node");
  EXPECT_EQ(strip_extends_lines("extendsNode"), "extendsNode");
  EXPECT_EQ(strip_extends_lines("var extends_value = 1"),
            "var extends_value = 1");
}
