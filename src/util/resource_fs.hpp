#ifndef GODOT_AUTOPILOT_RESOURCE_FS_HPP
#define GODOT_AUTOPILOT_RESOURCE_FS_HPP

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>
#include "util/sha256.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace resource_fs {

enum class TextWriteMode { kWrite, kAppend };

enum class TextWriteFailure {
  kNone,
  kSizeOverflow,
  kOpenWriteFailed,
  kAppendOpenFailed,
  kAppendTargetAppeared,
  kExistingReadFailed,
  kExistingSizeMismatch,
  kSeekFailed,
  kStoreFailed,
  kFlushFailed,
  kCloseFailed,
  kReadbackOpenFailed,
  kReadbackSizeMismatch,
  kReadbackReadFailed,
  kReadbackCloseFailed,
  kReadbackMismatch,
};

struct TextWriteResult {
  bool ok = false;
  bool created = false;
  bool append_created = false;
  TextWriteFailure failure = TextWriteFailure::kNone;
  godot::Error io_error = godot::OK;
  int64_t expected_bytes = 0;
  int64_t actual_bytes = -1;
  std::string message;
  std::string readback_content;
};

inline const char *text_write_failure_code(TextWriteFailure failure) {
  switch (failure) {
  case TextWriteFailure::kNone:
    return "none";
  case TextWriteFailure::kSizeOverflow:
    return "file_write_size_overflow";
  case TextWriteFailure::kOpenWriteFailed:
    return "file_write_open_failed";
  case TextWriteFailure::kAppendOpenFailed:
    return "append_open_failed";
  case TextWriteFailure::kAppendTargetAppeared:
    return "append_target_appeared";
  case TextWriteFailure::kExistingReadFailed:
    return "append_existing_read_failed";
  case TextWriteFailure::kExistingSizeMismatch:
    return "append_existing_size_mismatch";
  case TextWriteFailure::kSeekFailed:
    return "file_seek_failed";
  case TextWriteFailure::kStoreFailed:
    return "file_write_failed";
  case TextWriteFailure::kFlushFailed:
    return "file_flush_failed";
  case TextWriteFailure::kCloseFailed:
    return "file_close_failed";
  case TextWriteFailure::kReadbackOpenFailed:
    return "file_readback_open_failed";
  case TextWriteFailure::kReadbackSizeMismatch:
    return "file_readback_size_mismatch";
  case TextWriteFailure::kReadbackReadFailed:
    return "file_readback_read_failed";
  case TextWriteFailure::kReadbackCloseFailed:
    return "file_readback_close_failed";
  case TextWriteFailure::kReadbackMismatch:
    return "file_readback_mismatch";
  }
  return "file_write_failed";
}

class TextWriteFs {
 public:
  virtual ~TextWriteFs() = default;
  virtual bool file_exists(const std::string &path) const = 0;
  virtual bool open(const std::string &path,
                    godot::FileAccess::ModeFlags mode) = 0;
  virtual bool is_open() const = 0;
  virtual int64_t length() const = 0;
  virtual bool read_all(std::string &out) = 0;
  virtual bool seek_end() = 0;
  virtual bool store_string(const std::string &content) = 0;
  virtual godot::Error flush() = 0;
  virtual void close() = 0;
  virtual godot::Error get_error() const = 0;
};

class GodotTextWriteFs final : public TextWriteFs {
 public:
  bool file_exists(const std::string &path) const override {
    return godot::FileAccess::file_exists(godot::String::utf8(path.c_str()));
  }

  bool open(const std::string &path,
            godot::FileAccess::ModeFlags mode) override {
    file_ = godot::FileAccess::open(godot::String::utf8(path.c_str()), mode);
    if (file_.is_null()) {
      last_error_ = godot::ERR_FILE_CANT_OPEN;
      return false;
    }
    if (!file_->is_open()) {
      const godot::Error error = file_->get_error();
      last_error_ = error == godot::OK ? godot::ERR_FILE_CANT_OPEN : error;
      return false;
    }
    last_error_ = godot::OK;
    return true;
  }

  bool is_open() const override {
    return !file_.is_null() && file_->is_open();
  }

