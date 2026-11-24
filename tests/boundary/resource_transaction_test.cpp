#include "tools/domains/os/os_tools.hpp"
#include "tools/domains/resource/resource_tools.hpp"
#include "util/resource_fs.hpp"

#include <gtest/gtest.h>
#include <mcp/detail/sha256.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;
using godot_autopilot::resource_fs::CopyOverwriteFailure;
using godot_autopilot::resource_fs::FileTransactionFs;
using godot_autopilot::resource_fs::FileTransactionJournal;
using godot_autopilot::resource_fs::FileTransactionJournalKind;
using godot_autopilot::resource_fs::FileTransactionKind;
using godot_autopilot::resource_fs::FileTransactionOp;
using godot_autopilot::resource_fs::FileTransactionPlan;
using godot_autopilot::resource_fs::FileTransactionResult;
using godot_autopilot::resource_fs::FileTransactionSnapshot;
using godot_autopilot::resource_fs::check_copy_overwrite;
using godot_autopilot::resource_fs::commit_file_transaction;

class TempTree {
  public:
  TempTree() {
    static std::atomic<uint64_t> serial{0};
    std::error_code ec;
    fs::path base = fs::current_path(ec);
    if (ec) {
      base = ".";
    }
    const uint64_t stamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    root_ = base / ("gda_resource_transaction_" + std::to_string(stamp) + "_" +
                    std::to_string(serial.fetch_add(1)));
    std::error_code create_ec;
    fs::create_directories(root_, create_ec);
    if (!create_ec) {
      valid_ = true;
      error_.clear();
    } else {
      std::error_code existing_ec;
      valid_ = fs::is_directory(root_, existing_ec) && !existing_ec;
      error_ = valid_ ? std::string() : create_ec.message();
    }
  }

  ~TempTree() {
    if (!root_.empty()) {
      std::error_code ec;
      fs::remove_all(root_, ec);
    }
  }

  bool valid() const { return valid_; }
  const fs::path &root() const { return root_; }
  const std::string &error() const { return error_; }
  std::string path(const std::string &name) const {
    return (root_ / name).string();
  }

  private:
  fs::path root_;
  bool valid_ = false;
  std::string error_;
};

