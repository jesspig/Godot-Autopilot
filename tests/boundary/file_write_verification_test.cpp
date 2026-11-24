#include "tools/domains/text/text_ops.hpp"
#include "util/resource_fs.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using godot_autopilot::resource_fs::TextWriteFailure;
using godot_autopilot::resource_fs::TextWriteFs;
using godot_autopilot::resource_fs::TextWriteMode;
using godot_autopilot::resource_fs::TextWriteResult;
using godot_autopilot::resource_fs::text_write_failure_code;
using godot_autopilot::resource_fs::verify_text_file;
using godot_autopilot::resource_fs::write_text_verified;

class FakeTextWriteFs final : public TextWriteFs {
 public:
  explicit FakeTextWriteFs(std::string initial, bool exists = true)
      : data_(std::move(initial)), exists_(exists) {}

  bool file_exists(const std::string &) const override {
    ++exists_checks_;
    return appear_after_first_exists_check_ && exists_checks_ > 1
               ? true
               : exists_;
  }

  bool open(const std::string &,
            godot::FileAccess::ModeFlags mode) override {
    open_modes_.push_back(mode);
    if (mode == godot::FileAccess::READ_WRITE) {
      if (!exists_ || deny_read_write_) {
        last_error_ = godot::ERR_FILE_CANT_OPEN;
        return false;
      }
    } else if (mode == godot::FileAccess::WRITE) {
      if (deny_write_open_) {
        last_error_ = godot::ERR_FILE_CANT_WRITE;
        return false;
      }
      data_.clear();
      position_ = 0;
      exists_ = true;
    } else if (mode == godot::FileAccess::READ) {
      if (!exists_ || deny_read_open_) {
        last_error_ = godot::ERR_FILE_CANT_READ;
        return false;
      }
      if (override_readback_) {
        data_ = readback_content_;
      }
      position_ = 0;
    } else {
      last_error_ = godot::ERR_INVALID_PARAMETER;
      return false;
    }
    open_ = true;
    last_error_ = godot::OK;
    return true;
  }

  bool is_open() const override { return open_; }

  int64_t length() const override {
    if (length_override_) {
      return length_override_value_;
    }
    return static_cast<int64_t>(data_.size());
  }

  bool read_all(std::string &out) override {
    if (read_error_ != godot::OK) {
      last_error_ = read_error_;
      return false;
    }
    out = data_;
    position_ = data_.size();
    last_error_ = godot::ERR_FILE_EOF;
    return true;
  }

  bool seek_end() override {
    position_ = data_.size();
    last_error_ = seek_error_;
    return seek_error_ == godot::OK;
  }

  bool store_string(const std::string &content) override {
    if (!store_result_) {
      last_error_ = store_io_error_;
      return false;
    }
    if (position_ > data_.size()) {
      data_.resize(position_);
    }
    data_.insert(position_, content);
    position_ += content.size();
    last_error_ = store_io_error_;
    return true;
  }

  godot::Error flush() override {
    last_error_ = flush_error_;
    return flush_error_;
  }

  void close() override {
    if (!open_) {
      return;
    }
    open_ = false;
    ++close_calls_;
    last_error_ = close_error_on_call_ == close_calls_ ? close_error_
                                                       : godot::OK;
  }

  godot::Error get_error() const override { return last_error_; }

  const std::string &data() const { return data_; }
  const std::vector<godot::FileAccess::ModeFlags> &open_modes() const {
    return open_modes_;
  }

  bool deny_read_write_ = false;
  bool deny_write_open_ = false;
  bool deny_read_open_ = false;
  bool appear_after_first_exists_check_ = false;
  bool store_result_ = true;
  godot::Error store_io_error_ = godot::OK;
  godot::Error seek_error_ = godot::OK;
  godot::Error flush_error_ = godot::OK;
  godot::Error close_error_ = godot::ERR_FILE_CANT_WRITE;
  int close_error_on_call_ = 0;
  godot::Error read_error_ = godot::OK;
  bool length_override_ = false;
  int64_t length_override_value_ = 0;
  bool override_readback_ = false;
  std::string readback_content_;