  int64_t length() const override {
    if (file_.is_null()) {
      return -1;
    }
    const uint64_t value = file_->get_length();
    if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      return -1;
    }
    return static_cast<int64_t>(value);
  }

  bool read_all(std::string &out) override {
    if (file_.is_null()) {
      last_error_ = godot::ERR_FILE_CANT_READ;
      return false;
    }
    const godot::CharString utf8 = file_->get_as_text().utf8();
    out.assign(utf8.ptr(), static_cast<size_t>(utf8.length()));
    const godot::Error error = file_->get_error();
    return error == godot::OK || error == godot::ERR_FILE_EOF;
  }

  bool seek_end() override {
    if (file_.is_null()) {
      last_error_ = godot::ERR_FILE_CANT_OPEN;
      return false;
    }
    file_->seek_end();
    return file_->get_position() == file_->get_length();
  }

  bool store_string(const std::string &content) override {
    if (file_.is_null()) {
      last_error_ = godot::ERR_FILE_CANT_WRITE;
      return false;
    }
    return file_->store_string(godot::String::utf8(
        content.c_str(), static_cast<int>(content.size())));
  }

  godot::Error flush() override {
    if (file_.is_null()) {
      last_error_ = godot::ERR_FILE_CANT_WRITE;
      return last_error_;
    }
    file_->flush();
    return file_->get_error();
  }

  void close() override {
    if (!file_.is_null()) {
      file_->close();
    }
  }

  godot::Error get_error() const override {
    return file_.is_null() ? last_error_ : file_->get_error();
  }

 private:
  godot::Ref<godot::FileAccess> file_;
  godot::Error last_error_ = godot::OK;
};

inline bool acceptable_read_error(godot::Error error) {
  return error == godot::OK || error == godot::ERR_FILE_EOF;
}

inline TextWriteResult verify_text_file_impl(
    const std::string &path, const std::string &expected, TextWriteFs &fs) {
  TextWriteResult result;
  if (expected.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    result.failure = TextWriteFailure::kSizeOverflow;
    result.message = "expected file size exceeds the Godot string limit";
    return result;
  }
  result.expected_bytes = static_cast<int64_t>(expected.size());
  const auto fail = [&](TextWriteFailure failure, godot::Error error,
                        std::string message) {
    result.failure = failure;
    result.io_error = error;
    result.message = std::move(message);
    if (fs.is_open()) {
      fs.close();
    }
    return result;
  };

  if (!fs.open(path, godot::FileAccess::READ)) {
    return fail(TextWriteFailure::kReadbackOpenFailed, fs.get_error(),
                "failed to open file for readback: " + path);
  }
  result.actual_bytes = fs.length();
  if (result.actual_bytes < 0) {
    return fail(TextWriteFailure::kReadbackSizeMismatch, fs.get_error(),
                "failed to read file length during readback: " + path);
  }
  if (result.actual_bytes != result.expected_bytes) {
    return fail(TextWriteFailure::kReadbackSizeMismatch, fs.get_error(),
                "readback length mismatch for " + path + ": expected " +
                    std::to_string(result.expected_bytes) + " bytes, read " +
                    std::to_string(result.actual_bytes) + " bytes");
  }

  std::string actual;
  const bool read_ok = fs.read_all(actual);
  const godot::Error read_error = fs.get_error();
  fs.close();
  const godot::Error close_error = fs.get_error();
  if (!read_ok || !acceptable_read_error(read_error)) {
    return fail(TextWriteFailure::kReadbackReadFailed, read_error,
                "failed to read file during readback: " + path);
  }
  if (!acceptable_read_error(close_error)) {
    return fail(TextWriteFailure::kReadbackCloseFailed, close_error,
                "failed to close file after readback: " + path);
  }
  if (actual.size() != expected.size()) {
    return fail(TextWriteFailure::kReadbackMismatch, godot::OK,
                "readback content length mismatch for " + path + ": expected " +
                    std::to_string(expected.size()) + " bytes, read " +
                    std::to_string(actual.size()) + " bytes");
  }
  if (actual != expected) {
    return fail(TextWriteFailure::kReadbackMismatch, godot::OK,
                "readback content mismatch for " + path);
  }
  result.ok = true;
  result.readback_content = std::move(actual);
  return result;
}

inline TextWriteResult verify_text_file(const std::string &path,
                                        const std::string &expected,
                                        TextWriteFs &fs) {
  return verify_text_file_impl(path, expected, fs);
}

