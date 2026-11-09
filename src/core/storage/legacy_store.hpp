#ifndef GODOT_AUTOPILOT_LEGACY_STORE_HPP
#define GODOT_AUTOPILOT_LEGACY_STORE_HPP

#include <mcp/JsonValue.hpp>

#include <string>
#include <vector>

namespace godot_autopilot {

enum class LegacyTarget {
  kUnsupported,
  kGlobalConfig,
  kProjectLogs,
  kProjectTraces,
  kProjectCaptures,
};

enum class LegacyCopyAction {
  kCopy,
  kSkipExisting,
};

struct LegacyImportEntry {
  std::string relative_path;
  bool destination_exists{false};
};

struct LegacyImportPlan {
  std::vector<std::string> to_copy;
  std::vector<std::string> skipped_existing;
  std::vector<std::string> skipped_unsupported;
};

struct LegacyCopyOutcome {
  bool copied{false};
  bool skipped{false};
  std::string error;
};

struct LegacyImportResult {
  int copied{0};
  int skipped_existing{0};
  int skipped_unsupported{0};
  int failed{0};
  std::vector<std::string> skipped_existing_paths;
  std::vector<std::string> skipped_unsupported_paths;
  std::vector<std::string> failures;
  bool legacy_dir_missing{false};
};

class LegacyStore {
public:
  static std::string legacy_dir_path();
  static std::string legacy_config_path();

  static bool should_fallback_to_legacy(bool primary_present,
                                        bool legacy_present);

  static bool select_config_value(const std::string &primary_json,
                                  const std::string &legacy_json,
                                  const char *key, mcp::JsonValue &out,
                                  bool &from_legacy, std::string &error);

  static LegacyTarget classify_relative_path(const std::string &relative_path);

  static std::string destination_relative_path(LegacyTarget target,
                                               const std::string &relative_path);

  static std::string destination_absolute_path(
      LegacyTarget target, const std::string &relative_path,
      const std::string &global_config_dir, const std::string &project_dir);

  static LegacyCopyAction decide_copy_action(bool destination_exists);

  static LegacyCopyOutcome copy_file_preserving_source(
      const std::string &source_path, const std::string &destination_path,
      bool destination_exists);

  static LegacyImportPlan plan_import(
      const std::vector<LegacyImportEntry> &entries);

  static LegacyImportResult import_legacy_data(
      const std::string &legacy_dir, const std::string &global_config_dir,
      const std::string &project_dir);

  static std::string format_import_summary(const LegacyImportResult &result);
};

} // namespace godot_autopilot

#endif