void write_bytes(const std::string &path, const std::string &content) {
  std::ofstream output(fs::u8path(path), std::ios::binary | std::ios::trunc);
  output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string read_bytes(const std::string &path) {
  std::ifstream input(fs::u8path(path), std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

std::string hash_file(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  mcp::detail::Sha256 sha;
  std::array<char, 8192> buffer{};
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    sha.Update(std::string_view(buffer.data(),
                                static_cast<size_t>(input.gcount())));
  }
  static const char digits[] = "0123456789abcdef";
  const auto digest = sha.Final();
  std::string out;
  out.reserve(64);
  for (uint8_t byte : digest) {
    out.push_back(digits[byte >> 4]);
    out.push_back(digits[byte & 0x0F]);
  }
  return out;
}

class StdFileTransactionFs : public FileTransactionFs {
  public:
  bool inspect(const std::string &path, FileTransactionSnapshot &snapshot,
               std::string &error) override {
    snapshot = FileTransactionSnapshot();
    error.clear();
    const fs::path target = fs::u8path(path);
    std::error_code ec;
    const fs::file_status status = fs::status(target, ec);
    if (ec == std::errc::no_such_file_or_directory) {
      return true;
    }
    if (ec) {
      error = "failed to inspect " + path + ": " + ec.message();
      return false;
    }
    snapshot.exists = fs::exists(status);
    if (!snapshot.exists) {
      return true;
    }
    snapshot.is_directory = fs::is_directory(status);
    if (snapshot.is_directory) {
      return true;
    }
    snapshot.size = fs::file_size(target, ec);
    if (ec) {
      error = "failed to read size for " + path + ": " + ec.message();
      return false;
    }
    const auto modified = fs::last_write_time(target, ec);
    if (ec) {
      error = "failed to read modification time for " + path + ": " +
              ec.message();
      return false;
    }
    snapshot.modified_time = modified.time_since_epoch().count();
    snapshot.sha256 = hash_file(target);
    return true;
  }

  bool stage_copy(const std::string &source, const std::string &staged,
                  std::string &error) override {
    std::error_code ec;
    fs::copy_file(fs::u8path(source), fs::u8path(staged),
                  fs::copy_options::none, ec);
    if (ec) {
      error = "failed to stage copy: " + ec.message();
      return false;
    }
    return true;
  }

  bool stage_write(const std::string &staged, const std::string &content,
                   std::string &error) override {
    std::error_code ec;
    {
      std::ofstream output(fs::u8path(staged),
                           std::ios::binary | std::ios::trunc);
      output.write(content.data(), static_cast<std::streamsize>(content.size()));
      if (!output) {
        error = "failed to stage write";
        return false;
      }
    }
    const fs::path target = fs::u8path(staged);
    if (fs::file_size(target, ec) != content.size() || ec ||
        hash_file(target) != godot_autopilot::resource_fs::sha256_hex(content)) {
      error = "staged write readback mismatch";
      return false;
    }
    return true;
  }

  bool rename_unique(const std::string &source,
                     const std::string &destination,
                     std::string &error) override {
    std::error_code ec;
    if (fs::exists(fs::u8path(destination), ec) || ec) {
      error = "backup destination already exists";
      return false;
    }
    fs::rename(fs::u8path(source), fs::u8path(destination), ec);
    if (ec) {
      error = "failed to rename backup: " + ec.message();
      return false;
    }
    return true;
  }

  bool commit_staged(const std::string &staged,
                     const std::string &destination,
                     std::string &error) override {
    std::error_code ec;
    fs::create_hard_link(fs::u8path(staged), fs::u8path(destination), ec);
    if (ec) {
      error = "failed to commit without overwrite: " + ec.message();
      return false;
    }
    fs::remove(fs::u8path(staged), ec);
    return true;
  }

  bool remove_file(const std::string &path, std::string &error) override {
    std::error_code ec;
    if (!fs::remove(fs::u8path(path), ec) && !ec) {
      error = "transaction file does not exist: " + path;
      return false;
    }
    if (ec) {
      error = "failed to remove transaction file: " + ec.message();
      return false;
    }
    return true;
  }
};

class InjectedFileTransactionFs : public StdFileTransactionFs {
  public:
  bool fail_stage_copy = false;
  bool fail_stage_write = false;
  bool corrupt_staged_copy = false;
  bool corrupt_staged_write = false;
  bool mutate_source_on_stage = false;
  bool mutate_target_after_stage = false;
  int fail_commit_at = 0;
  int fail_source_removals = 0;
  std::string source_to_mutate;
  std::string target_to_mutate;
  std::string target_race_content;

  protected:
  int commit_calls_ = 0;

  bool stage_copy(const std::string &source, const std::string &staged,
                  std::string &error) override {
    if (fail_stage_copy) {
      error = "injected copy staging failure";
      return false;
    }
    if (!StdFileTransactionFs::stage_copy(source, staged, error)) {
      return false;
    }
    if (mutate_source_on_stage && source == source_to_mutate) {
      write_bytes(source, "concurrent source content");
    }
    if (corrupt_staged_copy) {
      write_bytes(staged, "corrupt");
    }
    if (mutate_target_after_stage && !target_to_mutate.empty()) {
      write_bytes(target_to_mutate, target_race_content);
    }
    return true;
  }

  bool stage_write(const std::string &staged, const std::string &content,
                   std::string &error) override {
    if (fail_stage_write) {
      error = "injected write staging failure";
      return false;
    }
    if (!StdFileTransactionFs::stage_write(staged, content, error)) {
      return false;
    }
    if (corrupt_staged_write) {
      write_bytes(staged, "corrupt");
    }
    return true;
  }

  bool commit_staged(const std::string &staged,
                     const std::string &destination,
                     std::string &error) override {
    ++commit_calls_;
    if (fail_commit_at == commit_calls_) {
      error = "injected commit failure";
      return false;
    }
    return StdFileTransactionFs::commit_staged(staged, destination, error);
  }

  bool remove_file(const std::string &path, std::string &error) override {
    if (fail_source_removals > 0 && path.find(".gda-txn-") == std::string::npos) {
      --fail_source_removals;
      error = "injected source removal failure";
      return false;
    }
    return StdFileTransactionFs::remove_file(path, error);
  }
};

FileTransactionPlan copy_plan(const std::string &source,
                              const std::string &destination,
                              bool overwrite) {
  FileTransactionPlan plan;
  plan.operations.push_back({FileTransactionKind::kCopy, source, destination, {},
                             false, overwrite});
  return plan;
}

bool has_transaction_residue(const TempTree &tree) {
  for (const auto &entry : fs::recursive_directory_iterator(tree.root())) {
    if (entry.path().filename().string().find(".gda-txn-") !=
        std::string::npos) {
      return true;
    }
  }
  return false;
}

const mcp::JsonValue *schema_properties(const std::string &tool_name) {
  std::vector<std::unique_ptr<godot_autopilot::ToolBase>> tools;
  if (tool_name == "os_file_write") {
    tools = godot_autopilot::os_tools::make_tools();
  } else {
    tools = godot_autopilot::resource_tools::make_tools();
  }
  for (const auto &tool : tools) {
    if (tool->meta().name != tool_name) {
      continue;
    }
    const mcp::JsonValue schema = tool->input_schema();
    const mcp::JsonValue *properties = schema.Find("properties");
    if (properties != nullptr && properties->IsObject()) {
      return new mcp::JsonValue(*properties);
    }
  }
  return nullptr;
}

TEST(ResourceTransactionTest, CopyOverwriteRequiresBothFlags) {
  EXPECT_EQ(check_copy_overwrite(false, false, false),
            CopyOverwriteFailure::kNone);
  EXPECT_EQ(check_copy_overwrite(true, false, true),
            CopyOverwriteFailure::kTargetExists);
  EXPECT_EQ(check_copy_overwrite(true, true, false),
            CopyOverwriteFailure::kConfirmationRequired);
  EXPECT_EQ(check_copy_overwrite(true, true, true),
            CopyOverwriteFailure::kNone);
}

TEST(ResourceTransactionTest, CopyRejectsExistingTargetByDefault) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("source.bin");
  const std::string destination = tree.path("destination.bin");
  write_bytes(source, std::string("new\0content", 11));
  write_bytes(destination, "old");
  StdFileTransactionFs file_system;
  FileTransactionJournal journal;
  const FileTransactionResult result = commit_file_transaction(
      copy_plan(source, destination, false), journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.code, "target_conflict");
  EXPECT_EQ(read_bytes(destination), "old");
  EXPECT_TRUE(journal.entries.empty());
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, CopyHashVerifiesBeforeAtomicReplacement) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("source.bin");
  const std::string destination = tree.path("destination.bin");
  const std::string content("new\0binary\r\ncontent", 19);
  write_bytes(source, content);
  write_bytes(destination, "old");
  StdFileTransactionFs file_system;
  FileTransactionJournal journal;
  const FileTransactionResult result = commit_file_transaction(
      copy_plan(source, destination, true), journal, file_system);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_EQ(read_bytes(source), content);
  EXPECT_EQ(read_bytes(destination), content);
  EXPECT_EQ(hash_file(fs::u8path(destination)),
            godot_autopilot::resource_fs::sha256_hex(content));
  ASSERT_EQ(journal.entries.size(), 2u);
  EXPECT_EQ(journal.entries[0].kind,
            FileTransactionJournalKind::kTargetBackedUp);
  EXPECT_EQ(journal.entries[1].kind,
            FileTransactionJournalKind::kTargetCommitted);
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, CopyStageFailurePreservesDestination) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("source.bin");
  const std::string destination = tree.path("destination.bin");
  write_bytes(source, "new");
  write_bytes(destination, "old");
  InjectedFileTransactionFs file_system;
  file_system.fail_stage_copy = true;
  FileTransactionJournal journal;
  const FileTransactionResult result = commit_file_transaction(
      copy_plan(source, destination, true), journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.code, "write_failed");
  EXPECT_EQ(read_bytes(destination), "old");
  EXPECT_TRUE(journal.entries.empty());
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, CopyReadbackMismatchPreservesDestination) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("source.bin");
  const std::string destination = tree.path("destination.bin");
  write_bytes(source, "new-content");
  write_bytes(destination, "old");
  InjectedFileTransactionFs file_system;
  file_system.corrupt_staged_copy = true;
  FileTransactionJournal journal;
  const FileTransactionResult result = commit_file_transaction(
      copy_plan(source, destination, true), journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.code, "readback_mismatch");
  EXPECT_EQ(read_bytes(destination), "old");
  EXPECT_TRUE(journal.entries.empty());
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, CopyConcurrentSourceModificationAborts) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("source.bin");
  const std::string destination = tree.path("destination.bin");
  write_bytes(source, "original");
  InjectedFileTransactionFs file_system;
  file_system.mutate_source_on_stage = true;
  file_system.source_to_mutate = source;
  FileTransactionJournal journal;
  const FileTransactionResult result = commit_file_transaction(
      copy_plan(source, destination, false), journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.code, "source_modified");
  EXPECT_FALSE(fs::exists(fs::u8path(destination)));
  EXPECT_TRUE(journal.entries.empty());
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, CopyConcurrentTargetModificationAborts) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("source.bin");
  const std::string destination = tree.path("destination.bin");
  write_bytes(source, "original");
  write_bytes(destination, "old");
  InjectedFileTransactionFs file_system;
  file_system.mutate_target_after_stage = true;
  file_system.target_to_mutate = destination;
  file_system.target_race_content = "concurrent";
  FileTransactionJournal journal;
  const FileTransactionResult result = commit_file_transaction(
      copy_plan(source, destination, true), journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.code, "target_modified");
  EXPECT_EQ(read_bytes(destination), "concurrent");
  EXPECT_TRUE(journal.entries.empty());
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, MovePreflightsEverySidecarTarget) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("old.tres");
  const std::string source_uid = source + ".uid";
  const std::string source_import = source + ".import";
  const std::string destination = tree.path("new.tres");
  const std::string destination_uid = destination + ".uid";
  const std::string destination_import = destination + ".import";
  write_bytes(source, "resource");
  write_bytes(source_uid, "uid");
  write_bytes(source_import, "import");
  write_bytes(destination_import, "conflict");
  FileTransactionPlan plan;
  plan.operations = {
      {FileTransactionKind::kMove, source, destination, {}, false, false},
      {FileTransactionKind::kMove, source_uid, destination_uid, {}, false,
       false},
      {FileTransactionKind::kMove, source_import, destination_import, {}, false,
       false},
  };
  StdFileTransactionFs file_system;
  FileTransactionJournal journal;
  const FileTransactionResult result =
      commit_file_transaction(plan, journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.code, "target_conflict");
  EXPECT_EQ(read_bytes(source), "resource");
  EXPECT_EQ(read_bytes(source_uid), "uid");
  EXPECT_EQ(read_bytes(source_import), "import");
  EXPECT_EQ(read_bytes(destination_import), "conflict");
  EXPECT_TRUE(journal.entries.empty());
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, MissingSourceSidecarStillRejectsTargetSidecar) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("old.tres");
  const std::string destination = tree.path("new.tres");
  write_bytes(source, "resource");
  write_bytes(destination + ".uid", "conflict");
  FileTransactionPlan plan;
  plan.operations = {
      {FileTransactionKind::kMove, source, destination, {}, false, false},
      {FileTransactionKind::kRequireAbsent, {}, destination + ".uid", {}, false,
       false},
  };
  StdFileTransactionFs file_system;
  FileTransactionJournal journal;
  const FileTransactionResult result =
      commit_file_transaction(plan, journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.code, "target_conflict");
  EXPECT_EQ(read_bytes(source), "resource");
  EXPECT_EQ(read_bytes(destination + ".uid"), "conflict");
  EXPECT_TRUE(journal.entries.empty());
}