inline TextWriteResult write_text_verified(const std::string &path,
                                           const std::string &content,
                                           TextWriteMode mode,
                                           TextWriteFs &fs) {
  TextWriteResult result;
  if (content.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    result.failure = TextWriteFailure::kSizeOverflow;
    result.message = "content size exceeds the Godot string limit";
    return result;
  }
  result.expected_bytes = static_cast<int64_t>(content.size());
  const auto fail = [&](TextWriteFailure failure, godot::Error error,
                        std::string message) {
    result.failure = failure;
    result.io_error = error;
    result.message = std::move(message);
    if (fs.is_open()) {
      fs.close();
    }
    return result;
  };

  const bool existed = fs.file_exists(path);
  std::string expected_content = content;
  if (mode == TextWriteMode::kAppend) {
    const bool append_opened = fs.open(path, godot::FileAccess::READ_WRITE);
    if (!append_opened) {
      const godot::Error open_error = fs.get_error();
      if (existed) {
        return fail(TextWriteFailure::kAppendOpenFailed, open_error,
                    "failed to open existing file for APPEND: " + path);
      }
      if (fs.file_exists(path)) {
        return fail(TextWriteFailure::kAppendTargetAppeared, open_error,
                    "APPEND target appeared before creation: " + path);
      }
      if (!fs.open(path, godot::FileAccess::WRITE)) {
        return fail(TextWriteFailure::kOpenWriteFailed, fs.get_error(),
                    "failed to create APPEND target: " + path);
      }
      result.created = true;
      result.append_created = true;
    } else {
      const int64_t original_length = fs.length();
      if (original_length < 0) {
        return fail(TextWriteFailure::kExistingReadFailed, fs.get_error(),
                    "failed to read existing file length for APPEND: " + path);
      }
      if (original_length > std::numeric_limits<int>::max()) {
        return fail(TextWriteFailure::kSizeOverflow, godot::OK,
                    "existing file exceeds the Godot string limit");
      }
      std::string original;
      const bool read_ok = fs.read_all(original);
      const godot::Error read_error = fs.get_error();
      if (!read_ok || !acceptable_read_error(read_error)) {
        return fail(TextWriteFailure::kExistingReadFailed, read_error,
                    "failed to read existing file for APPEND: " + path);
      }
      if (original.size() != static_cast<size_t>(original_length)) {
        return fail(TextWriteFailure::kExistingSizeMismatch, read_error,
                    "existing file length mismatch before APPEND: " + path);
      }
      if (content.size() >
          static_cast<size_t>(std::numeric_limits<int>::max() -
                              original_length)) {
        return fail(TextWriteFailure::kSizeOverflow, godot::OK,
                    "APPEND result exceeds the supported file length");
      }
      expected_content = original + content;
      result.expected_bytes =
          static_cast<int64_t>(expected_content.size());
      if (!fs.seek_end()) {
        return fail(TextWriteFailure::kSeekFailed, fs.get_error(),
                    "failed to seek to end before APPEND: " + path);
      }
    }
  } else if (!fs.open(path, godot::FileAccess::WRITE)) {
    return fail(TextWriteFailure::kOpenWriteFailed, fs.get_error(),
                "failed to open file for WRITE: " + path);
  } else {
    result.created = !existed;
  }

  if (!fs.is_open()) {
    return fail(TextWriteFailure::kOpenWriteFailed, fs.get_error(),
                "file is not open for writing: " + path);
  }
  if (!fs.store_string(content)) {
    const godot::Error write_error = fs.get_error();
    return fail(TextWriteFailure::kStoreFailed,
                write_error == godot::OK ? godot::ERR_FILE_CANT_WRITE
                                         : write_error,
                "failed to store file content: " + path);
  }
  const godot::Error write_error = fs.get_error();
  if (write_error != godot::OK) {
    return fail(TextWriteFailure::kStoreFailed, write_error,
                "file write returned an I/O error: " + path);
  }
  const godot::Error flush_error = fs.flush();
  if (flush_error != godot::OK) {
    return fail(TextWriteFailure::kFlushFailed, flush_error,
                "failed to flush file: " + path);
  }
  fs.close();
  const godot::Error close_error = fs.get_error();
  if (close_error != godot::OK) {
    return fail(TextWriteFailure::kCloseFailed, close_error,
                "failed to close file after writing: " + path);
  }

  TextWriteResult verified = verify_text_file_impl(path, expected_content, fs);
  verified.created = result.created;
  verified.append_created = result.append_created;
  return verified;
}

inline bool is_resource_root_dir(const std::string &dir) {
  return dir.empty() || dir == "res://" || dir == "res:/" || dir == "res:" ||
         dir == "user://" || dir == "user:/" || dir == "user:";
}

