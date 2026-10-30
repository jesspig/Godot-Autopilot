#include <gtest/gtest.h>

#include <string>

enum class ElementBranch { kUntyped, kNative, kScript, kUnresolvable };

struct BranchDecision {
  ElementBranch branch = ElementBranch::kUntyped;
  bool element_script_loaded = false;
  std::string error;
};

static std::string parse_hint_class(const std::string &hint_string) {
  const std::string prefix = "Type:";
  std::string s = hint_string;
  if (s.compare(0, prefix.size(), prefix) == 0) {
    s = s.substr(prefix.size());
  }
  size_t comma = s.find(',');
  if (comma != std::string::npos) {
    s = s.substr(0, comma);
  }
  size_t start = s.find_first_not_of(" \t");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = s.find_last_not_of(" \t");
  return s.substr(start, end - start + 1);
}

static BranchDecision decide_element_branch(const std::string &element_class,
                                            bool native_exists, bool script_resolves) {
  BranchDecision decision;
  if (element_class.empty()) {
    return decision;
  }
  if (native_exists) {
    decision.branch = ElementBranch::kNative;
    return decision;
  }
  if (!script_resolves) {
    decision.branch = ElementBranch::kUnresolvable;
    decision.error = "cannot convert element: cannot resolve array element class '" +
                     element_class + "'";
    return decision;
  }
  decision.branch = ElementBranch::kScript;
  decision.element_script_loaded = true;
  return decision;
}

static std::string native_mismatch_error(const std::string &label,
                                         const std::string &actual,
                                         const std::string &expected) {
  return "cannot convert " + label + ": resource class '" + actual + "' is not '" + expected +
         "'";
}

static std::string script_mismatch_error(const std::string &label,
                                         const std::string &expected) {
  return "cannot convert " + label + ": resource script is not '" + expected + "'";
}

TEST(ArrayScriptBranchTest, EmptyClassStaysUntyped) {
  const BranchDecision decision = decide_element_branch("", false, false);
  EXPECT_EQ(ElementBranch::kUntyped, decision.branch);
  EXPECT_FALSE(decision.element_script_loaded);
  EXPECT_TRUE(decision.error.empty());
}

TEST(ArrayScriptBranchTest, NativeClassTakesNativeBranch) {
  const BranchDecision decision = decide_element_branch("Node2D", true, false);
  EXPECT_EQ(ElementBranch::kNative, decision.branch);
  EXPECT_FALSE(decision.element_script_loaded);
  EXPECT_TRUE(decision.error.empty());
}

TEST(ArrayScriptBranchTest, GlobalScriptClassTakesScriptBranch) {
  const BranchDecision decision = decide_element_branch("GdaPropItem", false, true);
  EXPECT_EQ(ElementBranch::kScript, decision.branch);
  EXPECT_TRUE(decision.element_script_loaded);
  EXPECT_TRUE(decision.error.empty());
}

TEST(ArrayScriptBranchTest, UnknownClassReportsUnresolvable) {
  const BranchDecision decision = decide_element_branch("NoSuchClass", false, false);
  EXPECT_EQ(ElementBranch::kUnresolvable, decision.branch);
  EXPECT_FALSE(decision.element_script_loaded);
  EXPECT_EQ("cannot convert element: cannot resolve array element class 'NoSuchClass'",
            decision.error);
}

TEST(ArrayScriptBranchTest, NativeMismatchNamesBothClasses) {
  EXPECT_EQ("cannot convert element #0: resource class 'Sprite2D' is not 'Node2D'",
            native_mismatch_error("element #0", "Sprite2D", "Node2D"));
}

TEST(ArrayScriptBranchTest, ScriptMismatchNamesExpectedClass) {
  EXPECT_EQ("cannot convert element #1: resource script is not 'GdaPropItem'",
            script_mismatch_error("element #1", "GdaPropItem"));
}

TEST(ArrayScriptBranchTest, ParseHintClassKeepsBareName) {
  EXPECT_EQ("Node2D", parse_hint_class("Node2D"));
}

TEST(ArrayScriptBranchTest, ParseHintClassStripsTypePrefix) {
  EXPECT_EQ("Node2D", parse_hint_class("Type:Node2D"));
}

TEST(ArrayScriptBranchTest, ParseHintClassCutsAtCommaAndTrimsSpaces) {
  EXPECT_EQ("Type:Node2D", parse_hint_class("  Type:Node2D , extra"))
      << "locks src parse_hint_class semantics: leading spaces block Type: prefix strip, comma cut and trim still apply";
}

TEST(ArrayScriptBranchTest, ParseHintClassBlankYieldsEmpty) {
  EXPECT_EQ("", parse_hint_class("   "));
  EXPECT_EQ("", parse_hint_class(""));
}