 private:
  std::string data_;
  bool exists_ = false;
  bool open_ = false;
  size_t position_ = 0;
  mutable size_t exists_checks_ = 0;
  int close_calls_ = 0;
  godot::Error last_error_ = godot::OK;
  std::vector<godot::FileAccess::ModeFlags> open_modes_;
};

TextWriteResult write(FakeTextWriteFs &fs, const std::string &content,
                      TextWriteMode mode) {
  return write_text_verified("res://write-target.txt", content, mode, fs);
}

TEST(FileWriteVerificationTest, AppendPreservesExistingContent) {
  FakeTextWriteFs fs("original\n");
  const TextWriteResult result = write(fs, "appended\n", TextWriteMode::kAppend);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_EQ(fs.data(), "original\nappended\n");
  EXPECT_EQ(result.expected_bytes, 18);
  EXPECT_EQ(result.actual_bytes, 18);
  EXPECT_EQ(result.readback_content, fs.data());
  ASSERT_EQ(fs.open_modes().size(), 2u);
  EXPECT_EQ(fs.open_modes()[0], godot::FileAccess::READ_WRITE);
  EXPECT_EQ(fs.open_modes()[1], godot::FileAccess::READ);
}

TEST(FileWriteVerificationTest, AppendCreatesOnlyMissingTarget) {
  FakeTextWriteFs fs("", false);
  const TextWriteResult result = write(fs, "new", TextWriteMode::kAppend);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_TRUE(result.created);
  EXPECT_TRUE(result.append_created);
  EXPECT_EQ(fs.data(), "new");
  ASSERT_EQ(fs.open_modes().size(), 3u);
  EXPECT_EQ(fs.open_modes()[0], godot::FileAccess::READ_WRITE);
  EXPECT_EQ(fs.open_modes()[1], godot::FileAccess::WRITE);
  EXPECT_EQ(fs.open_modes()[2], godot::FileAccess::READ);
}

TEST(FileWriteVerificationTest, ExistingAppendOpenFailureNeverFallsBackToWrite) {
  FakeTextWriteFs fs("must survive");
  fs.deny_read_write_ = true;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kAppend);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kAppendOpenFailed);
  EXPECT_EQ(fs.data(), "must survive");
  ASSERT_EQ(fs.open_modes().size(), 1u);
  EXPECT_EQ(fs.open_modes()[0], godot::FileAccess::READ_WRITE);
}

TEST(FileWriteVerificationTest, AppendTargetAppearingBeforeFallbackIsRejected) {
  FakeTextWriteFs fs("raced content", false);
  fs.appear_after_first_exists_check_ = true;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kAppend);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kAppendTargetAppeared);
  EXPECT_EQ(fs.data(), "raced content");
  ASSERT_EQ(fs.open_modes().size(), 1u);
  EXPECT_EQ(fs.open_modes()[0], godot::FileAccess::READ_WRITE);
}

TEST(FileWriteVerificationTest, WriteOpenFailureIsAnError) {
  FakeTextWriteFs fs("old");
  fs.deny_write_open_ = true;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kOpenWriteFailed);
  EXPECT_EQ(fs.data(), "old");
}

TEST(FileWriteVerificationTest, AppendCreateOpenFailureIsAnError) {
  FakeTextWriteFs fs("", false);
  fs.deny_write_open_ = true;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kAppend);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kOpenWriteFailed);
  EXPECT_TRUE(fs.data().empty());
  ASSERT_EQ(fs.open_modes().size(), 2u);
  EXPECT_EQ(fs.open_modes()[0], godot::FileAccess::READ_WRITE);
  EXPECT_EQ(fs.open_modes()[1], godot::FileAccess::WRITE);
}

TEST(FileWriteVerificationTest, StoreFailureIsAnError) {
  FakeTextWriteFs fs("old");
  fs.store_result_ = false;
  fs.store_io_error_ = godot::ERR_FILE_CANT_WRITE;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kStoreFailed);
  EXPECT_EQ(result.io_error, godot::ERR_FILE_CANT_WRITE);
  ASSERT_EQ(fs.open_modes().size(), 1u);
  EXPECT_EQ(fs.open_modes()[0], godot::FileAccess::WRITE);
}