inline std::string parent_directory_of(const std::string &resource_path) {
  const godot::String path = godot::String::utf8(resource_path.c_str());
  return std::string(path.get_base_dir().utf8().get_data());
}

inline bool ensure_parent_directory(const std::string &resource_path,
                                    std::string &dir_created,
                                    std::string &error_out) {
  dir_created.clear();
  error_out.clear();
  const std::string parent = parent_directory_of(resource_path);
  if (is_resource_root_dir(parent)) {
    return true;
  }
  const godot::String parent_gs = godot::String::utf8(parent.c_str());
  if (godot::DirAccess::dir_exists_absolute(parent_gs)) {
    return true;
  }
  const godot::Error mk_err =
      godot::DirAccess::make_dir_recursive_absolute(parent_gs);
  if (mk_err != godot::Error::OK) {
    error_out = "failed to create directory: " + parent + " (error " +
                std::to_string(static_cast<int>(mk_err)) + ")";
    return false;
  }
  dir_created = parent;
  return true;
}

struct FileTransactionSnapshot {
  bool exists = false;
  bool is_directory = false;
  uintmax_t size = 0;
  int64_t modified_time = 0;
  std::string sha256;

  bool same_file_as(const FileTransactionSnapshot &other) const {
    return exists == other.exists && is_directory == other.is_directory &&
           size == other.size && sha256 == other.sha256;
  }
};

class FileTransactionFs {
  public:
  virtual ~FileTransactionFs() = default;
  virtual bool inspect(const std::string &path,
                       FileTransactionSnapshot &snapshot,
                       std::string &error) = 0;
  virtual bool stage_copy(const std::string &source, const std::string &staged,
                          std::string &error) = 0;
  virtual bool stage_write(const std::string &staged, const std::string &content,
                           std::string &error) = 0;
  virtual bool rename_unique(const std::string &source,
                             const std::string &destination,
                             std::string &error) = 0;
  virtual bool commit_staged(const std::string &staged,
                             const std::string &destination,
                             std::string &error) = 0;
  virtual bool remove_file(const std::string &path, std::string &error) = 0;
};

class GodotFileTransactionFs final : public FileTransactionFs {
  public:
  bool inspect(const std::string &path, FileTransactionSnapshot &snapshot,
               std::string &error) override {
    snapshot = FileTransactionSnapshot();
    error.clear();
    const godot::String path_gs = godot::String::utf8(path.c_str());
    if (godot::DirAccess::dir_exists_absolute(path_gs)) {
      snapshot.exists = true;
      snapshot.is_directory = true;
      return true;
    }
    if (!godot::FileAccess::file_exists(path_gs)) {
      return true;
    }
    godot::Ref<godot::FileAccess> file =
        godot::FileAccess::open(path_gs, godot::FileAccess::READ);
    if (file.is_null()) {
      error = "failed to open file for transaction inspection: " + path;
      return false;
    }
    snapshot.exists = true;
    snapshot.size = file->get_length();
    snapshot.modified_time =
        static_cast<int64_t>(godot::FileAccess::get_modified_time(path_gs));
    snapshot.sha256 =
        std::string(godot::FileAccess::get_sha256(path_gs).utf8().get_data());
    const godot::Error read_error = file->get_error();
    file->close();
    if (read_error != godot::OK && read_error != godot::ERR_FILE_EOF) {
      error = "failed to read file for transaction inspection: " + path;
      return false;
    }
    if (snapshot.sha256.size() != 64) {
      error = "failed to hash file for transaction inspection: " + path;
      return false;
    }
    return true;
  }

  bool stage_copy(const std::string &source, const std::string &staged,
                  std::string &error) override {
    if (path_exists(staged)) {
      error = "transaction staging path already exists: " + staged;
      return false;
    }
    const godot::Error result = godot::DirAccess::copy_absolute(
        godot::String::utf8(source.c_str()), godot::String::utf8(staged.c_str()));
    if (result != godot::OK) {
      error = "failed to stage copy " + source + " -> " + staged + " (error " +
              std::to_string(static_cast<int>(result)) + ")";
      return false;
    }
    return true;
  }

  bool stage_write(const std::string &staged, const std::string &content,
                   std::string &error) override {
    GodotTextWriteFs file_system;
    const TextWriteResult written = write_text_verified(
        staged, content, TextWriteMode::kWrite, file_system);
    if (!written.ok) {
      error = "failed to stage verified write " + staged + ": " +
              written.message;
      return false;
    }
    return true;
  }

