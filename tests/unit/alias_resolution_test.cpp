#include <gtest/gtest.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "util/json_value.hpp"

#include "core/command_queue.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/tool_catalog.hpp"
#include "tools/base/tool_registry.hpp"
#include "tools/base/tool_spec.hpp"
#include "util/bm25_index.hpp"

namespace {

using godot_autopilot::Bm25Index;
using godot_autopilot::CommandQueue;
using godot_autopilot::SideEffect;
using godot_autopilot::ToolAlias;
using godot_autopilot::ToolCatalog;
using godot_autopilot::ToolRegistry;
using godot_autopilot::ToolSpec;
namespace tool_flags = godot_autopilot::tool_flags;

godot_autopilot::util::JsonValue object() {
  return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
}

std::string string_field(const godot_autopilot::util::JsonValue &value, const char *key) {
  const godot_autopilot::util::JsonValue *found = value.Find(key);
  if (found == nullptr || !found->IsString()) {
    return std::string();
  }
  return found->GetString();
}

ToolSpec make_tool(
    const std::string &name, SideEffect effect,
    std::function<godot_autopilot::util::JsonValue(const godot_autopilot::util::JsonValue &)> handler) {
  ToolSpec spec;
  spec.name = name;
  spec.description = name + " alias test target";
  spec.category = "Testing";
  spec.side_effect = effect;
  spec.flags = tool_flags::kNone;
  spec.handler = std::move(handler);
  return spec;
}

ToolAlias make_alias(const std::string &name, const std::string &canonical) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

class AliasResolutionTest : public ::testing::Test {
protected:
  void SetUp() override {
    godot_autopilot::runtime_ops::set_editor_queue(&queue_);

    ToolSpec echo = make_tool(
        "alias_target_echo", SideEffect::None,
        [](const godot_autopilot::util::JsonValue &args) {
          godot_autopilot::util::JsonValue result(object());
          result["echo"] = args;
          return result;
        });
    echo.examples.push_back("{\"old_key\":\"value\"}");
    echo.aliases.push_back(make_alias("old_echo_name", "alias_target_echo"));

    ToolAlias param = make_alias("old_echo_param", "alias_target_echo");
    param.adapt_args = [](const godot_autopilot::util::JsonValue &args) {
      godot_autopilot::util::JsonValue adapted(object());
      const godot_autopilot::util::JsonValue *old_key = args.Find("old_key");
      if (old_key != nullptr) {
        adapted["new_key"] = *old_key;
      }
      return adapted;
    };
    echo.aliases.push_back(param);

    ToolAlias scalar = make_alias("old_echo_scalar", "alias_target_echo");
    scalar.adapt_args = [](const godot_autopilot::util::JsonValue &args) {
      godot_autopilot::util::JsonValue adapted(object());
      godot_autopilot::util::JsonValue items(godot_autopilot::util::JsonValue::array_tag);
      const godot_autopilot::util::JsonValue *item = args.Find("item");
      if (item != nullptr) {
        items.PushBack(*item);
      }
      adapted["items"] = std::move(items);
      return adapted;
    };
    echo.aliases.push_back(scalar);
    registry_->add(godot_autopilot::make_spec_tool(std::move(echo)));

    ToolSpec profile = make_tool(
        "alias_target_profile", SideEffect::None,
        [](const godot_autopilot::util::JsonValue &) {
          godot_autopilot::util::JsonValue result(object());
          result["value"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(42));
          result["extra"] = godot_autopilot::util::JsonValue("x");
          return result;
        });
    ToolAlias projection = make_alias("old_profile", "alias_target_profile");
    projection.project_result = [](const godot_autopilot::util::JsonValue &result) {
      godot_autopilot::util::JsonValue projected(object());
      const godot_autopilot::util::JsonValue *value = result.Find("value");
      if (value != nullptr) {
        projected["result"] = *value;
      }
      return projected;
    };
    profile.aliases.push_back(projection);
    registry_->add(godot_autopilot::make_spec_tool(std::move(profile)));

    ToolSpec destructive = make_tool(
        "alias_target_destructive", SideEffect::Destructive,
        [](const godot_autopilot::util::JsonValue &) {
          godot_autopilot::util::JsonValue result(object());
          result["result"] = godot_autopilot::util::JsonValue("ok");
          return result;
        });
    ToolAlias destructive_alias =
        make_alias("old_destructive", "alias_target_destructive");
    destructive_alias.adapt_args = [](const godot_autopilot::util::JsonValue &args) {
      godot_autopilot::util::JsonValue adapted(object());
      const godot_autopilot::util::JsonValue *payload = args.Find("payload");
      if (payload != nullptr) {
        adapted["payload"] = *payload;
      }
      return adapted;
    };
    destructive.aliases.push_back(destructive_alias);
    registry_->add(godot_autopilot::make_spec_tool(std::move(destructive)));

    ToolSpec conflict = make_tool(
        "alias_target_conflict", SideEffect::None,
        [](const godot_autopilot::util::JsonValue &) { return object(); });
    conflict.aliases.push_back(
        make_alias("alias_target_echo", "alias_target_conflict"));
    registry_->add(godot_autopilot::make_spec_tool(std::move(conflict)));

    ToolSpec future = make_tool(
        "alias_target_future", SideEffect::None,
        [](const godot_autopilot::util::JsonValue &) { return object(); });
    future.aliases.push_back(make_alias("future_real", "alias_target_future"));
    registry_->add(godot_autopilot::make_spec_tool(std::move(future)));

    ToolSpec real = make_tool(
        "future_real", SideEffect::None,
        [](const godot_autopilot::util::JsonValue &) {
          godot_autopilot::util::JsonValue result(object());
          result["real"] = godot_autopilot::util::JsonValue(true);
          return result;
        });
    registry_->add(godot_autopilot::make_spec_tool(std::move(real)));

    godot_autopilot::refresh_derived(registry_, catalog_, index_);
    queue_.drain();
  }

