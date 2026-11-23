#include "tools/domains/editor/save_write_receipt.hpp"

#include <gtest/gtest.h>

using godot_autopilot::editor_ops_detail::decide_write_outcome;
using godot_autopilot::editor_ops_detail::FileStamp;
using godot_autopilot::editor_ops_detail::WriteOutcome;

TEST(SaveWriteReceiptTest, MissingTargetThenCreatedIsVerified) {
  FileStamp before;
  before.exists = false;
  FileStamp after;
  after.exists = true;
  after.modified_ns = 100;
  after.size_bytes = 10;
  EXPECT_EQ(decide_write_outcome(before, after), WriteOutcome::kVerified);
}

TEST(SaveWriteReceiptTest, ExistingTargetStillPresentWithoutChangeIsUnverifiable) {
  FileStamp before;
  before.exists = true;
  before.modified_ns = 100;
  before.size_bytes = 50;
  FileStamp after = before;
  EXPECT_EQ(decide_write_outcome(before, after), WriteOutcome::kUnverifiable)
      << "an existing file with no timestamp or size change cannot prove a new write";
}

TEST(SaveWriteReceiptTest, ExistingTargetWithNewTimestampIsVerified) {
  FileStamp before;
  before.exists = true;
  before.modified_ns = 100;
  before.size_bytes = 50;
  FileStamp after = before;
  after.modified_ns = 200;
  EXPECT_EQ(decide_write_outcome(before, after), WriteOutcome::kVerified);
}

TEST(SaveWriteReceiptTest, MissingTargetAfterSaveIsUnverifiable) {
  FileStamp before;
  before.exists = true;
  before.modified_ns = 100;
  FileStamp after;
  after.exists = false;
  EXPECT_EQ(decide_write_outcome(before, after), WriteOutcome::kUnverifiable);
}
