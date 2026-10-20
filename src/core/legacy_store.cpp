#include "core/legacy_store.hpp"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <mcp/McpError.hpp>

#include "core/path_util.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kLegacyDirPath = "user://godot_autopilot";
constexpr const char *kLegacyConfigFileName = "config.json";
constexpr const char *kLegacyLogsDirName = "logs";
constexpr const char *kLegacyTracesDirName = "traces";
constexpr const char *kLegacyCapturesDirName = "captures";
constexpr std::size_t kLegacySummaryNameLimit = 8;

std::string legacy_join_names(const std::vector<std::string> &names) {
  std::string out;
  std::size_t shown = 0;
  for (const std::string &name : names) {
    if (shown >= kLegacySummaryNameLimit) {
      if (!out.empty()) {
        out += ", ";
      }
      out += "...";
      break;
    }
    if (!out.empty()) {
      out += ", ";
    }
    out += name;
    ++shown;
  }
  return out;
}

void legacy_collect_relative_files(const std::filesystem::path &base,
                                   const std::string &prefix,
                                   std::vector<std::string> &out) {
  std::error_code ec;
  std::filesystem::directory_iterator iterator(base, ec);
  if (ec) {
    return;
  }
  for (const std::filesystem::directory_entry &entry : iterator) {
    std::error_code entry_ec;
    const std::string name = entry.path().filename().u8string();
    if (name.empty() || name == "." || name == "..") {
      continue;
    }
    const std::string relative = prefix.empty() ? name : prefix + "/" + name;
    if (entry.is_directory(entry_ec) && !entry_ec) {
      legacy_collect_relative_files(entry.path(), relative, out);
      continue;
    }
    out.push_back(relative);
  }
}

bool legacy_path_exists(const std::string &path) {
  if (path.empty()) {
    return false;
  }
  std::error_code ec;
  const bool exists = std::filesystem::exists(to_fs_path(path), ec);
  return exists && !ec;
}

} // namespace

std::string LegacyStore::legacy_dir_path() { return kLegacyDirPath; }

std::string LegacyStore::legacy_config_path() {
  return join_path(kLegacyDirPath, kLegacyConfigFileName);
}

bool LegacyStore::should_fallback_to_legacy(bool primary_present,
                                            bool legacy_present) {
  return !primary_present && legacy_present;
}

bool LegacyStore::select_config_value(const std::string &primary_json,
                                      const std::string &legacy_json,
                                      const char *key, mcp::JsonValue &out,
                                      bool &from_legacy, std::string &error) {
  error.clear();
  from_legacy = false;
  if (!primary_json.empty()) {
    mcp::JsonValue::Object primary_object;
    bool primary_parsed = false;
    try {
      mcp::JsonValue parsed = mcp::JsonValue::Parse(primary_json);
      if (parsed.IsObject()) {
        primary_object = std::move(parsed.GetObject());
        primary_parsed = true;
      } else {
        error = "primary config root is not a JSON object";
      }
    } catch (const mcp::McpError &e) {
      error = std::string("primary config parse failed: ") +
              std::string(e.ErrorMessage());
    } catch (const std::exception &e) {
      error = std::string("primary config parse failed: ") + e.what();
    } catch (...) {
      error = "primary config parse failed: unknown exception";
    }
    if (primary_parsed) {
      const mcp::JsonValue document(std::move(primary_object));
      const mcp::JsonValue *found = document.Find(key);
      if (found != nullptr) {
        out = *found;
        return true;
      }
    }
  }
  if (legacy_json.empty()) {
    return false;
  }
  try {
    mcp::JsonValue parsed = mcp::JsonValue::Parse(legacy_json);
    if (!parsed.IsObject()) {
      if (!error.empty()) {
        error += "; ";
      }
      error += "legacy config root is not a JSON object";
      return false;
    }
    const mcp::JsonValue document(std::move(parsed.GetObject()));
    const mcp::JsonValue *found = document.Find(key);
    if (found == nullptr) {
      return false;
    }
    out = *found;
    from_legacy = true;
    return true;
  } catch (const mcp::McpError &e) {
    if (!error.empty()) {
      error += "; ";
    }
    error += std::string("legacy config parse failed: ") +
            std::string(e.ErrorMessage());
  } catch (const std::exception &e) {
    if (!error.empty()) {
      error += "; ";
    }
    error += std::string("legacy config parse failed: ") + e.what();
  } catch (...) {
    if (!error.empty()) {
      error += "; ";
    }
    error += "legacy config parse failed: unknown exception";
  }
  return false;
}

LegacyTarget LegacyStore::classify_relative_path(
    const std::string &relative_path) {
  if (relative_path == kLegacyConfigFileName) {
    return LegacyTarget::kGlobalConfig;
  }
  const std::size_t separator = relative_path.find('/');
  if (separator == std::string::npos) {
    return LegacyTarget::kUnsupported;
  }
  const std::string head = relative_path.substr(0, separator);
  const std::string tail = relative_path.substr(separator + 1);
  if (tail.empty()) {
    return LegacyTarget::kUnsupported;
  }
  if (head == kLegacyLogsDirName) {
    return LegacyTarget::kProjectLogs;
  }
  if (head == kLegacyTracesDirName) {
    return LegacyTarget::kProjectTraces;
  }
  if (head == kLegacyCapturesDirName) {
    return LegacyTarget::kProjectCaptures;
  }
  return LegacyTarget::kUnsupported;
}

std::string LegacyStore::destination_relative_path(
    LegacyTarget target, const std::string &relative_path) {
  switch (target) {
  case LegacyTarget::kGlobalConfig:
    return kLegacyConfigFileName;
  case LegacyTarget::kProjectLogs:
  case LegacyTarget::kProjectTraces:
  case LegacyTarget::kProjectCaptures:
    return relative_path;
  case LegacyTarget::kUnsupported:
    break;
  }
  return std::string();
}