TEST(FileWriteVerificationTest, SilentStoreErrorIsAnError) {
  FakeTextWriteFs fs("old");
  fs.store_io_error_ = godot::ERR_FILE_CANT_WRITE;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kStoreFailed);
  EXPECT_EQ(result.io_error, godot::ERR_FILE_CANT_WRITE);
}

TEST(FileWriteVerificationTest, FlushFailureIsAnError) {
  FakeTextWriteFs fs("old");
  fs.flush_error_ = godot::ERR_FILE_CANT_WRITE;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kFlushFailed);
  EXPECT_EQ(result.io_error, godot::ERR_FILE_CANT_WRITE);
}

TEST(FileWriteVerificationTest, WriterCloseFailureSkipsReadback) {
  FakeTextWriteFs fs("old");
  fs.close_error_on_call_ = 1;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kCloseFailed);
  EXPECT_EQ(result.io_error, godot::ERR_FILE_CANT_WRITE);
  ASSERT_EQ(fs.open_modes().size(), 1u);
}

TEST(FileWriteVerificationTest, ReadbackOpenFailureIsAnError) {
  FakeTextWriteFs fs("old");
  fs.deny_read_open_ = true;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kReadbackOpenFailed);
}

TEST(FileWriteVerificationTest, ReadbackLengthMismatchIsAnError) {
  FakeTextWriteFs fs("old");
  fs.length_override_ = true;
  fs.length_override_value_ = 4;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kReadbackSizeMismatch);
  EXPECT_EQ(result.expected_bytes, 3);
  EXPECT_EQ(result.actual_bytes, 4);
}

TEST(FileWriteVerificationTest, SameLengthReadbackMismatchIsAnError) {
  FakeTextWriteFs fs("old");
  fs.override_readback_ = true;
  fs.readback_content_ = "bad";
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kReadbackMismatch);
  EXPECT_EQ(result.expected_bytes, 3);
  EXPECT_EQ(result.actual_bytes, 3);
}

TEST(FileWriteVerificationTest, ReadbackReadFailureIsAnError) {
  FakeTextWriteFs fs("old");
  fs.read_error_ = godot::ERR_FILE_CANT_READ;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kReadbackReadFailed);
  EXPECT_EQ(result.io_error, godot::ERR_FILE_CANT_READ);
}

TEST(FileWriteVerificationTest, ReadbackCloseFailureIsAnError) {
  FakeTextWriteFs fs("old");
  fs.close_error_on_call_ = 2;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kWrite);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kReadbackCloseFailed);
  EXPECT_EQ(result.io_error, godot::ERR_FILE_CANT_WRITE);
}

TEST(FileWriteVerificationTest, AppendExistingReadFailureDoesNotWrite) {
  FakeTextWriteFs fs("must survive");
  fs.read_error_ = godot::ERR_FILE_CANT_READ;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kAppend);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kExistingReadFailed);
  EXPECT_EQ(fs.data(), "must survive");
  ASSERT_EQ(fs.open_modes().size(), 1u);
}

TEST(FileWriteVerificationTest, AppendExistingLengthMismatchDoesNotWrite) {
  FakeTextWriteFs fs("must survive");
  fs.length_override_ = true;
  fs.length_override_value_ = 99;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kAppend);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kExistingSizeMismatch);
  EXPECT_EQ(fs.data(), "must survive");
  ASSERT_EQ(fs.open_modes().size(), 1u);
}

TEST(FileWriteVerificationTest, AppendSeekFailureDoesNotWrite) {
  FakeTextWriteFs fs("must survive");
  fs.seek_error_ = godot::ERR_FILE_CANT_READ;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kAppend);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.failure, TextWriteFailure::kSeekFailed);
  EXPECT_EQ(fs.data(), "must survive");
}

TEST(FileWriteVerificationTest, EmptyWriteTruncatesAndVerifies) {
  FakeTextWriteFs fs("old content");
  const TextWriteResult result = write(fs, "", TextWriteMode::kWrite);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_FALSE(result.created);
  EXPECT_TRUE(fs.data().empty());
  EXPECT_EQ(result.expected_bytes, 0);
  EXPECT_EQ(result.actual_bytes, 0);
}

