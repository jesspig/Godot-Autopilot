#include <gtest/gtest.h>

#include "tools/base/runtime_ops.hpp"
#include "tools/domains/tween/tween_ops.hpp"
#include "util/json_value.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace godot_autopilot {
namespace tween_ops {
bool tween_ops_property_is_indexed(const std::string &property);
bool tween_ops_property_path_valid(const std::string &property,
                                   std::string &error);
} // namespace tween_ops
} // namespace godot_autopilot

namespace {

using godot_autopilot::util::JsonValue;

JsonValue parse(const std::string &text) { return JsonValue::Parse(text); }

std::string error_text(const JsonValue &result) {
  const JsonValue *error = result.Find("error");
  return (error != nullptr && error->IsString()) ? error->GetString()
                                                 : std::string();
}

std::filesystem::path find_source(const std::string &relative) {
  std::filesystem::path current = std::filesystem::current_path();
  while (true) {
    const std::filesystem::path candidate =
        current / std::filesystem::path(relative);
    if (std::filesystem::exists(candidate))
      return candidate;
    if (!current.has_parent_path())
      break;
    const std::filesystem::path parent = current.parent_path();
    if (parent == current)
      break;
    current = parent;
  }
  return std::filesystem::path();
}

std::string read_source(const std::string &relative) {
  const std::filesystem::path path = find_source(relative);
  if (path.empty())
    return std::string();
  std::ifstream input(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

} // namespace

TEST(TweenPropertyContractTest, PlainAndSubpropertyPathsUseDistinctReads) {
  namespace ops = godot_autopilot::tween_ops;
  EXPECT_FALSE(ops::tween_ops_property_is_indexed("position"));
  EXPECT_TRUE(ops::tween_ops_property_is_indexed("modulate:a"));
  std::string error;
  EXPECT_TRUE(ops::tween_ops_property_path_valid("position", error));
  EXPECT_TRUE(ops::tween_ops_property_path_valid("modulate:a", error));
}

TEST(TweenPropertyContractTest, InvalidPropertyPathsRejectedBeforeRuntime) {
  namespace ops = godot_autopilot::tween_ops;
  const char *invalid[] = {"modulate:", ":alpha", "a::b", "a/b:c"};
  for (const char *property : invalid) {
    std::string error;
    EXPECT_FALSE(ops::tween_ops_property_path_valid(property, error)) << property;
    EXPECT_NE(error.find("invalid property path"), std::string::npos) << property;
  }
}

TEST(TweenPropertyContractTest, PropertyValidationPrecedesTweenCreation) {
  namespace ops = godot_autopilot::tween_ops;
  const JsonValue missing_property =
      parse(R"({"action":"property","path":"Player","target_value":1,"duration":1})");
  const std::string missing_property_error =
      error_text(ops::handle_tween_editor_property(missing_property));
  EXPECT_NE(missing_property_error.find("missing required parameter: property"),
            std::string::npos);
  EXPECT_EQ(missing_property_error.find("EditorInterface not available"),
            std::string::npos);

  const JsonValue missing_value =
      parse(R"({"action":"property","path":"Player","property":"modulate:a","duration":1})");
  EXPECT_NE(error_text(ops::handle_tween_editor_property(missing_value))
                .find("missing required parameter: target_value"),
            std::string::npos);

  const JsonValue missing_duration =
      parse(R"({"node_path":"/root/Main","property":"modulate:a","target_value":1})");
  EXPECT_NE(error_text(ops::handle_create_game_tween(missing_duration))
                .find("missing required parameter: duration"),
            std::string::npos);
}

TEST(TweenPropertyContractTest, RuntimeReadsIndexedTypeAndKillsOnFailure) {
  const std::string runtime = read_source("src/runtime/game_bridge.cpp");
  ASSERT_FALSE(runtime.empty());
  EXPECT_NE(runtime.find("node->get_indexed(godot::NodePath"), std::string::npos);
  const std::size_t create = runtime.find("tree->create_tween()");
  const std::size_t hint = runtime.find("tween_property_type_hint(node, prop_p");
  const std::size_t kill = runtime.find("tween->kill()");
  ASSERT_NE(create, std::string::npos);
  ASSERT_NE(hint, std::string::npos);
  ASSERT_NE(kill, std::string::npos);
  EXPECT_LT(hint, create);
  EXPECT_LT(create, kill);
  EXPECT_NE(runtime.find("return fail(\"tween registry is full"), std::string::npos);
  EXPECT_EQ(runtime.find("node->get(godot::StringName(prop_p->GetString().c_str()))"),
            std::string::npos);
}

TEST(TweenPropertyContractTest, EditorReadsIndexedTypeAndKillsOnFailure) {
  const std::string editor = read_source("src/tools/domains/tween/tween_ops.cpp");
  ASSERT_FALSE(editor.empty());
  EXPECT_NE(editor.find("node->get_indexed(godot::NodePath"), std::string::npos);
  const std::size_t create = editor.find("tree->create_tween()");
  const std::size_t hint = editor.find("tween_ops_property_type_hint(node, property");
  const std::size_t kill = editor.find("tween->kill()");
  ASSERT_NE(create, std::string::npos);
  ASSERT_NE(hint, std::string::npos);
  ASSERT_NE(kill, std::string::npos);
  EXPECT_LT(hint, create);
  EXPECT_LT(create, kill);
}

TEST(TweenPropertyContractTest, GameStatusUnknownParameterListsAllowedFields) {
  const JsonValue unknown =
      godot_autopilot::runtime_ops::handle_game_status(parse(R"({"bogus":1})"));
  const std::string error = error_text(unknown);
  EXPECT_NE(error.find("bogus"), std::string::npos);
  EXPECT_NE(error.find("wait_ready"), std::string::npos);
  EXPECT_NE(error.find("session_id"), std::string::npos);
  EXPECT_EQ(error.find("accepts no parameters"), std::string::npos);

  const JsonValue bad_wait =
      godot_autopilot::runtime_ops::handle_game_status(
          parse(R"({"wait_ready":"yes"})"));
  EXPECT_EQ(error_text(bad_wait), "wait_ready must be a boolean");
  const JsonValue bad_session =
      godot_autopilot::runtime_ops::handle_game_status(
          parse(R"({"session_id":"7"})"));
  EXPECT_EQ(error_text(bad_session), "session_id must be an integer");
}