  bool rename_unique(const std::string &source,
                     const std::string &destination,
                     std::string &error) override {
    std::filesystem::path source_fs;
    std::filesystem::path destination_fs;
    if (!native_paths(source, destination, source_fs, destination_fs, error)) {
      return false;
    }
    std::error_code ec;
    if (std::filesystem::exists(destination_fs, ec) || ec) {
      error = "transaction backup path already exists: " + destination;
      return false;
    }
    std::filesystem::rename(source_fs, destination_fs, ec);
    if (ec) {
      error = "failed to back up " + source + " -> " + destination + ": " +
              ec.message();
      return false;
    }
    return true;
  }

  bool commit_staged(const std::string &staged,
                     const std::string &destination,
                     std::string &error) override {
    std::filesystem::path staged_fs;
    std::filesystem::path destination_fs;
    if (!native_paths(staged, destination, staged_fs, destination_fs, error)) {
      return false;
    }
    std::error_code ec;
    std::filesystem::create_hard_link(staged_fs, destination_fs, ec);
    if (ec) {
      error = "failed to commit staged file without overwriting " + destination +
              ": " + ec.message();
      return false;
    }
    std::error_code cleanup_ec;
    std::filesystem::remove(staged_fs, cleanup_ec);
    return true;
  }

  bool remove_file(const std::string &path, std::string &error) override {
    const godot::Error result = godot::DirAccess::remove_absolute(
        godot::String::utf8(path.c_str()));
    if (result != godot::OK) {
      error = "failed to remove transaction file " + path + " (error " +
              std::to_string(static_cast<int>(result)) + ")";
      return false;
    }
    return true;
  }

  private:
  static bool path_exists(const std::string &path) {
    const godot::String path_gs = godot::String::utf8(path.c_str());
    return godot::FileAccess::file_exists(path_gs) ||
           godot::DirAccess::dir_exists_absolute(path_gs);
  }

  static bool native_paths(const std::string &source,
                           const std::string &destination,
                           std::filesystem::path &source_fs,
                           std::filesystem::path &destination_fs,
                           std::string &error) {
    auto *settings = godot::ProjectSettings::get_singleton();
    if (settings == nullptr) {
      error = "ProjectSettings is unavailable for transaction path resolution";
      return false;
    }
    const godot::String source_global =
        settings->globalize_path(godot::String::utf8(source.c_str()));
    const godot::String destination_global = settings->globalize_path(
        godot::String::utf8(destination.c_str()));
    if (source_global.is_empty() || destination_global.is_empty()) {
      error = "transaction path could not be globalized";
      return false;
    }
    source_fs = std::filesystem::u8path(source_global.utf8().get_data());
    destination_fs =
        std::filesystem::u8path(destination_global.utf8().get_data());
    return true;
  }
};

enum class FileTransactionKind { kCopy, kMove, kWrite, kRequireAbsent };

struct FileTransactionOp {
  FileTransactionKind kind = FileTransactionKind::kCopy;
  std::string source;
  std::string destination;
  std::string content;
  bool has_content = false;
  bool overwrite = false;
};

enum class FileTransactionJournalKind {
  kTargetBackedUp,
  kTargetCommitted,
  kSourceRemoved
};

struct FileTransactionJournalEntry {
  FileTransactionJournalKind kind = FileTransactionJournalKind::kTargetCommitted;
  size_t op_index = 0;
  std::string source;
  std::string destination;
  std::string backup;
  FileTransactionSnapshot before;
  FileTransactionSnapshot after;
};

struct FileTransactionJournal {
  std::vector<FileTransactionJournalEntry> entries;
};

struct FileTransactionPlan {
  std::vector<FileTransactionOp> operations;
};

struct FileTransactionResult {
  bool ok = false;
  bool compensated = false;
  std::string code;
  std::string message;
  std::vector<std::string> compensation_errors;
};

enum class CopyOverwriteFailure { kNone, kTargetExists, kConfirmationRequired };

inline CopyOverwriteFailure check_copy_overwrite(bool destination_exists,
                                                 bool overwrite,
                                                 bool confirm) {
  if (!destination_exists) {
    return CopyOverwriteFailure::kNone;
  }
  if (!overwrite) {
    return CopyOverwriteFailure::kTargetExists;
  }
  if (!confirm) {
    return CopyOverwriteFailure::kConfirmationRequired;
  }
  return CopyOverwriteFailure::kNone;
}

