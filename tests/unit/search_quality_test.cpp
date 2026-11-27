#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/command_queue.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/tool_catalog.hpp"
#include "tools/base/tool_registry.hpp"
#include "util/bm25_index.hpp"

namespace {

struct SearchQueryEntry {
  std::string id;
  std::string text;
  std::vector<std::string> expect_any;
};

struct BaselineEntry {
  std::optional<int64_t> best_rank;
  std::optional<bool> hit;
};

struct SearchBaseline {
  bool present = false;
  std::optional<int64_t> hit_count;
  std::optional<int64_t> query_count;
  std::unordered_map<std::string, BaselineEntry> entries;
};

godot_autopilot::util::JsonValue load_json(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to open " + path.generic_string());
  }
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  try {
    return godot_autopilot::util::JsonValue::Parse(text);
  } catch (const std::exception &ex) {
    throw std::runtime_error("failed to parse " + path.generic_string() +
                             ": " + ex.what());
  }
}

const godot_autopilot::util::JsonValue *require_member(const godot_autopilot::util::JsonValue &object,
                                     const char *key,
                                     const std::string &context) {
  const godot_autopilot::util::JsonValue *value = object.Find(key);
  if (value == nullptr) {
    throw std::runtime_error(context + " is missing member: " + key);
  }
  return value;
}

std::string require_string(const godot_autopilot::util::JsonValue &object, const char *key,
                           const std::string &context) {
  const godot_autopilot::util::JsonValue *value = require_member(object, key, context);
  if (!value->IsString()) {
    throw std::runtime_error(context + " member is not a string: " + key);
  }
  return value->GetString();
}

int64_t require_int(const godot_autopilot::util::JsonValue &object, const char *key,
                    const std::string &context) {
  const godot_autopilot::util::JsonValue *value = require_member(object, key, context);
  if (!value->IsInt()) {
    throw std::runtime_error(context + " member is not an integer: " + key);
  }
  return value->GetInt();
}

