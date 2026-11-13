#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarness;

const char *const kMetaCategory = "Meta";
const char *const kSideEffectNone = "none";
const char *const kGroupCandidate = "Candidate";
const char *const kGroupMutating = "Mutating";
const char *const kGroupDynamic = "Dynamic";

struct ToolFacts {
  std::string name;
  std::string category;
  std::string side_effect;
  bool mutating = false;
  bool dynamic = false;
  bool has_required = false;
  bool complete = false;
};

mcp::JsonValue empty_object() {
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

mcp::JsonValue call_search_all(IntegrationHarness &harness) {
  mcp::JsonValue args = empty_object();
  args["limit"] = mcp::JsonValue(static_cast<int64_t>(0));
  return harness.call("search_tools", args);
}

mcp::JsonValue call_tool_detail(IntegrationHarness &harness,
                                const std::string &name) {
  mcp::JsonValue args = empty_object();
  args["name"] = mcp::JsonValue(name);
  return harness.call("get_tool_detail", args);
}

std::vector<std::string> enumerate_tool_names(IntegrationHarness &harness) {
  std::vector<std::string> names;
  const mcp::JsonValue result = call_search_all(harness);
  const auto *results = result.Find("results");
  if (!results || !results->IsArray()) {
    return names;
  }
  for (const auto &item : results->GetArray()) {
    if (!item.IsObject()) {
      continue;
    }
    const auto *name = item.Find("name");
    if (!name || !name->IsString() || name->GetString().empty()) {
      continue;
    }
    names.push_back(name->GetString());
  }
  return names;
}

ToolFacts read_tool_facts(IntegrationHarness &harness,
                          const std::string &name) {
  ToolFacts facts;
  facts.name = name;
  const mcp::JsonValue detail = call_tool_detail(harness, name);
  const auto *tool = detail.Find("tool");
  if (!tool || !tool->IsObject()) {
    return facts;
  }
  facts.complete = true;
  if (const auto *category = tool->Find("category");
      category && category->IsString()) {
    facts.category = category->GetString();
  }
  if (const auto *side_effect = tool->Find("side_effect");
      side_effect && side_effect->IsString()) {
    facts.side_effect = side_effect->GetString();
  }
  if (const auto *mutating = tool->Find("mutating");
      mutating && mutating->IsBool()) {
    facts.mutating = mutating->GetBool();
  }
  if (const auto *dynamic_flag = tool->Find("dynamic");
      dynamic_flag && dynamic_flag->IsBool()) {
    facts.dynamic = dynamic_flag->GetBool();
  }
  const auto *schema = tool->Find("input_schema");
  if (schema && schema->IsObject()) {
    if (const auto *required = schema->Find("required");
        required && required->IsArray()) {
      facts.has_required = !required->GetArray().empty();
    }
  }
  return facts;
}

std::vector<ToolFacts> collect_domain_facts(IntegrationHarness &harness) {
  std::vector<ToolFacts> facts;
  for (const auto &name : enumerate_tool_names(harness)) {
    ToolFacts item = read_tool_facts(harness, name);
    if (item.complete && item.category != kMetaCategory) {
      facts.push_back(std::move(item));
    }
  }
  return facts;
}

std::string call_group_of(const ToolFacts &facts) {
  if (!facts.side_effect.empty() && facts.side_effect != kSideEffectNone) {
    return facts.side_effect;
  }
  if (facts.mutating) {
    return kGroupMutating;
  }
  if (facts.dynamic) {
    return kGroupDynamic;
  }
  return kGroupCandidate;
}

std::string error_text_of(const mcp::JsonValue &response) {
  const auto *error = response.Find("error");
  if (error && error->IsString()) {
    return error->GetString();
  }
  return std::string();
}

bool is_authorization_denied(const mcp::JsonValue &response) {
  (void)response;
  return false;
}

void expect_missing_parameter_error(IntegrationHarness &harness,
                                    const ToolFacts &facts) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  args["confirm"] = mcp::JsonValue(true);
  const mcp::JsonValue response = harness.call(facts.name, args);
  EXPECT_TRUE(response.IsObject()) << facts.name << " -> " << response.Dump();
  if (!response.IsObject()) {
    return;
  }
  const auto *error = response.Find("error");
  ASSERT_NE(error, nullptr) << facts.name << " -> " << response.Dump();
  ASSERT_TRUE(error->IsString()) << facts.name << " -> " << response.Dump();
  EXPECT_NE(error->GetString().find("missing"), std::string::npos)
      << facts.name << " -> " << error->GetString();
}