  void TearDown() override {
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::clear_active_registry();
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
  }

  godot_autopilot::util::JsonValue call(const std::string &name, const godot_autopilot::util::JsonValue &args) {
    return godot_autopilot::dispatch::call_handler(name, args);
  }

  CommandQueue queue_;
  ToolCatalog catalog_;
  Bm25Index index_;
  std::shared_ptr<ToolRegistry> registry_ = std::make_shared<ToolRegistry>();
};

TEST_F(AliasResolutionTest, RenameAliasReturnsCanonicalResult) {
  const godot_autopilot::util::JsonValue result = call("old_echo_name", object());
  EXPECT_EQ(string_field(result, "deprecated_alias"), "old_echo_name");
  EXPECT_EQ(string_field(result, "canonical_name"), "alias_target_echo");
  EXPECT_EQ(string_field(result, "alias_removal_version"), "0.6.0");
  EXPECT_EQ(string_field(result, "deprecated_since"), "0.4.0");
  const godot_autopilot::util::JsonValue *echo = result.Find("echo");
  ASSERT_NE(echo, nullptr);
  EXPECT_TRUE(echo->IsObject());
}

TEST_F(AliasResolutionTest, ParamRenameAliasAdaptsArgs) {
  godot_autopilot::util::JsonValue args = object();
  args["old_key"] = godot_autopilot::util::JsonValue("value");
  const godot_autopilot::util::JsonValue result = call("old_echo_param", args);
  const godot_autopilot::util::JsonValue *echo = result.Find("echo");
  ASSERT_NE(echo, nullptr);
  EXPECT_EQ(string_field(*echo, "new_key"), "value");
  EXPECT_EQ(echo->Find("old_key"), nullptr);
}

TEST_F(AliasResolutionTest, ScalarToArrayAliasWrapsValue) {
  godot_autopilot::util::JsonValue args = object();
  args["item"] = godot_autopilot::util::JsonValue("x");
  const godot_autopilot::util::JsonValue result = call("old_echo_scalar", args);
  const godot_autopilot::util::JsonValue *echo = result.Find("echo");
  ASSERT_NE(echo, nullptr);
  const godot_autopilot::util::JsonValue *items = echo->Find("items");
  ASSERT_NE(items, nullptr);
  ASSERT_TRUE(items->IsArray());
  ASSERT_EQ(items->GetArray().size(), 1u);
  EXPECT_EQ(items->GetArray()[0].GetString(), "x");
}

TEST_F(AliasResolutionTest, ProjectionRestoresOldShape) {
  const godot_autopilot::util::JsonValue result = call("old_profile", object());
  const godot_autopilot::util::JsonValue *projected = result.Find("result");
  ASSERT_NE(projected, nullptr);
  EXPECT_EQ(projected->GetInt(), 42);
  EXPECT_EQ(result.Find("value"), nullptr);
  EXPECT_EQ(string_field(result, "deprecated_alias"), "old_profile");
}

TEST_F(AliasResolutionTest, DestructiveAliasRequiresConfirm) {
  godot_autopilot::util::JsonValue denied_args = object();
  denied_args["payload"] = godot_autopilot::util::JsonValue("data");
  const godot_autopilot::util::JsonValue denied = call("old_destructive", denied_args);
  EXPECT_EQ(string_field(denied, "code"), "confirmation_required");

  godot_autopilot::util::JsonValue allowed_args = object();
  allowed_args["payload"] = godot_autopilot::util::JsonValue("data");
  allowed_args["confirm"] = godot_autopilot::util::JsonValue(true);
  const godot_autopilot::util::JsonValue allowed = call("old_destructive", allowed_args);
  EXPECT_EQ(string_field(allowed, "result"), "ok");
  EXPECT_EQ(string_field(allowed, "deprecated_alias"), "old_destructive");
}

TEST_F(AliasResolutionTest, ConflictIsRejectedAndRealToolWins) {
  EXPECT_FALSE(registry_->resolve_alias("alias_target_echo").has_value());
  EXPECT_NE(registry_->find("alias_target_echo"), nullptr);
  EXPECT_FALSE(registry_->resolve_alias("future_real").has_value());
  EXPECT_NE(registry_->find("future_real"), nullptr);
  EXPECT_EQ(registry_->alias_count(), 5u);
}

TEST_F(AliasResolutionTest, CatalogResolvesAliasButListsOnlyRealTools) {
  const auto info = catalog_.get_tool("old_echo_name");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->name, "alias_target_echo");

