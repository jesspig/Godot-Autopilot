#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

constexpr std::size_t kHistoricalDomainToolFloor = 451;

const char *const kAllowedStatuses[] = {"kept", "mapped", "alias", "merged"};

mcp::JsonValue load_json(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to open " + path.generic_string());
  }
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  try {
    return mcp::JsonValue::Parse(text);
  } catch (const std::exception &ex) {
    throw std::runtime_error("failed to parse " + path.generic_string() +
                             ": " + ex.what());
  }
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
  const std::optional<std::filesystem::path> found =
      find_optional_file(relative);
  if (!found.has_value()) {
    throw std::runtime_error(relative + " not found from " +
                             std::filesystem::current_path().string());
  }
  return *found;
}

const mcp::JsonValue *require_member(const mcp::JsonValue &object,
                                     const char *key,
                                     const std::string &context) {
  const mcp::JsonValue *value = object.Find(key);
  if (value == nullptr) {
    throw std::runtime_error(context + " is missing member: " + key);
  }
  return value;
}

std::string require_string(const mcp::JsonValue &object, const char *key,
                           const std::string &context) {
  const mcp::JsonValue *value = require_member(object, key, context);
  if (!value->IsString()) {
    throw std::runtime_error(context + " member is not a string: " + key);
  }
  return value->GetString();
}

int64_t require_int(const mcp::JsonValue &object, const char *key,
                    const std::string &context) {
  const mcp::JsonValue *value = require_member(object, key, context);
  if (!value->IsInt()) {
    throw std::runtime_error(context + " member is not an integer: " + key);
  }
  return value->GetInt();
}

bool is_allowed_status(const std::string &status) {
  for (const char *allowed : kAllowedStatuses) {
    if (status == allowed) {
      return true;
    }
  }
  return false;
}

std::string join_tools(const std::vector<std::string> &tools) {
  std::string text;
  for (const std::string &tool : tools) {
    if (!text.empty()) {
      text += ",";
    }
    text += tool;
  }
  return text;
}

} // namespace

TEST(CapabilityMatrixTest, HistoricalToolsStayMappedOrKept) {
  const std::filesystem::path matrix_path =
      require_data_file("tests/data/capability_matrix.json");
  const mcp::JsonValue matrix = load_json(matrix_path);
  const mcp::JsonValue *rows =
      require_member(matrix, "rows", matrix_path.generic_string());
  if (!rows->IsArray()) {
    throw std::runtime_error(matrix_path.generic_string() +
                             " member is not an array: rows");
  }
  const auto &domain_rows = rows->GetArray();
  ASSERT_GE(domain_rows.size(), kHistoricalDomainToolFloor)
      << "capability matrix lost historical domain tools: rows="
      << domain_rows.size() << " floor=" << kHistoricalDomainToolFloor;

  const int64_t declared_rows =
      require_int(matrix, "row_count", matrix_path.generic_string());
  EXPECT_EQ(static_cast<std::size_t>(declared_rows), domain_rows.size())
      << "capability matrix row_count does not match rows";
  const int64_t declared_unique =
      require_int(matrix, "unique_tool_count", matrix_path.generic_string());
  EXPECT_EQ(static_cast<std::size_t>(declared_unique), domain_rows.size())
      << "capability matrix unique_tool_count does not match rows";

  std::unordered_set<std::string> old_tools;
  old_tools.reserve(domain_rows.size());
  std::vector<std::string> missing;
  std::vector<std::string> unknown;

  for (const mcp::JsonValue &row : domain_rows) {
    if (!row.IsObject()) {
      ADD_FAILURE() << "capability matrix row is not an object";
      continue;
    }
    const std::string old_tool =
        require_string(row, "old_tool", "capability matrix row");
    const std::string new_tool =
        require_string(row, "new_tool", "capability matrix row " + old_tool);
    const std::string status =
        require_string(row, "status", "capability matrix row " + old_tool);
    EXPECT_FALSE(old_tool.empty())
        << "capability matrix row has an empty old_tool";
    EXPECT_FALSE(new_tool.empty())
        << "capability matrix row has an empty new_tool: " << old_tool;
    EXPECT_TRUE(old_tools.insert(old_tool).second)
        << "duplicate old_tool in capability matrix: " << old_tool;
    if (status == "missing") {
      missing.push_back(old_tool);
    } else if (!is_allowed_status(status)) {
      unknown.push_back(old_tool + ":" + status);
    }
  }

  EXPECT_EQ(old_tools.size(), domain_rows.size())
      << "capability matrix old_tool values are not unique";
  if (const mcp::JsonValue *meta_rows = matrix.Find("meta_rows");
      meta_rows != nullptr) {
    if (!meta_rows->IsArray()) {
      throw std::runtime_error(matrix_path.generic_string() +
                               " member is not an array: meta_rows");
    }
    for (const mcp::JsonValue &row : meta_rows->GetArray()) {
      if (!row.IsObject()) {
        ADD_FAILURE() << "capability matrix meta row is not an object";
        continue;
      }
      const std::string old_tool =
          require_string(row, "old_tool", "capability matrix meta row");
      const std::string status =
          require_string(row, "status", "capability matrix meta row " + old_tool);
      if (status == "missing") {
        missing.push_back(old_tool);
      } else if (!is_allowed_status(status)) {
        unknown.push_back(old_tool + ":" + status);
      }
    }
  }

  EXPECT_TRUE(missing.empty())
      << "capability matrix has missing old tools: " << join_tools(missing);
  EXPECT_TRUE(unknown.empty())
      << "capability matrix has unknown statuses: " << join_tools(unknown);
  RecordProperty("capability_matrix_rows", std::to_string(old_tools.size()));
  RecordProperty("capability_matrix_missing", std::to_string(missing.size()));
  RecordProperty("capability_matrix_unknown", std::to_string(unknown.size()));
  if (const mcp::JsonValue *declared_missing = matrix.Find("domain_missing_count");
      declared_missing != nullptr) {
    if (!declared_missing->IsInt()) {
      throw std::runtime_error(
          "capability matrix domain_missing_count is not an integer");
    }
    EXPECT_EQ(declared_missing->GetInt(), 0);
  }
}

TEST(CapabilityMatrixTest, UniqueOldToolCountMatchesRowCount) {
  const std::filesystem::path matrix_path =
      require_data_file("tests/data/capability_matrix.json");
  const mcp::JsonValue matrix = load_json(matrix_path);
  const mcp::JsonValue *rows =
      require_member(matrix, "rows", matrix_path.generic_string());
  if (!rows->IsArray()) {
    throw std::runtime_error(matrix_path.generic_string() +
                             " member is not an array: rows");
  }
  std::unordered_set<std::string> old_tools;
  for (const mcp::JsonValue &row : rows->GetArray()) {
    if (!row.IsObject()) {
      ADD_FAILURE() << "capability matrix row is not an object";
      continue;
    }
    old_tools.insert(require_string(row, "old_tool", "capability matrix row"));
  }
  EXPECT_EQ(old_tools.size(), rows->GetArray().size());
  const int64_t declared_unique =
      require_int(matrix, "unique_tool_count", matrix_path.generic_string());
  EXPECT_EQ(static_cast<std::size_t>(declared_unique), old_tools.size());
  const int64_t declared_rows =
      require_int(matrix, "row_count", matrix_path.generic_string());
  EXPECT_EQ(static_cast<std::size_t>(declared_rows), rows->GetArray().size());
}
