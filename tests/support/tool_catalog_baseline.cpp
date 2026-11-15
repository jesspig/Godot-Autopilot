#include "support/tool_catalog_baseline.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <mcp/JsonValue.hpp>

namespace {

std::filesystem::path find_baseline_path() {
  std::filesystem::path current = std::filesystem::current_path();
  while (true) {
    const std::filesystem::path candidate =
        current / "tests" / "data" / "tool_catalog_baseline.json";
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
  throw std::runtime_error(
      "tool_catalog_baseline.json not found from " +
      std::filesystem::current_path().string());
}

std::size_t require_int(const mcp::JsonValue &root, const char *key) {
  const mcp::JsonValue *value = root.Find(key);
  if (value == nullptr || !value->IsInt() || value->GetInt() < 0) {
    throw std::runtime_error(
        std::string("tool catalog baseline requires a non-negative integer: ") +
        key);
  }
  return static_cast<std::size_t>(value->GetInt());
}

} // namespace

namespace gda_test {

const ToolCatalogBaseline &load_tool_catalog_baseline() {
  static const ToolCatalogBaseline baseline = [] {
    const std::filesystem::path path = find_baseline_path();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
      throw std::runtime_error("failed to open tool catalog baseline: " +
                               path.string());
    }
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    mcp::JsonValue root;
    try {
      root = mcp::JsonValue::Parse(text);
    } catch (const std::exception &ex) {
      throw std::runtime_error("failed to parse tool catalog baseline " +
                               path.string() + ": " + ex.what());
    }
    if (!root.IsObject()) {
      throw std::runtime_error("tool catalog baseline root is not an object: " +
                               path.string());
    }
    ToolCatalogBaseline result;
    result.expected_catalog_total = require_int(root, "catalog_total");
    result.expected_non_empty = require_int(root, "schema_non_empty");
    result.expected_empty = require_int(root, "schema_empty");
    result.expected_alias_count = require_int(root, "alias_count");
    return result;
  }();
  return baseline;
}

} // namespace gda_test