std::string LegacyStore::destination_absolute_path(
    LegacyTarget target, const std::string &relative_path,
    const std::string &global_config_dir, const std::string &project_dir) {
  const std::string relative = destination_relative_path(target, relative_path);
  if (relative.empty()) {
    return std::string();
  }
  if (target == LegacyTarget::kGlobalConfig) {
    if (global_config_dir.empty()) {
      return std::string();
    }
    return join_path(global_config_dir, relative);
  }
  if (project_dir.empty()) {
    return std::string();
  }
  return join_path(project_dir, relative);
}

LegacyCopyAction LegacyStore::decide_copy_action(bool destination_exists) {
  return destination_exists ? LegacyCopyAction::kSkipExisting
                            : LegacyCopyAction::kCopy;
}

LegacyCopyOutcome LegacyStore::copy_file_preserving_source(
    const std::string &source_path, const std::string &destination_path,
    bool destination_exists) {
  LegacyCopyOutcome outcome;
  if (decide_copy_action(destination_exists) == LegacyCopyAction::kSkipExisting) {
    outcome.skipped = true;
    return outcome;
  }
  if (source_path.empty() || destination_path.empty()) {
    outcome.error = "source or destination path is empty";
    return outcome;
  }
  std::error_code ec;
  const std::filesystem::path source = to_fs_path(source_path);
  if (!std::filesystem::is_regular_file(source, ec) || ec) {
    outcome.error = "source is not a regular file";
    return outcome;
  }
  const std::filesystem::path destination = to_fs_path(destination_path);
  const std::filesystem::path parent = destination.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      outcome.error = "destination directory creation failed";
      return outcome;
    }
  }
  std::filesystem::copy_file(source, destination,
                             std::filesystem::copy_options::none, ec);
  if (ec) {
    outcome.error = ec.message();
    return outcome;
  }
  outcome.copied = true;
  return outcome;
}

LegacyImportPlan LegacyStore::plan_import(
    const std::vector<LegacyImportEntry> &entries) {
  LegacyImportPlan plan;
  for (const LegacyImportEntry &entry : entries) {
    if (classify_relative_path(entry.relative_path) ==
        LegacyTarget::kUnsupported) {
      plan.skipped_unsupported.push_back(entry.relative_path);
      continue;
    }
    if (decide_copy_action(entry.destination_exists) ==
        LegacyCopyAction::kSkipExisting) {
      plan.skipped_existing.push_back(entry.relative_path);
      continue;
    }
    plan.to_copy.push_back(entry.relative_path);
  }
  return plan;
}

LegacyImportResult LegacyStore::import_legacy_data(
    const std::string &legacy_dir, const std::string &global_config_dir,
    const std::string &project_dir) {
  LegacyImportResult result;
  if (legacy_dir.empty() || global_config_dir.empty() || project_dir.empty()) {
    result.legacy_dir_missing = true;
    return result;
  }
  std::error_code ec;
  const std::filesystem::path legacy_root = to_fs_path(legacy_dir);
  if (!std::filesystem::is_directory(legacy_root, ec) || ec) {
    result.legacy_dir_missing = true;
    return result;
  }

  std::vector<std::string> relative_paths;
  legacy_collect_relative_files(legacy_root, std::string(), relative_paths);

  std::vector<LegacyImportEntry> entries;
  entries.reserve(relative_paths.size());
  for (const std::string &relative : relative_paths) {
    LegacyImportEntry entry;
    entry.relative_path = relative;
    entry.destination_exists = legacy_path_exists(destination_absolute_path(
        classify_relative_path(relative), relative, global_config_dir,
        project_dir));
    entries.push_back(entry);
  }

  const LegacyImportPlan plan = plan_import(entries);
  result.skipped_existing = static_cast<int>(plan.skipped_existing.size());
  result.skipped_unsupported = static_cast<int>(plan.skipped_unsupported.size());
  result.skipped_existing_paths = plan.skipped_existing;
  result.skipped_unsupported_paths = plan.skipped_unsupported;

  for (const std::string &relative : plan.to_copy) {
    const std::string source = join_path(legacy_dir, relative);
    const std::string destination = destination_absolute_path(
        classify_relative_path(relative), relative, global_config_dir,
        project_dir);
    const LegacyCopyOutcome outcome = copy_file_preserving_source(
        source, destination, legacy_path_exists(destination));
    if (outcome.copied) {
      ++result.copied;
      continue;
    }
    if (outcome.skipped) {
      ++result.skipped_existing;
      result.skipped_existing_paths.push_back(relative);
      continue;
    }
    ++result.failed;
    result.failures.push_back(relative + ": " + outcome.error);
  }
  return result;
}

std::string LegacyStore::format_import_summary(
    const LegacyImportResult &result) {
  const int skipped = result.skipped_existing + result.skipped_unsupported;
  std::string summary = "Imported " + std::to_string(result.copied) +
                        " file(s), skipped " + std::to_string(skipped) +
                        ", failed " + std::to_string(result.failed) + ".";
  if (!result.skipped_existing_paths.empty()) {
    summary += " Skipped (already present): " +
               legacy_join_names(result.skipped_existing_paths);
  }
  if (!result.skipped_unsupported_paths.empty()) {
    summary += " Skipped (unsupported layout): " +
               legacy_join_names(result.skipped_unsupported_paths);
  }
  if (!result.failures.empty()) {
    summary += " Failures: " + legacy_join_names(result.failures);
  }
  return summary;
}

} // namespace godot_autopilot