void expect_authorization_error(IntegrationHarness &harness,
                                const ToolFacts &facts) {
  const mcp::JsonValue response = harness.call(facts.name, empty_object());
  EXPECT_TRUE(response.IsObject()) << facts.name << " -> " << response.Dump();
  if (!response.IsObject()) {
    return;
  }
  EXPECT_TRUE(is_authorization_denied(response))
      << facts.name << " -> " << response.Dump();
}

void run_empty_arg_shard(IntegrationHarness &harness, const char *group,
                         bool expect_missing_parameter, int shard, int total) {
  const std::vector<ToolFacts> all = collect_domain_facts(harness);
  ASSERT_FALSE(all.empty());
  std::vector<ToolFacts> targets;
  for (const auto &facts : all) {
    if (facts.has_required && call_group_of(facts) == group) {
      targets.push_back(facts);
    }
  }
  size_t invoked = 0;
  for (size_t i = 0; i < targets.size(); ++i) {
    if (static_cast<int>(i) % total != shard) {
      continue;
    }
    ++invoked;
    expect_missing_parameter_error(harness, targets[i]);
  }
  GTEST_SUCCEED() << "group=" << group << " shard=" << shard << "/" << total
                  << " enumerated=" << all.size()
                  << " targets=" << targets.size()
                  << " invoked=" << invoked;
}

void run_empty_arg_group(IntegrationHarness &harness, const char *group,
                         bool expect_missing_parameter) {
  run_empty_arg_shard(harness, group, expect_missing_parameter, 0, 1);
}

TEST_F(IntegrationHarness, SearchToolsEmptyQueryEnumeratesAllTools) {
  const mcp::JsonValue result = call_search_all(*this);
  ASSERT_TRUE(result.IsObject()) << result.Dump();
  const auto *results = result.Find("results");
  ASSERT_NE(results, nullptr) << result.Dump();
  ASSERT_TRUE(results->IsArray()) << result.Dump();
  EXPECT_FALSE(results->GetArray().empty());
  std::size_t count = 0;
  for (const auto &item : results->GetArray()) {
    EXPECT_TRUE(item.IsObject()) << item.Dump();
    const auto *name = item.Find("name");
    EXPECT_NE(name, nullptr) << item.Dump();
    EXPECT_TRUE(name && name->IsString() && !name->GetString().empty())
        << item.Dump();
    const auto *score = item.Find("score");
    EXPECT_NE(score, nullptr) << item.Dump();
    EXPECT_TRUE(score && score->IsNumber()) << item.Dump();
    if (item.IsObject() && name && name->IsString() && !name->GetString().empty()) {
      ++count;
    }
  }
  GTEST_SUCCEED() << "enumerated=" << count;
}

TEST_F(IntegrationHarness, MetaToolsDistinguishableByCategoryField) {
  const std::vector<std::string> names = enumerate_tool_names(*this);
  ASSERT_FALSE(names.empty());
  std::set<std::string> categories;
  std::vector<std::string> meta_names;
  for (const auto &name : names) {
    const mcp::JsonValue detail = call_tool_detail(*this, name);
    const auto *tool = detail.Find("tool");
    EXPECT_NE(tool, nullptr) << name << " -> " << detail.Dump();
    if (!tool) {
      continue;
    }
    const auto *category = tool->Find("category");
    EXPECT_NE(category, nullptr) << name << " -> " << detail.Dump();
    EXPECT_TRUE(category && category->IsString())
        << name << " -> " << detail.Dump();
    if (!category || !category->IsString()) {
      continue;
    }
    EXPECT_FALSE(category->GetString().empty()) << name;
    categories.insert(category->GetString());
    if (category->GetString() == kMetaCategory) {
      const auto *tool_name = tool->Find("name");
      EXPECT_TRUE(tool_name && tool_name->IsString() &&
                  tool_name->GetString() == name)
          << name;
      meta_names.push_back(name);
    }
  }
  EXPECT_FALSE(meta_names.empty())
      << "no enumerated tool carries category \"" << kMetaCategory
      << "\"; runtime fields cannot separate meta tools";
  GTEST_SUCCEED() << "meta_count=" << meta_names.size()
                  << " distinct_categories=" << categories.size();
}

