#include <gtest/gtest.h>

#include <string>
#include <util/gdscript_wrap.hpp>
#include <vector>

TEST(GdscriptAutoloadHintTest, MatchesRegisteredAutoload) {
  const std::string err =
      "Identifier \"Player\" not declared in the current scope.";
  const std::string hint = godot_autopilot::gdscript_wrap::autoload_global_hint(
      err, {"Player", "GameState"});
  EXPECT_NE(hint.find("get_node(\"/root/Player\")"), std::string::npos);
}

TEST(GdscriptAutoloadHintTest, NoHintWhenNameIsNotAutoload) {
  const std::string err =
      "Identifier \"Goblin\" not declared in the current scope.";
  EXPECT_TRUE(
      godot_autopilot::gdscript_wrap::autoload_global_hint(err, {"Player"})
          .empty());
}

TEST(GdscriptAutoloadHintTest, NoHintForOtherCompileErrors) {
  const std::string err = "Expected end of statement after expression.";
  EXPECT_TRUE(
      godot_autopilot::gdscript_wrap::autoload_global_hint(err, {"Player"})
          .empty());
}

TEST(GdscriptAutoloadHintTest, DeduplicatesRepeatedNames) {
  const std::string err =
      "Identifier \"Player\" not declared in the current scope.\n"
      "Identifier \"Player\" not declared in the current scope.";
  const std::string hint = godot_autopilot::gdscript_wrap::autoload_global_hint(
      err, {"Player"});
  const size_t first = hint.find("get_node(\"/root/Player\")");
  ASSERT_NE(first, std::string::npos);
  EXPECT_EQ(hint.find("get_node(\"/root/Player\")", first + 1),
            std::string::npos);
}

TEST(GdscriptAutoloadHintTest, BareIdentifierWithoutQuotesMatches) {
  const std::string err =
      "Identifier GameState not declared in the current scope.";
  const std::string hint = godot_autopilot::gdscript_wrap::autoload_global_hint(
      err, {"GameState"});
  EXPECT_NE(hint.find("get_node(\"/root/GameState\")"), std::string::npos);
}

TEST(GdscriptAutoloadHintTest, EmptyInputsProduceNoHint) {
  EXPECT_TRUE(
      godot_autopilot::gdscript_wrap::autoload_global_hint("", {"Player"})
          .empty());
  EXPECT_TRUE(godot_autopilot::gdscript_wrap::autoload_global_hint(
                  "Identifier \"Player\" not declared in the current scope.",
                  {})
                  .empty());
}