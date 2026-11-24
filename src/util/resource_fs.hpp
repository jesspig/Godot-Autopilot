#ifndef GODOT_AUTOPILOT_RESOURCE_FS_HPP
#define GODOT_AUTOPILOT_RESOURCE_FS_HPP

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

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

} // namespace resource_fs
} // namespace godot_autopilot

#endif