TEST_F(IntegrationHarness, DetailContractForEveryEnumeratedTool) {
  const std::vector<std::string> names = enumerate_tool_names(*this);
  ASSERT_FALSE(names.empty());
  std::size_t checked = 0;
  for (const auto &name : names) {
    const mcp::JsonValue detail = call_tool_detail(*this, name);
    EXPECT_TRUE(detail.IsObject()) << name << " -> " << detail.Dump();
    const auto *tool = detail.Find("tool");
    EXPECT_NE(tool, nullptr) << name << " -> " << detail.Dump();
    EXPECT_TRUE(tool && tool->IsObject()) << name;
    if (!detail.IsObject() || !tool || !tool->IsObject()) {
      continue;
    }
    const auto *tool_name = tool->Find("name");
    EXPECT_TRUE(tool_name && tool_name->IsString()) << name;
    if (tool_name && tool_name->IsString()) {
      EXPECT_EQ(tool_name->GetString(), name);
    }
    const auto *schema = tool->Find("input_schema");
    EXPECT_NE(schema, nullptr) << name << " -> " << detail.Dump();
    EXPECT_TRUE(schema && schema->IsObject()) << name;
    const auto *side_effect = tool->Find("side_effect");
    EXPECT_TRUE(side_effect && side_effect->IsString() &&
                !side_effect->GetString().empty())
        << name;
    const auto *mutating = tool->Find("mutating");
    EXPECT_TRUE(mutating && mutating->IsBool()) << name;
    const auto *dynamic_flag = tool->Find("dynamic");
    EXPECT_TRUE(dynamic_flag && dynamic_flag->IsBool()) << name;
    ++checked;
  }
  GTEST_SUCCEED() << "checked=" << checked;
}

TEST_F(IntegrationHarness, DetailContractUnknownNameReportsToolNotFound) {
  const mcp::JsonValue detail =
      call_tool_detail(*this, "gda_contract_no_such_tool_probe");
  const auto *error = detail.Find("error");
  ASSERT_NE(error, nullptr) << detail.Dump();
  ASSERT_TRUE(error->IsString()) << detail.Dump();
  EXPECT_NE(error->GetString().find("tool not found"), std::string::npos)
      << error->GetString();
}

TEST_F(IntegrationHarness, ExclusionPartitionCoversAllEnumeratedTools) {
  const std::vector<ToolFacts> facts = collect_domain_facts(*this);
  ASSERT_FALSE(facts.empty());
  std::set<std::string> all_names;
  std::set<std::string> excluded;
  std::set<std::string> candidates;
  for (const auto &item : facts) {
    all_names.insert(item.name);
    const bool has_side_effect =
        !item.side_effect.empty() && item.side_effect != kSideEffectNone;
    if (has_side_effect || item.mutating || item.dynamic) {
      excluded.insert(item.name);
    } else {
      candidates.insert(item.name);
    }
  }
  std::size_t overlap = 0;
  for (const auto &name : excluded) {
    if (candidates.count(name) != 0) {
      ++overlap;
    }
  }
  EXPECT_EQ(overlap, std::size_t(0));
  EXPECT_EQ(excluded.size() + candidates.size(), all_names.size());
  std::size_t covered = 0;
  for (const auto &name : all_names) {
    if (excluded.count(name) != 0 || candidates.count(name) != 0) {
      ++covered;
    }
  }
  EXPECT_EQ(covered, all_names.size());
  GTEST_SUCCEED() << "enumerated=" << all_names.size()
                  << " excluded=" << excluded.size()
                  << " candidates=" << candidates.size();
}

TEST_F(IntegrationHarness, EmptyArgCallRejects_Mutating_0) {
  run_empty_arg_group(*this, kGroupMutating, true);
}

TEST_F(IntegrationHarness, EmptyArgCallRejects_Dynamic_0) {
  run_empty_arg_group(*this, kGroupDynamic, true);
}

} // namespace