  const auto tools = catalog_.get_all_tools();
  EXPECT_EQ(tools.size(), catalog_.size());
  for (const auto &tool : tools) {
    EXPECT_NE(tool.name, "old_echo_name");
  }
  EXPECT_EQ(index_.size(), catalog_.size());
  Bm25Index::SearchQuery query;
  query.text = "old_echo_name";
  query.max_results = 20;
  for (const auto &hit : index_.search(query)) {
    EXPECT_NE(hit.name, "old_echo_name");
  }
}

TEST_F(AliasResolutionTest, ToolDetailReportsAliasMetadata) {
  const godot_autopilot::util::JsonValue detail = godot_autopilot::tool_detail_for(
      "old_echo_name", catalog_, registry_);
  std::cout << "[alias-detail] " << detail.Dump() << "\n";
  EXPECT_EQ(string_field(detail, "deprecated_alias"), "old_echo_name");
  EXPECT_EQ(string_field(detail, "canonical_name"), "alias_target_echo");
  EXPECT_EQ(string_field(detail, "alias_removal_version"), "0.6.0");
  const godot_autopilot::util::JsonValue *tool = detail.Find("tool");
  ASSERT_NE(tool, nullptr);
  EXPECT_EQ(string_field(*tool, "name"), "alias_target_echo");
  EXPECT_EQ(string_field(*tool, "side_effect"), "none");
  const godot_autopilot::util::JsonValue *examples = tool->Find("examples");
  ASSERT_NE(examples, nullptr);
  ASSERT_TRUE(examples->IsArray());
  ASSERT_EQ(examples->Size(), 1u);
  EXPECT_EQ((*examples)[0].GetString(), "{\"old_key\":\"value\"}");
}

} // namespace
