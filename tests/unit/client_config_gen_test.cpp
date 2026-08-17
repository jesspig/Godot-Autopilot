#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include "util/client_config_gen.hpp"

#include <string>

namespace {

using namespace godot_autopilot::client_config_gen;

std::string entry_url(const std::string &json, const char *top_key) {
  mcp::JsonValue doc = mcp::JsonValue::Parse(json);
  const auto *servers = doc.Find(top_key);
  EXPECT_TRUE(servers != nullptr && servers->IsObject());
  const auto *entry = servers->Find(kServerName);
  EXPECT_TRUE(entry != nullptr && entry->IsObject());
  const auto *url = entry->Find("url");
  EXPECT_TRUE(url != nullptr && url->IsString());
  return url->GetString();
}

TEST(ClientConfigGenTest, FilePathsMatchClients) {
  EXPECT_STREQ(file_path(ClientId::OpenCode), "opencode.json");
  EXPECT_STREQ(file_path(ClientId::ClaudeCode), ".mcp.json");
  EXPECT_STREQ(file_path(ClientId::Codex), ".codex/config.toml");
  EXPECT_STREQ(file_path(ClientId::Cursor), ".cursor/mcp.json");
  EXPECT_STREQ(file_path(ClientId::Copilot), ".github/mcp.json");
  EXPECT_STREQ(file_path(ClientId::Trae), ".trae/mcp.json");
  EXPECT_STREQ(file_path(ClientId::Qoder), ".qoder/settings.json");
  EXPECT_STREQ(file_path(ClientId::WorkBuddy), ".workbuddy/mcp.json");
}

TEST(ClientConfigGenTest, RenderJsonUsesPortInUrl) {
  for (int id = 0; id < static_cast<int>(ClientId::COUNT); ++id) {
    if (static_cast<ClientId>(id) == ClientId::Codex) {
      continue;
    }
    const char *top_key = static_cast<ClientId>(id) == ClientId::OpenCode
                              ? "mcp"
                              : "mcpServers";
    std::string json = render_config(static_cast<ClientId>(id), 9527);
    EXPECT_EQ(entry_url(json, top_key), "http://127.0.0.1:9527/mcp");
    json = render_config(static_cast<ClientId>(id), 9530);
    EXPECT_EQ(entry_url(json, top_key), "http://127.0.0.1:9530/mcp");
  }
}

TEST(ClientConfigGenTest, RenderTypeFieldPerClient) {
  auto type_of = [](ClientId id) -> std::string {
    mcp::JsonValue doc = mcp::JsonValue::Parse(render_config(id, 9527));
    const char *top_key = id == ClientId::OpenCode ? "mcp" : "mcpServers";
    const auto *entry = doc.Find(top_key)->Find(kServerName);
    const auto *type = entry->Find("type");
    if (type == nullptr) {
      return "";
    }
    return type->GetString();
  };

  EXPECT_EQ(type_of(ClientId::OpenCode), "remote");
  EXPECT_EQ(type_of(ClientId::ClaudeCode), "http");
  EXPECT_EQ(type_of(ClientId::Cursor), "");
  EXPECT_EQ(type_of(ClientId::Copilot), "http");
  EXPECT_EQ(type_of(ClientId::Trae), "");
  EXPECT_EQ(type_of(ClientId::Qoder), "http");
  EXPECT_EQ(type_of(ClientId::WorkBuddy), "http");
}

TEST(ClientConfigGenTest, RenderOpenCodeEnablesServer) {
  mcp::JsonValue doc = mcp::JsonValue::Parse(render_config(ClientId::OpenCode, 9527));
  const auto *entry = doc.Find("mcp")->Find(kServerName);
  const auto *enabled = entry->Find("enabled");
  ASSERT_TRUE(enabled != nullptr && enabled->IsBool());
  EXPECT_TRUE(enabled->GetBool());
}

TEST(ClientConfigGenTest, RenderCodexToml) {
  std::string toml = render_config(ClientId::Codex, 9527);
  EXPECT_NE(toml.find("[mcp_servers.godot-autopilot]"), std::string::npos);
  EXPECT_NE(toml.find("url = \"http://127.0.0.1:9527/mcp\""),
            std::string::npos);
}

TEST(ClientConfigGenTest, MergeEmptyJsonMatchesRender) {
  for (int id = 0; id < static_cast<int>(ClientId::COUNT); ++id) {
    if (static_cast<ClientId>(id) == ClientId::Codex) {
      continue;
    }
    MergeResult result =
        merge_json_config(static_cast<ClientId>(id), 9527, "");
    EXPECT_EQ(result.status, MergeResult::Status::Merged);
    EXPECT_EQ(result.content, render_config(static_cast<ClientId>(id), 9527));
  }
}

TEST(ClientConfigGenTest, MergePreservesExistingKeys) {
  std::string existing =
      "{\"other\": {\"keep\": true}, \"mcp\": {\"old\": {\"type\": "
      "\"remote\", \"url\": \"http://127.0.0.1:9999/mcp\"}}}";
  MergeResult result = merge_json_config(ClientId::OpenCode, 9527, existing);
  ASSERT_EQ(result.status, MergeResult::Status::Merged);
  mcp::JsonValue doc = mcp::JsonValue::Parse(result.content);
  EXPECT_TRUE(doc.Find("other")->Find("keep")->GetBool());
  EXPECT_EQ(entry_url(result.content, "mcp"), "http://127.0.0.1:9527/mcp");
}

TEST(ClientConfigGenTest, MergeUpdatesServerUrl) {
  std::string existing =
      "{\"mcpServers\": {\"godot-autopilot\": {\"type\": \"http\", \"url\": "
      "\"http://127.0.0.1:9000/mcp\"}, \"other\": {}}}";
  MergeResult result =
      merge_json_config(ClientId::ClaudeCode, 9527, existing);
  ASSERT_EQ(result.status, MergeResult::Status::Merged);
  mcp::JsonValue doc = mcp::JsonValue::Parse(result.content);
  EXPECT_TRUE(doc.Find("mcpServers")->Contains("other"));
  EXPECT_EQ(entry_url(result.content, "mcpServers"),
            "http://127.0.0.1:9527/mcp");
}

TEST(ClientConfigGenTest, MergeRejectsInvalidJson) {
  MergeResult result =
      merge_json_config(ClientId::Cursor, 9527, "not json at all");
  EXPECT_EQ(result.status, MergeResult::Status::Unparsable);
  result = merge_json_config(ClientId::Trae, 9527, "[1, 2, 3]");
  EXPECT_EQ(result.status, MergeResult::Status::Unparsable);
}

TEST(ClientConfigGenTest, TomlMergeAppendsWhenAbsent) {
  TomlMergeResult result = merge_toml_config(9527, "");
  ASSERT_EQ(result.status, TomlMergeResult::Status::Merged);
  EXPECT_NE(result.content.find("url = \"http://127.0.0.1:9527/mcp\""),
            std::string::npos);

  result = merge_toml_config(9527, "model = \"gpt-5\"\n");
  ASSERT_EQ(result.status, TomlMergeResult::Status::Merged);
  EXPECT_NE(result.content.find("model = \"gpt-5\""), std::string::npos);
  EXPECT_NE(result.content.find("[mcp_servers.godot-autopilot]"),
            std::string::npos);
}

TEST(ClientConfigGenTest, TomlMergeSkipsWhenConfigured) {
  TomlMergeResult result =
      merge_toml_config(9527, "[mcp_servers.other]\nurl = \"x\"\n");
  EXPECT_EQ(result.status, TomlMergeResult::Status::AlreadyConfigured);
}

} // namespace