inline uint64_t next_file_transaction_serial() {
  static std::atomic<uint64_t> serial{0};
  return serial.fetch_add(1) + 1;
}

inline std::string sha256_hex(std::string_view content) {
  return godot_autopilot::util::sha256_hex(content);
}

inline std::string file_transaction_temp_path(const std::string &destination,
                                              uint64_t serial, size_t index) {
  return destination + ".gda-txn-" + std::to_string(serial) + "-" +
         std::to_string(index);
}

inline FileTransactionResult commit_file_transaction(
    const FileTransactionPlan &plan, FileTransactionJournal &journal,
    FileTransactionFs &fs) {
  struct Prepared {
    FileTransactionOp op;
    FileTransactionSnapshot source_before;
    FileTransactionSnapshot destination_before;
    FileTransactionSnapshot staged_snapshot;
    std::string staged;
    std::string backup;
    bool backed_up = false;
  };

  FileTransactionResult result;
  journal.entries.clear();
  std::vector<Prepared> prepared;
  prepared.reserve(plan.operations.size());
  std::set<std::string> mutation_destinations;
  std::set<std::string> move_sources;
  const uint64_t serial = next_file_transaction_serial();

  const auto fail = [&](const std::string &code, const std::string &message) {
    result.ok = false;
    result.code = code;
    result.message = message;
    if (!journal.entries.empty()) {
      result.compensated = true;
      std::set<size_t> protected_targets;
      for (auto entry = journal.entries.rbegin();
           entry != journal.entries.rend(); ++entry) {
        std::string error;
        if (entry->kind == FileTransactionJournalKind::kSourceRemoved) {
          FileTransactionSnapshot source_now;
          if (!fs.inspect(entry->source, source_now, error)) {
            result.compensation_errors.push_back(error);
            protected_targets.insert(entry->op_index);
            continue;
          }
          if (source_now.exists) {
            if (!source_now.same_file_as(entry->before)) {
              result.compensation_errors.push_back(
                  "refusing to replace concurrently modified source: " +
                  entry->source);
              protected_targets.insert(entry->op_index);
            }
            continue;
          }
          FileTransactionSnapshot target_now;
          if (!fs.inspect(entry->destination, target_now, error)) {
            result.compensation_errors.push_back(error);
            protected_targets.insert(entry->op_index);
            continue;
          }
          if (!target_now.same_file_as(entry->after)) {
            result.compensation_errors.push_back(
                "refusing to restore source from modified target: " +
                entry->destination);
            protected_targets.insert(entry->op_index);
            continue;
          }
          if (!fs.commit_staged(entry->destination, entry->source, error)) {
            result.compensation_errors.push_back(error);
            protected_targets.insert(entry->op_index);
          }
          continue;
        }

        if (protected_targets.count(entry->op_index) != 0) {
          continue;
        }
        FileTransactionSnapshot target_now;
        if (!fs.inspect(entry->destination, target_now, error)) {
          result.compensation_errors.push_back(error);
          protected_targets.insert(entry->op_index);
          continue;
        }
        if (target_now.exists) {
          if (entry->kind == FileTransactionJournalKind::kTargetCommitted &&
              target_now.same_file_as(entry->after)) {
            if (!fs.remove_file(entry->destination, error)) {
              result.compensation_errors.push_back(error);
              protected_targets.insert(entry->op_index);
              continue;
            }
          } else if (!target_now.same_file_as(entry->before)) {
            result.compensation_errors.push_back(
                "refusing to replace concurrently modified target: " +
                entry->destination);
            protected_targets.insert(entry->op_index);
            continue;
          }
        }
        if (entry->kind == FileTransactionJournalKind::kTargetBackedUp ||
            !entry->backup.empty()) {
          FileTransactionSnapshot backup_now;
          if (!fs.inspect(entry->backup, backup_now, error)) {
            result.compensation_errors.push_back(error);
            continue;
          }
          if (backup_now.exists) {
            FileTransactionSnapshot restore_target;
            std::string restore_error;
            if (!fs.inspect(entry->destination, restore_target,
                            restore_error) ||
                restore_target.exists ||
                !fs.commit_staged(entry->backup, entry->destination,
                                  restore_error)) {
              result.compensation_errors.push_back(
                  restore_error.empty()
                      ? "failed to restore transaction backup for " +
                            entry->destination
                      : restore_error);
            }
          }
        }
      }
    }
    for (const Prepared &item : prepared) {
      if (!item.staged.empty()) {
        std::string ignored;
        fs.remove_file(item.staged, ignored);
      }
      if (result.compensation_errors.empty() && !item.backup.empty()) {
        std::string ignored;
        fs.remove_file(item.backup, ignored);
      }
    }
    if (!result.compensation_errors.empty()) {
      result.message += "; transaction compensation incomplete";
    }
    return result;
  };

  for (size_t i = 0; i < plan.operations.size(); ++i) {
    const FileTransactionOp &op = plan.operations[i];
    if (op.destination.empty()) {
      return fail("transaction_failed", "transaction destination is empty");
    }
    if (op.kind != FileTransactionKind::kWrite &&
        op.kind != FileTransactionKind::kRequireAbsent && op.source.empty()) {
      return fail("transaction_failed", "transaction source is empty");
    }
    if ((op.kind == FileTransactionKind::kCopy ||
         op.kind == FileTransactionKind::kMove) &&
        op.source == op.destination) {
      return fail("transaction_failed", "transaction source equals destination");
    }
    if (op.kind == FileTransactionKind::kMove && op.overwrite) {
      return fail("transaction_failed", "move transactions cannot overwrite");
    }
    if (op.kind != FileTransactionKind::kRequireAbsent &&
        !mutation_destinations.insert(op.destination).second) {
      return fail("transaction_failed",
                  "duplicate transaction destination: " + op.destination);
    }
    if (op.kind == FileTransactionKind::kCopy ||
        op.kind == FileTransactionKind::kMove) {
      move_sources.insert(op.source);
    }

    Prepared item;
    item.op = op;
    std::string error;
    if (!fs.inspect(op.destination, item.destination_before, error)) {
      return fail("transaction_failed", error);
    }
    if (item.destination_before.is_directory) {
      return fail("target_conflict", "transaction target is a directory: " +
                                         op.destination);
    }
    if (item.destination_before.exists &&
        (op.kind == FileTransactionKind::kRequireAbsent ||
         (op.kind != FileTransactionKind::kWrite && !op.overwrite))) {
      return fail("target_conflict",
                  "transaction target already exists: " + op.destination);
    }
    if (op.kind == FileTransactionKind::kCopy ||
        op.kind == FileTransactionKind::kMove) {
      if (!fs.inspect(op.source, item.source_before, error)) {
        return fail("transaction_failed", error);
      }
      if (!item.source_before.exists || item.source_before.is_directory) {
        return fail("transaction_failed",
                    "transaction source is not a file: " + op.source);
      }
    }
    item.staged = file_transaction_temp_path(op.destination, serial, i);
    item.backup = file_transaction_temp_path(op.destination, serial + 1000000, i);
    prepared.push_back(std::move(item));
  }

  for (const Prepared &item : prepared) {
    if (item.op.kind == FileTransactionKind::kRequireAbsent) {
      continue;
    }
    if (move_sources.count(item.op.destination) != 0) {
      return fail("transaction_failed",
                  "transaction destination is another operation's source: " +
                      item.op.destination);
    }
  }

  for (Prepared &item : prepared) {
    if (item.op.kind == FileTransactionKind::kRequireAbsent) {
      continue;
    }
    std::string error;
    bool staged_ok = false;
    if (item.op.kind == FileTransactionKind::kWrite ||
        (item.op.kind == FileTransactionKind::kMove && item.op.has_content)) {
      staged_ok = fs.stage_write(item.staged, item.op.content, error);
    } else {
      staged_ok = fs.stage_copy(item.op.source, item.staged, error);
    }
    if (!staged_ok) {
      return fail("write_failed", error);
    }
    if (!fs.inspect(item.staged, item.staged_snapshot, error)) {
      return fail("write_failed", error);
    }
    if (!item.staged_snapshot.exists || item.staged_snapshot.is_directory) {
      return fail("write_failed", "transaction staging did not create a file");
    }
    if (item.op.kind == FileTransactionKind::kWrite ||
        item.op.has_content) {
      const auto expected = static_cast<uintmax_t>(item.op.content.size());
      if (item.staged_snapshot.size != expected ||
          item.staged_snapshot.sha256 != sha256_hex(item.op.content)) {
        return fail("readback_mismatch",
                    "staged write hash mismatch for " + item.op.destination);
      }
    } else if (!item.staged_snapshot.same_file_as(item.source_before)) {
      return fail("readback_mismatch",
                  "staged copy hash mismatch for " + item.op.destination);
    }
  }

  for (const Prepared &item : prepared) {
    std::string error;
    FileTransactionSnapshot destination_now;
    if (!fs.inspect(item.op.destination, destination_now, error)) {
      return fail("transaction_failed", error);
    }
    if (!destination_now.same_file_as(item.destination_before)) {
      return fail("target_modified",
                  "transaction target changed during staging: " +
                      item.op.destination);
    }
    if (item.op.kind == FileTransactionKind::kCopy ||
        item.op.kind == FileTransactionKind::kMove) {
      FileTransactionSnapshot source_now;
      if (!fs.inspect(item.op.source, source_now, error)) {
        return fail("transaction_failed", error);
      }
      if (!source_now.same_file_as(item.source_before)) {
        return fail("source_modified",
                    "transaction source changed during staging: " +
                        item.op.source);
      }
    }
  }

  for (size_t i = 0; i < prepared.size(); ++i) {
    Prepared &item = prepared[i];
    std::string error;
    if (item.op.kind == FileTransactionKind::kRequireAbsent) {
      FileTransactionSnapshot destination_now;
      if (!fs.inspect(item.op.destination, destination_now, error)) {
        return fail("transaction_failed", error);
      }
      if (destination_now.exists) {
        return fail("target_conflict",
                    "transaction target appeared before commit: " +
                        item.op.destination);
      }
      continue;
    }

    FileTransactionSnapshot destination_now;
    if (!fs.inspect(item.op.destination, destination_now, error)) {
      return fail("transaction_failed", error);
    }
    if (!destination_now.same_file_as(item.destination_before)) {
      return fail("target_modified",
                  "transaction target changed before commit: " +
                      item.op.destination);
    }
    if (item.op.kind == FileTransactionKind::kCopy ||
        item.op.kind == FileTransactionKind::kMove) {
      FileTransactionSnapshot source_now;
      if (!fs.inspect(item.op.source, source_now, error)) {
        return fail("transaction_failed", error);
      }
      if (!source_now.same_file_as(item.source_before)) {
        return fail("source_modified",
                    "transaction source changed before commit: " +
                        item.op.source);
      }
    }

    if (item.destination_before.exists) {
      if (!fs.rename_unique(item.op.destination, item.backup, error)) {
        return fail("write_failed", error);
      }
      item.backed_up = true;
      journal.entries.push_back({FileTransactionJournalKind::kTargetBackedUp, i,
                                 item.op.destination, item.backup, item.backup,
                                 item.destination_before, item.destination_before});
      FileTransactionSnapshot backup_now;
      if (!fs.inspect(item.backup, backup_now, error) ||
          !backup_now.same_file_as(item.destination_before)) {
        return fail("target_modified",
                    "transaction target changed while being backed up: " +
                        item.op.destination);
      }
    }
    if (!fs.commit_staged(item.staged, item.op.destination, error)) {
      return fail("write_failed", error);
    }
    journal.entries.push_back({FileTransactionJournalKind::kTargetCommitted, i,
                               item.op.source, item.op.destination, item.backup,
                               item.destination_before, item.staged_snapshot});
    FileTransactionSnapshot committed;
    if (!fs.inspect(item.op.destination, committed, error)) {
      return fail("readback_mismatch", error);
    }
    if (!committed.same_file_as(item.staged_snapshot)) {
      return fail("readback_mismatch",
                  "committed file hash mismatch for " + item.op.destination);
    }
    if (item.op.kind == FileTransactionKind::kMove) {
      FileTransactionSnapshot source_now;
      if (!fs.inspect(item.op.source, source_now, error)) {
        return fail("transaction_failed", error);
      }
      if (!source_now.same_file_as(item.source_before)) {
        return fail("source_modified",
                    "transaction source changed before removal: " +
                        item.op.source);
      }
      if (!fs.remove_file(item.op.source, error)) {
        return fail("write_failed", error);
      }
      journal.entries.push_back({FileTransactionJournalKind::kSourceRemoved, i,
                                 item.op.source, item.op.destination,
                                 item.backup, item.source_before,
                                 item.staged_snapshot});
    }
  }

  for (Prepared &item : prepared) {
    if (item.backed_up || item.backup.empty()) {
      std::string ignored;
      fs.remove_file(item.backup, ignored);
    }
    if (!item.staged.empty()) {
      std::string ignored;
      fs.remove_file(item.staged, ignored);
    }
  }
  result.ok = true;
  result.code = "ok";
  return result;
}

} // namespace resource_fs
} // namespace godot_autopilot

#endif