TEST(FileWriteVerificationTest, EmptyWriteCreatesAndVerifies) {
  FakeTextWriteFs fs("", false);
  const TextWriteResult result = write(fs, "", TextWriteMode::kWrite);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_TRUE(result.created);
  EXPECT_TRUE(fs.data().empty());
}

TEST(FileWriteVerificationTest, EmptyAppendPreservesExistingContent) {
  FakeTextWriteFs fs("unchanged");
  const TextWriteResult result = write(fs, "", TextWriteMode::kAppend);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_EQ(fs.data(), "unchanged");
  EXPECT_EQ(result.expected_bytes, 9);
}

TEST(FileWriteVerificationTest, EmptyAppendCreatesMissingFile) {
  FakeTextWriteFs fs("", false);
  const TextWriteResult result = write(fs, "", TextWriteMode::kAppend);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_TRUE(result.append_created);
  EXPECT_TRUE(fs.data().empty());
}

TEST(FileWriteVerificationTest, Utf8LengthAndContentAreVerified) {
  FakeTextWriteFs fs("");
  const std::string content = "A\xE4\xB8\xAD\xF0\x9F\x98\x80";
  const TextWriteResult result = write(fs, content, TextWriteMode::kWrite);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_EQ(result.expected_bytes, static_cast<int64_t>(content.size()));
  EXPECT_EQ(fs.data(), content);
}

TEST(FileWriteVerificationTest, EmbeddedNulContentIsVerifiedByLength) {
  FakeTextWriteFs fs("");
  const std::string content("a\0b", 3);
  const TextWriteResult result = write(fs, content, TextWriteMode::kWrite);
  ASSERT_TRUE(result.ok) << result.message;
  EXPECT_EQ(result.expected_bytes, 3);
  EXPECT_EQ(fs.data(), content);
}

TEST(FileWriteVerificationTest, LongWriteAndAppendAreVerified) {
  const std::string content(3 * 1024 * 1024 + 17, 'x');
  FakeTextWriteFs fs("prefix");
  const TextWriteResult appended =
      write(fs, content, TextWriteMode::kAppend);
  ASSERT_TRUE(appended.ok) << appended.message;
  EXPECT_EQ(fs.data(), "prefix" + content);
  EXPECT_EQ(appended.expected_bytes,
            static_cast<int64_t>(fs.data().size()));

  FakeTextWriteFs overwrite("stale");
  const TextWriteResult written =
      write(overwrite, content, TextWriteMode::kWrite);
  ASSERT_TRUE(written.ok) << written.message;
  EXPECT_EQ(overwrite.data(), content);
}

TEST(FileWriteVerificationTest, StandaloneVerificationChecksExactContent) {
  FakeTextWriteFs fs("exact");
  const TextWriteResult matched =
      verify_text_file("res://script.gd", "exact", fs);
  EXPECT_TRUE(matched.ok) << matched.message;

  FakeTextWriteFs mismatched("other");
  const TextWriteResult failed =
      verify_text_file("res://script.gd", "exact", mismatched);
  EXPECT_FALSE(failed.ok);
  EXPECT_EQ(failed.failure, TextWriteFailure::kReadbackMismatch);
}

TEST(FileWriteVerificationTest, FailureJsonIsStructuredAndHasNoOkResult) {
  FakeTextWriteFs fs("must survive");
  fs.deny_read_write_ = true;
  const TextWriteResult result = write(fs, "new", TextWriteMode::kAppend);
  const mcp::JsonValue error = godot_autopilot::text_ops::file_write_failure_json(
      "res://write-target.txt", "APPEND", result);
  EXPECT_EQ(error.Find("result"), nullptr);
  const mcp::JsonValue *message = error.Find("error");
  ASSERT_NE(message, nullptr);
  ASSERT_TRUE(message->IsString());
  EXPECT_NE(message->GetString().find("APPEND"), std::string::npos);
  const mcp::JsonValue *structured = error.Find("structured_error");
  ASSERT_NE(structured, nullptr);
  ASSERT_TRUE(structured->IsObject());
  const mcp::JsonValue *code = structured->Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), text_write_failure_code(result.failure));
  const mcp::JsonValue *path = structured->Find("path");
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->GetString(), "res://write-target.txt");
}

} // namespace