TEST(ResourceTransactionTest, MoveCommitsMainAndSidecarsTogether) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("old.tres");
  const std::string source_uid = source + ".uid";
  const std::string source_import = source + ".import";
  const std::string destination = tree.path("new.tres");
  write_bytes(source, "resource");
  write_bytes(source_uid, "uid");
  write_bytes(source_import, "import");
  FileTransactionPlan plan;
  plan.operations = {
      {FileTransactionKind::kMove, source, destination, {}, false, false},
      {FileTransactionKind::kMove, source_uid, destination + ".uid", {}, false,
       false},
      {FileTransactionKind::kMove, source_import, destination + ".import", {},
       false, false},
  };
  StdFileTransactionFs file_system;
  FileTransactionJournal journal;
  const FileTransactionResult result =
      commit_file_transaction(plan, journal, file_system);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_FALSE(fs::exists(fs::u8path(source)));
  EXPECT_FALSE(fs::exists(fs::u8path(source_uid)));
  EXPECT_FALSE(fs::exists(fs::u8path(source_import)));
  EXPECT_EQ(read_bytes(destination), "resource");
  EXPECT_EQ(read_bytes(destination + ".uid"), "uid");
  EXPECT_EQ(read_bytes(destination + ".import"), "import");
  EXPECT_EQ(journal.entries.size(), 6u);
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, MoveFailureCompensatesOnlyJournaledSteps) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("old.tres");
  const std::string source_uid = source + ".uid";
  const std::string source_import = source + ".import";
  const std::string destination = tree.path("new.tres");
  write_bytes(source, "resource");
  write_bytes(source_uid, "uid");
  write_bytes(source_import, "import");
  FileTransactionPlan plan;
  plan.operations = {
      {FileTransactionKind::kMove, source, destination, {}, false, false},
      {FileTransactionKind::kMove, source_uid, destination + ".uid", {}, false,
       false},
      {FileTransactionKind::kMove, source_import, destination + ".import", {},
       false, false},
  };
  InjectedFileTransactionFs file_system;
  file_system.fail_commit_at = 3;
  FileTransactionJournal journal;
  const FileTransactionResult result =
      commit_file_transaction(plan, journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.code, "write_failed");
  EXPECT_TRUE(result.compensated);
  EXPECT_TRUE(result.compensation_errors.empty());
  EXPECT_EQ(read_bytes(source), "resource");
  EXPECT_EQ(read_bytes(source_uid), "uid");
  EXPECT_EQ(read_bytes(source_import), "import");
  EXPECT_FALSE(fs::exists(fs::u8path(destination)));
  EXPECT_FALSE(fs::exists(fs::u8path(destination + ".uid")));
  EXPECT_FALSE(fs::exists(fs::u8path(destination + ".import")));
  EXPECT_FALSE(journal.entries.empty());
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, SourceRemovalFailureRestoresCommittedTarget) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string source = tree.path("old.tres");
  const std::string destination = tree.path("new.tres");
  write_bytes(source, "resource");
  FileTransactionPlan plan;
  plan.operations.push_back(
      {FileTransactionKind::kMove, source, destination, {}, false, false});
  InjectedFileTransactionFs file_system;
  file_system.fail_source_removals = 1;
  FileTransactionJournal journal;
  const FileTransactionResult result =
      commit_file_transaction(plan, journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(result.compensated);
  EXPECT_TRUE(result.compensation_errors.empty());
  EXPECT_EQ(read_bytes(source), "resource");
  EXPECT_FALSE(fs::exists(fs::u8path(destination)));
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, AllDependenciesStageBeforeAnyCommit) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string first = tree.path("first.tscn");
  const std::string second = tree.path("second.tscn");
  write_bytes(first, "old-first");
  write_bytes(second, "old-second");
  FileTransactionPlan plan;
  plan.operations = {
      {FileTransactionKind::kWrite, {}, first, "new-first", true, true},
      {FileTransactionKind::kWrite, {}, second, "new-second", true, true},
  };
  InjectedFileTransactionFs file_system;
  file_system.fail_stage_write = true;
  FileTransactionJournal journal;
  const FileTransactionResult result =
      commit_file_transaction(plan, journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(read_bytes(first), "old-first");
  EXPECT_EQ(read_bytes(second), "old-second");
  EXPECT_TRUE(journal.entries.empty());
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, CommitFailureRestoresCommittedDependency) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string dependency = tree.path("dependent.tscn");
  const std::string source = tree.path("old.tres");
  const std::string destination = tree.path("new.tres");
  write_bytes(dependency, "old-dependency");
  write_bytes(source, "resource");
  FileTransactionPlan plan;
  plan.operations = {
      {FileTransactionKind::kWrite, {}, dependency, "new-dependency", true, true},
      {FileTransactionKind::kMove, source, destination, {}, false, false},
  };
  InjectedFileTransactionFs file_system;
  file_system.fail_commit_at = 2;
  FileTransactionJournal journal;
  const FileTransactionResult result =
      commit_file_transaction(plan, journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(result.compensated);
  EXPECT_TRUE(result.compensation_errors.empty());
  EXPECT_EQ(read_bytes(dependency), "old-dependency");
  EXPECT_EQ(read_bytes(source), "resource");
  EXPECT_FALSE(fs::exists(fs::u8path(destination)));
  EXPECT_FALSE(has_transaction_residue(tree));
}

