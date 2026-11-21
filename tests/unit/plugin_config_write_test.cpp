#include "core/plugin_config.hpp"
#include "core/path_util.hpp"
#include "support/test_temp.hpp"

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

using godot_autopilot::ConfigReadStatus;
using godot_autopilot::PluginConfig;

TEST(PluginConfigWriteTest, ParseFailureRefusesToMerge) {
  mcp::JsonValue::Object doc;
  doc["port"] = mcp::JsonValue(static_cast<int64_t>(9527));
  doc["desensitize"] = mcp::JsonValue(true);
  std::string text = "SENTINEL";

  EXPECT_FALSE(PluginConfig::merge_value(ConfigReadStatus::kParseFailed, doc,
                                         "show_time", mcp::JsonValue(false),
                                         text));
  EXPECT_EQ(text, "SENTINEL");
}

TEST(PluginConfigWriteTest, MissingConfigMergesIntoEmptyDocument) {
  mcp::JsonValue::Object doc;
  std::string text;

  ASSERT_TRUE(PluginConfig::merge_value(ConfigReadStatus::kMissing, doc,
                                        "port",
                                        mcp::JsonValue(static_cast<int64_t>(1)),
                                        text));
  mcp::JsonValue parsed = mcp::JsonValue::Parse(text);
  ASSERT_TRUE(parsed.IsObject());
  ASSERT_TRUE(parsed.GetObject().find("port") != parsed.GetObject().end());
  EXPECT_EQ(parsed.GetObject().at("port").GetInt(), 1);
}

TEST(PluginConfigWriteTest, SuccessfulReadPreservesOtherKeys) {
  mcp::JsonValue::Object doc;
  doc["show_time"] = mcp::JsonValue(true);
  doc["desensitize"] = mcp::JsonValue(true);
  std::string text;

  ASSERT_TRUE(PluginConfig::merge_value(ConfigReadStatus::kOk, doc, "port",
                                        mcp::JsonValue(static_cast<int64_t>(2)),
                                        text));
  mcp::JsonValue parsed = mcp::JsonValue::Parse(text);
  ASSERT_TRUE(parsed.IsObject());
  const mcp::JsonValue::Object &out = parsed.GetObject();
  ASSERT_TRUE(out.find("port") != out.end());
  ASSERT_TRUE(out.find("show_time") != out.end());
  ASSERT_TRUE(out.find("desensitize") != out.end());
  EXPECT_EQ(out.at("port").GetInt(), 2);
  EXPECT_TRUE(out.at("show_time").GetBool());
  EXPECT_TRUE(out.at("desensitize").GetBool());
}

TEST(PluginConfigWriteTest, ExistingKeyValueIsReplaced) {
  mcp::JsonValue::Object doc;
  doc["port"] = mcp::JsonValue(static_cast<int64_t>(1));
  doc["show_time"] = mcp::JsonValue(false);
  std::string text;

  ASSERT_TRUE(PluginConfig::merge_value(ConfigReadStatus::kOk, doc, "port",
                                        mcp::JsonValue(static_cast<int64_t>(9)),
                                        text));
  mcp::JsonValue parsed = mcp::JsonValue::Parse(text);
  ASSERT_TRUE(parsed.IsObject());
  EXPECT_EQ(parsed.GetObject().at("port").GetInt(), 9);
  EXPECT_FALSE(parsed.GetObject().at("show_time").GetBool());
}

TEST(AtomicWriteTest, ReplacesTargetAndLeavesNoTempFile) {
  const std::filesystem::path base =
      gda_test::test_temp_root() / "gda_atomic_write_test";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  ASSERT_TRUE(std::filesystem::create_directories(base, ec));
  ASSERT_FALSE(ec);

  const std::filesystem::path target = base / "config.json";
  {
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    out << "OLD-CONTENT";
  }

  ASSERT_TRUE(godot_autopilot::write_file_atomically(target.string(),
                                                     "NEW-CONTENT"));
  std::ifstream in(target, std::ios::binary);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  EXPECT_EQ(content, "NEW-CONTENT");
  EXPECT_FALSE(std::filesystem::exists(base / "config.json.tmp"));

  std::filesystem::remove_all(base, ec);
}

TEST(AtomicWriteTest, CreatesNewTargetFile) {
  const std::filesystem::path base =
      gda_test::test_temp_root() / "gda_atomic_create_test";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  ASSERT_TRUE(std::filesystem::create_directories(base, ec));
  ASSERT_FALSE(ec);

  const std::filesystem::path target = base / "index.json";
  ASSERT_TRUE(godot_autopilot::write_file_atomically(target.string(), "{}"));
  EXPECT_TRUE(std::filesystem::exists(target));
  EXPECT_FALSE(std::filesystem::exists(base / "index.json.tmp"));

  std::filesystem::remove_all(base, ec);
}

TEST(AtomicWriteTest, EmptyPathIsRejected) {
  EXPECT_FALSE(godot_autopilot::write_file_atomically("", "x"));
}

TEST(AtomicWriteTest, WritesBinaryContentWithEmbeddedNull) {
  const std::filesystem::path base =
      gda_test::test_temp_root() / "gda_atomic_binary_test";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  ASSERT_TRUE(std::filesystem::create_directories(base, ec));
  ASSERT_FALSE(ec);

  const std::filesystem::path target = base / "blob.bin";
  const std::string content("a\0b", 3);
  ASSERT_TRUE(godot_autopilot::write_file_atomically(target.string(), content));
  std::ifstream in(target, std::ios::binary);
  std::string read((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  EXPECT_EQ(read.size(), 3u);
  EXPECT_EQ(read, content);

  std::filesystem::remove_all(base, ec);
}