std::optional<std::filesystem::path>
find_optional_file(const std::string &relative) {
  std::filesystem::path current = std::filesystem::current_path();
  while (true) {
    const std::filesystem::path candidate =
        current / std::filesystem::path(relative);
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
    if (!current.has_parent_path()) {
      break;
    }
    const std::filesystem::path parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return std::nullopt;
}

std::filesystem::path require_data_file(const std::string &relative) {
  const std::optional<std::filesystem::path> found = find_optional_file(relative);
  if (!found.has_value()) {
    throw std::runtime_error(relative + " not found from " +
                             std::filesystem::current_path().string());
  }
  return *found;
}

std::vector<SearchQueryEntry>
load_search_queries(const std::filesystem::path &path) {
  const godot_autopilot::util::JsonValue root = load_json(path);
  const godot_autopilot::util::JsonValue *queries =
      require_member(root, "queries", path.generic_string());
  if (!queries->IsArray()) {
    throw std::runtime_error(path.generic_string() +
                             " member is not an array: queries");
  }
  std::vector<SearchQueryEntry> entries;
  for (const godot_autopilot::util::JsonValue &item : queries->GetArray()) {
    if (!item.IsObject()) {
      throw std::runtime_error(path.generic_string() +
                               " query entry is not an object");
    }
    SearchQueryEntry entry;
    entry.id = require_string(item, "id", "search query entry");
    if (entry.id.empty()) {
      throw std::runtime_error("search query entry has an empty id");
    }
    entry.text = require_string(item, "query", "search query " + entry.id);
    if (entry.text.empty()) {
      throw std::runtime_error("search query text is empty: " + entry.id);
    }
    const godot_autopilot::util::JsonValue *expect =
        require_member(item, "expect_any", "search query " + entry.id);
    if (!expect->IsArray()) {
      throw std::runtime_error("search query expect_any is not an array: " +
                               entry.id);
    }
    for (const godot_autopilot::util::JsonValue &name : expect->GetArray()) {
      if (!name.IsString() || name.GetString().empty()) {
        throw std::runtime_error(
            "search query expect_any holds an empty or non-string entry: " +
            entry.id);
      }
      entry.expect_any.push_back(name.GetString());
    }
    if (entry.expect_any.empty()) {
      throw std::runtime_error("search query has no expect_any entries: " +
                               entry.id);
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

std::optional<std::filesystem::path> find_search_baseline() {
  const char *const kCandidates[] = {
      "evidence/T-03/search-baseline.json",
      ".handoff/evidence/T-03/search-baseline.json"};
  for (const char *candidate : kCandidates) {
    const std::optional<std::filesystem::path> found =
        find_optional_file(candidate);
    if (found.has_value()) {
      return found;
    }
  }
  return std::nullopt;
}

SearchBaseline load_search_baseline() {
  SearchBaseline baseline;
  const std::optional<std::filesystem::path> path = find_search_baseline();
  if (!path.has_value()) {
    return baseline;
  }
  baseline.present = true;
  const godot_autopilot::util::JsonValue root = load_json(*path);
  if (!root.IsObject()) {
    throw std::runtime_error("search baseline root is not an object: " +
                             path->generic_string());
  }
  if (const godot_autopilot::util::JsonValue *hit_count = root.Find("hit_count");
      hit_count != nullptr) {
    if (!hit_count->IsInt() || hit_count->GetInt() < 0) {
      throw std::runtime_error(
          "search baseline hit_count is not a non-negative integer: " +
          path->generic_string());
    }
    baseline.hit_count = hit_count->GetInt();
  }
  if (const godot_autopilot::util::JsonValue *query_count = root.Find("query_count");
      query_count != nullptr) {
    if (!query_count->IsInt() || query_count->GetInt() < 0) {
      throw std::runtime_error(
          "search baseline query_count is not a non-negative integer: " +
          path->generic_string());
    }
    baseline.query_count = query_count->GetInt();
  }
  const godot_autopilot::util::JsonValue *entries = root.Find("queries");
  if (entries == nullptr) {
    entries = root.Find("results");
  }
  if (entries != nullptr) {
    if (!entries->IsArray()) {
      throw std::runtime_error(
          "search baseline queries is not an array: " + path->generic_string());
    }
    for (const godot_autopilot::util::JsonValue &item : entries->GetArray()) {
      if (!item.IsObject()) {
        throw std::runtime_error("search baseline query entry is not an object: " +
                                 path->generic_string());
      }
      const std::string id =
          require_string(item, "id", "search baseline query entry");
      BaselineEntry entry;
      if (const godot_autopilot::util::JsonValue *best_rank = item.Find("best_rank");
          best_rank != nullptr) {
        if (!best_rank->IsInt() || best_rank->GetInt() < 0) {
          throw std::runtime_error(
              "search baseline best_rank is not a non-negative integer: " + id);
        }
        entry.best_rank = best_rank->GetInt();
      }
      if (const godot_autopilot::util::JsonValue *hit = item.Find("hit"); hit != nullptr) {
        if (!hit->IsBool()) {
          throw std::runtime_error("search baseline hit is not a boolean: " +
                                   id);
        }
        entry.hit = hit->GetBool();
      }
      baseline.entries[id] = std::move(entry);
    }
  }
  if (!baseline.hit_count.has_value() && !baseline.query_count.has_value() &&
      baseline.entries.empty()) {
    throw std::runtime_error("unrecognized search baseline schema: " +
                             path->generic_string());
  }
  return baseline;
}

std::string join_names(const std::vector<std::string> &names) {
  std::string text;
  for (const std::string &name : names) {
    if (!text.empty()) {
      text += ",";
    }
    text += name;
  }
  return text;
}

std::string join_results(
    const std::vector<godot_autopilot::Bm25Result> &results) {
  std::string text;
  for (const godot_autopilot::Bm25Result &result : results) {
    if (!text.empty()) {
      text += ",";
    }
    text += result.name + ":" + std::to_string(result.score);
  }
  return text;
}

void compare_with_baseline(
    const SearchBaseline &baseline,
    const std::vector<SearchQueryEntry> &queries,
    const std::unordered_map<std::string, int> &best_ranks,
    std::size_t hit_count) {
  if (!baseline.present) {
    return;
  }
  if (baseline.query_count.has_value() &&
      static_cast<std::size_t>(*baseline.query_count) > queries.size()) {
    ADD_FAILURE() << "search query coverage dropped: baseline="
                  << *baseline.query_count << " current=" << queries.size();
  }
  if (baseline.hit_count.has_value() &&
      hit_count < static_cast<std::size_t>(*baseline.hit_count)) {
    ADD_FAILURE() << "search hit count dropped: baseline="
                  << *baseline.hit_count << " current=" << hit_count;
  }
  std::unordered_set<std::string> current_ids;
  current_ids.reserve(queries.size());
  for (const SearchQueryEntry &query : queries) {
    current_ids.insert(query.id);
  }
  for (const auto &[id, entry] : baseline.entries) {
    if (current_ids.find(id) == current_ids.end()) {
      ADD_FAILURE() << "baseline query is missing from search_queries.json: "
                    << id;
      continue;
    }
    const auto rank = best_ranks.find(id);
    const bool current_hit = rank != best_ranks.end();
    if (entry.hit.has_value() && *entry.hit && !current_hit) {
      ADD_FAILURE() << "baseline hit regressed to a miss: " << id;
    }
    if (entry.best_rank.has_value() && *entry.best_rank > 0) {
      if (!current_hit) {
        ADD_FAILURE() << "baseline rank regressed to a miss: " << id
                      << " baseline_rank=" << *entry.best_rank;
      } else if (rank->second > *entry.best_rank) {
        ADD_FAILURE() << "baseline rank regressed: " << id
                      << " baseline_rank=" << *entry.best_rank
                      << " current_rank=" << rank->second;
      }
    }
  }
}

struct SearchQualityTest : ::testing::Test {
  godot_autopilot::CommandQueue queue;
  godot_autopilot::ToolCatalog catalog;
  godot_autopilot::Bm25Index index;

  void SetUp() override {
    godot_autopilot::install_tool_handlers(queue, catalog, index, 9540);
    queue.drain();
  }

  void TearDown() override {
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::clear_active_registry();
  }
};

TEST_F(SearchQualityTest, StandardQueriesHitTopTen) {
  const std::filesystem::path queries_path =
      require_data_file("tests/data/search_queries.json");
  const std::vector<SearchQueryEntry> queries =
      load_search_queries(queries_path);
  ASSERT_GE(queries.size(), 50u)
      << "search_queries.json must keep at least 50 standard queries";

  const SearchBaseline baseline = load_search_baseline();
  std::unordered_map<std::string, int> best_ranks;
  std::size_t hit_count = 0;
  std::size_t unresolved_expected = 0;

  for (const SearchQueryEntry &query : queries) {
    std::vector<std::string> accepted;
    accepted.reserve(query.expect_any.size());
    for (const std::string &expected : query.expect_any) {
      const std::optional<godot_autopilot::ToolInfo> resolved =
          catalog.get_tool(expected);
      if (resolved.has_value()) {
        accepted.push_back(resolved->name);
      } else {
        accepted.push_back(expected);
        ++unresolved_expected;
        EXPECT_TRUE(resolved.has_value())
            << "query id=" << query.id << " expect_any entry is neither a tool "
            << "nor an alias: " << expected;
      }
    }

    godot_autopilot::Bm25Index::SearchQuery search;
    search.text = query.text;
    search.max_results = 10;
    const std::vector<godot_autopilot::Bm25Result> results =
        index.search(search);

    int best_rank = 0;
    std::string best_name;
    for (std::size_t position = 0; position < results.size(); ++position) {
      if (std::find(accepted.begin(), accepted.end(), results[position].name) !=
          accepted.end()) {
        best_rank = static_cast<int>(position + 1);
        best_name = results[position].name;
        break;
      }
    }
    if (best_rank > 0) {
      ++hit_count;
      best_ranks[query.id] = best_rank;
    }
    std::cout << "[search-quality] id=" << query.id << " rank=" << best_rank
              << " hit=" << best_name
              << " expected_raw=" << join_names(query.expect_any)
              << " expected=" << join_names(accepted)
              << " top10=" << join_results(results) << "\n";
    EXPECT_GT(best_rank, 0)
        << "query id=" << query.id << " text=\"" << query.text
        << "\" expected_raw=" << join_names(query.expect_any)
        << " expected=" << join_names(accepted)
        << " top10=" << join_results(results);
  }

  std::cout << "[search-quality] total=" << queries.size()
            << " hits=" << hit_count
            << " unresolved_expected=" << unresolved_expected << "\n";
  RecordProperty("search_quality_total", std::to_string(queries.size()));
  RecordProperty("search_quality_hits", std::to_string(hit_count));
  EXPECT_EQ(hit_count, queries.size());
  EXPECT_EQ(unresolved_expected, 0u);
  compare_with_baseline(baseline, queries, best_ranks, hit_count);
}

} // namespace