TEST(ResourceTransactionTest, CompensationRefusesConcurrentTargetOverwrite) {
  TempTree tree;
  ASSERT_TRUE(tree.valid()) << tree.error() << " root=" << tree.root().string();
  const std::string dependency = tree.path("dependent.tscn");
  const std::string source = tree.path("old.tres");
  const std::string destination = tree.path("new.tres");
  write_bytes(dependency, "old-dependency");
  write_bytes(source, "resource");
  FileTransactionPlan plan;
  plan.operations = {
      {FileTransactionKind::kWrite, {}, dependency, "new-dependency", true, true},
      {FileTransactionKind::kMove, source, destination, {}, false, false},
  };
  class RacingFs final : public InjectedFileTransactionFs {
    public:
    std::string dependency;
    bool raced = false;
    bool commit_staged(const std::string &staged, const std::string &target,
                       std::string &error) override {
      if (target == dependency && !raced) {
        raced = true;
        if (!InjectedFileTransactionFs::commit_staged(staged, target, error)) {
          return false;
        }
        write_bytes(target, "concurrent-user-edit");
        return true;
      }
      return InjectedFileTransactionFs::commit_staged(staged, target, error);
    }
  };
  RacingFs file_system;
  file_system.dependency = dependency;
  file_system.fail_commit_at = 2;
  FileTransactionJournal journal;
  const FileTransactionResult result =
      commit_file_transaction(plan, journal, file_system);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(result.compensated);
  EXPECT_FALSE(result.compensation_errors.empty());
  EXPECT_EQ(read_bytes(dependency), "concurrent-user-edit");
  EXPECT_EQ(read_bytes(source), "resource");
}

TEST(ResourceTransactionSchemaTest, CopyFlagsAreOptionalBooleans) {
  for (const std::string &name : {"os_file_write", "resource_file"}) {
    std::unique_ptr<const mcp::JsonValue> properties(
        schema_properties(name));
    ASSERT_NE(properties, nullptr) << name;
    for (const std::string &flag : {"overwrite", "confirm"}) {
      const mcp::JsonValue *parameter = properties->Find(flag.c_str());
      ASSERT_NE(parameter, nullptr) << name << ":" << flag;
      ASSERT_TRUE(parameter->IsObject());
      const mcp::JsonValue *type = parameter->Find("type");
      ASSERT_NE(type, nullptr);
      EXPECT_EQ(type->GetString(), "boolean");
    }
  }
}

} // namespace
