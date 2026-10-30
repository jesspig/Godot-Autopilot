#include "core/legacy_store.hpp"

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

using godot_autopilot::LegacyCopyAction;
using godot_autopilot::LegacyCopyOutcome;
using godot_autopilot::LegacyImportEntry;
using godot_autopilot::LegacyImportPlan;
using godot_autopilot::LegacyImportResult;
using godot_autopilot::LegacyStore;
using godot_autopilot::LegacyTarget;

namespace {

const char *kPrimaryJson = "{\"port\": 9600, \"show_time\": true}";
const char *kLegacyJson =
    "{\"port\": 9527, \"allow\": \"code_execute\", \"show_time\": false}";

std::filesystem::path test_root() {
  return std::filesystem::temp_directory_path() / "gda_legacy_compat_test";
}

void write_text_file(const std::filesystem::path &path,
                     const std::string &content) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
  out.close();
}

std::string read_text_file(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

std::string to_std(const std::filesystem::path &path) {
  return path.u8string();
}

} // namespace

TEST(LegacyFallbackTest, NewPathValueSuppressesLegacyFallback) {
  EXPECT_FALSE(LegacyStore::should_fallback_to_legacy(true, true));
  EXPECT_FALSE(LegacyStore::should_fallback_to_legacy(true, false));

  mcp::JsonValue value;
  bool from_legacy = true;
  std::string error;
  ASSERT_TRUE(LegacyStore::select_config_value(kPrimaryJson, kLegacyJson,
                                               "port", value, from_legacy,
                                               error));
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(from_legacy);
  ASSERT_TRUE(value.IsInt());
  EXPECT_EQ(value.GetInt(), 9600);
}

TEST(LegacyFallbackTest, MissingNewPathValueFallsBackToLegacyValue) {
  EXPECT_TRUE(LegacyStore::should_fallback_to_legacy(false, true));
  EXPECT_FALSE(LegacyStore::should_fallback_to_legacy(false, false));

  mcp::JsonValue value;
  bool from_legacy = false;
  std::string error;
  ASSERT_TRUE(LegacyStore::select_config_value("{\"show_time\": true}",
                                               kLegacyJson, "port", value,
                                               from_legacy, error));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(from_legacy);
  ASSERT_TRUE(value.IsInt());
  EXPECT_EQ(value.GetInt(), 9527);
}

TEST(LegacyFallbackTest, LegacyFallbackAppliesToEveryConfigKey) {
  mcp::JsonValue value;
  bool from_legacy = false;
  std::string error;
  ASSERT_TRUE(LegacyStore::select_config_value("", kLegacyJson, "allow",
                                               value, from_legacy, error));
  EXPECT_TRUE(from_legacy);
  ASSERT_TRUE(value.IsString());
  EXPECT_EQ(value.GetString(), "code_execute");

  from_legacy = false;
  ASSERT_TRUE(LegacyStore::select_config_value("", kLegacyJson, "show_time",
                                               value, from_legacy, error));
  EXPECT_TRUE(from_legacy);
  ASSERT_TRUE(value.IsBool());
  EXPECT_FALSE(value.GetBool());
}

TEST(LegacyFallbackTest, ValueMissingInBothPathsIsNotFound) {
  mcp::JsonValue value;
  bool from_legacy = true;
  std::string error;
  EXPECT_FALSE(LegacyStore::select_config_value(kPrimaryJson, kLegacyJson,
                                                "desensitize", value,
                                                from_legacy, error));
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(from_legacy);
}

TEST(LegacyFallbackTest, UnparsablePrimaryFallsBackAndReportsError) {
  mcp::JsonValue value;
  bool from_legacy = false;
  std::string error;
  ASSERT_TRUE(LegacyStore::select_config_value("{not json", kLegacyJson,
                                               "port", value, from_legacy,
                                               error));
  EXPECT_FALSE(error.empty());
  EXPECT_TRUE(from_legacy);
  ASSERT_TRUE(value.IsInt());
  EXPECT_EQ(value.GetInt(), 9527);
}

TEST(LegacyFallbackTest, UnparsableBothPathsReportErrorAndNoValue) {
  mcp::JsonValue value;
  bool from_legacy = true;
  std::string error;
  EXPECT_FALSE(LegacyStore::select_config_value("{not json", "[1,2]",
                                                "port", value, from_legacy,
                                                error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(from_legacy);
}

TEST(LegacyLayoutTest, ClassifiesLegacyRelativePaths) {
  EXPECT_EQ(LegacyStore::classify_relative_path("config.json"),
            LegacyTarget::kGlobalConfig);
  EXPECT_EQ(LegacyStore::classify_relative_path("logs/gda-1.log"),
            LegacyTarget::kProjectLogs);
  EXPECT_EQ(LegacyStore::classify_relative_path("traces/trace-1.jsonl"),
            LegacyTarget::kProjectTraces);
  EXPECT_EQ(LegacyStore::classify_relative_path("captures/shot.png"),
            LegacyTarget::kProjectCaptures);
  EXPECT_EQ(LegacyStore::classify_relative_path("captures/nested/shot.png"),
            LegacyTarget::kProjectCaptures);
  EXPECT_EQ(LegacyStore::classify_relative_path("stray.txt"),
            LegacyTarget::kUnsupported);
  EXPECT_EQ(LegacyStore::classify_relative_path("logs"),
            LegacyTarget::kUnsupported);
  EXPECT_EQ(LegacyStore::classify_relative_path("logs/"),
            LegacyTarget::kUnsupported);
}

TEST(LegacyLayoutTest, DestinationPathsFollowNewLayout) {
  EXPECT_EQ(LegacyStore::destination_absolute_path(
                LegacyTarget::kGlobalConfig, "config.json", "/root",
                "/root/projects/demo"),
            "/root/config.json");
  EXPECT_EQ(LegacyStore::destination_absolute_path(
                LegacyTarget::kProjectLogs, "logs/gda-1.log", "/root",
                "/root/projects/demo"),
            "/root/projects/demo/logs/gda-1.log");
  EXPECT_EQ(LegacyStore::destination_absolute_path(
                LegacyTarget::kUnsupported, "stray.txt", "/root",
                "/root/projects/demo"),
            "");
  EXPECT_EQ(LegacyStore::destination_absolute_path(
                LegacyTarget::kGlobalConfig, "config.json", "", "/root/projects"),
            "");
}

TEST(LegacyImportTest, PlanSkipsExistingAndUnsupportedEntries) {
  std::vector<LegacyImportEntry> entries;
  LegacyImportEntry fresh;
  fresh.relative_path = "logs/gda-1.log";
  fresh.destination_exists = false;
  entries.push_back(fresh);
  LegacyImportEntry existing;
  existing.relative_path = "traces/trace-1.jsonl";
  existing.destination_exists = true;
  entries.push_back(existing);
  LegacyImportEntry stray;
  stray.relative_path = "stray.txt";
  stray.destination_exists = false;
  entries.push_back(stray);

  const LegacyImportPlan plan = LegacyStore::plan_import(entries);
  ASSERT_EQ(plan.to_copy.size(), 1u);
  EXPECT_EQ(plan.to_copy[0], "logs/gda-1.log");
  ASSERT_EQ(plan.skipped_existing.size(), 1u);
  EXPECT_EQ(plan.skipped_existing[0], "traces/trace-1.jsonl");
  ASSERT_EQ(plan.skipped_unsupported.size(), 1u);
  EXPECT_EQ(plan.skipped_unsupported[0], "stray.txt");

  EXPECT_EQ(LegacyStore::decide_copy_action(false), LegacyCopyAction::kCopy);
  EXPECT_EQ(LegacyStore::decide_copy_action(true),
            LegacyCopyAction::kSkipExisting);
}

TEST(LegacyImportTest, CopyKeepsSourceAndWritesDestination) {
  const std::filesystem::path base = test_root() / "copy";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  const std::filesystem::path source = base / "legacy" / "logs" / "gda-1.log";
  const std::filesystem::path destination = base / "new" / "logs" / "gda-1.log";
  write_text_file(source, "legacy payload");

  const LegacyCopyOutcome outcome = LegacyStore::copy_file_preserving_source(
      to_std(source), to_std(destination), false);
  EXPECT_TRUE(outcome.copied);
  EXPECT_FALSE(outcome.skipped);
  EXPECT_TRUE(outcome.error.empty());
  EXPECT_TRUE(std::filesystem::exists(source));
  EXPECT_EQ(read_text_file(source), "legacy payload");
  EXPECT_EQ(read_text_file(destination), "legacy payload");

  std::filesystem::remove_all(base, ec);
}

TEST(LegacyImportTest, CopySkipsExistingDestinationWithoutTouchingIt) {
  const std::filesystem::path base = test_root() / "copy_skip";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  const std::filesystem::path source = base / "legacy" / "config.json";
  const std::filesystem::path destination = base / "new" / "config.json";
  write_text_file(source, "legacy payload");
  write_text_file(destination, "new payload");

  const LegacyCopyOutcome outcome = LegacyStore::copy_file_preserving_source(
      to_std(source), to_std(destination), true);
  EXPECT_FALSE(outcome.copied);
  EXPECT_TRUE(outcome.skipped);
  EXPECT_TRUE(outcome.error.empty());
  EXPECT_EQ(read_text_file(destination), "new payload");
  EXPECT_EQ(read_text_file(source), "legacy payload");

  std::filesystem::remove_all(base, ec);
}

TEST(LegacyImportTest, ImportCopiesLayoutAndKeepsLegacyFiles) {
  const std::filesystem::path base = test_root() / "import";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  const std::filesystem::path legacy = base / "legacy";
  const std::filesystem::path root = base / "root";
  const std::filesystem::path project = root / "projects" / "demo-1";
  write_text_file(legacy / "config.json", "{\"port\": 9527}");
  write_text_file(legacy / "logs" / "gda-1.log", "log payload");
  write_text_file(legacy / "traces" / "trace-1.jsonl", "trace payload");
  write_text_file(legacy / "captures" / "shot.png", "capture payload");
  write_text_file(legacy / "stray.txt", "stray payload");

  const LegacyImportResult result = LegacyStore::import_legacy_data(
      to_std(legacy), to_std(root), to_std(project));
  EXPECT_FALSE(result.legacy_dir_missing);
  EXPECT_EQ(result.copied, 4);
  EXPECT_EQ(result.skipped_existing, 0);
  EXPECT_EQ(result.skipped_unsupported, 1);
  EXPECT_EQ(result.failed, 0);
  EXPECT_TRUE(result.failures.empty());

  EXPECT_EQ(read_text_file(root / "config.json"), "{\"port\": 9527}");
  EXPECT_EQ(read_text_file(project / "logs" / "gda-1.log"), "log payload");
  EXPECT_EQ(read_text_file(project / "traces" / "trace-1.jsonl"),
            "trace payload");
  EXPECT_EQ(read_text_file(project / "captures" / "shot.png"),
            "capture payload");
  EXPECT_FALSE(std::filesystem::exists(project / "stray.txt"));

  EXPECT_EQ(read_text_file(legacy / "config.json"), "{\"port\": 9527}");
  EXPECT_EQ(read_text_file(legacy / "logs" / "gda-1.log"), "log payload");
  EXPECT_EQ(read_text_file(legacy / "traces" / "trace-1.jsonl"),
            "trace payload");
  EXPECT_EQ(read_text_file(legacy / "captures" / "shot.png"),
            "capture payload");
  EXPECT_EQ(read_text_file(legacy / "stray.txt"), "stray payload");

  std::filesystem::remove_all(base, ec);
}

TEST(LegacyImportTest, SecondImportSkipsFilesAlreadyPresent) {
  const std::filesystem::path base = test_root() / "import_twice";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  const std::filesystem::path legacy = base / "legacy";
  const std::filesystem::path root = base / "root";
  const std::filesystem::path project = root / "projects" / "demo-1";
  write_text_file(legacy / "config.json", "{\"port\": 9527}");
  write_text_file(legacy / "logs" / "gda-1.log", "log payload");

  const LegacyImportResult first = LegacyStore::import_legacy_data(
      to_std(legacy), to_std(root), to_std(project));
  EXPECT_EQ(first.copied, 2);
  EXPECT_EQ(first.skipped_existing, 0);

  const LegacyImportResult second = LegacyStore::import_legacy_data(
      to_std(legacy), to_std(root), to_std(project));
  EXPECT_EQ(second.copied, 0);
  EXPECT_EQ(second.skipped_existing, 2);
  EXPECT_EQ(second.failed, 0);
  EXPECT_EQ(read_text_file(project / "logs" / "gda-1.log"), "log payload");

  std::filesystem::remove_all(base, ec);
}

TEST(LegacyImportTest, ImportWithMissingLegacyDirectoryReturnsSafely) {
  const std::filesystem::path base = test_root() / "import_missing";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  const std::filesystem::path legacy = base / "absent";
  const std::filesystem::path root = base / "root";
  const std::filesystem::path project = root / "projects" / "demo-1";

  const LegacyImportResult result = LegacyStore::import_legacy_data(
      to_std(legacy), to_std(root), to_std(project));
  EXPECT_TRUE(result.legacy_dir_missing);
  EXPECT_EQ(result.copied, 0);
  EXPECT_EQ(result.skipped_existing, 0);
  EXPECT_EQ(result.skipped_unsupported, 0);
  EXPECT_EQ(result.failed, 0);
  EXPECT_FALSE(std::filesystem::exists(root / "config.json"));

  const LegacyImportResult empty_paths = LegacyStore::import_legacy_data(
      std::string(), to_std(root), to_std(project));
  EXPECT_TRUE(empty_paths.legacy_dir_missing);
  EXPECT_EQ(empty_paths.copied, 0);

  std::filesystem::remove_all(base, ec);
}

TEST(LegacyImportTest, SummaryReportsCopiedSkippedAndFailedCounts) {
  LegacyImportResult result;
  result.copied = 2;
  result.skipped_existing = 1;
  result.skipped_unsupported = 1;
  result.failed = 0;
  result.skipped_existing_paths.push_back("config.json");
  result.skipped_unsupported_paths.push_back("stray.txt");
  const std::string summary = LegacyStore::format_import_summary(result);
  EXPECT_NE(summary.find("Imported 2"), std::string::npos);
  EXPECT_NE(summary.find("skipped 2"), std::string::npos);
  EXPECT_NE(summary.find("failed 0"), std::string::npos);
  EXPECT_NE(summary.find("config.json"), std::string::npos);
}
