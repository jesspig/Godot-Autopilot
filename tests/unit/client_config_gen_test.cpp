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

// 测试本地副本:客户端 -> 顶层 server 映射键(与实现保持一致)。
const char *expected_top_key(ClientId id) {
  if (id == ClientId::OpenCode || id == ClientId::Crush) {
    return "mcp";
  }
  if (id == ClientId::CopilotVSCode) {
    return "servers";
  }
  if (id == ClientId::Zed) {
    return "context_servers";
  }
  return "mcpServers";
}

// 注意:返回指针借用 doc,调用方需保证 doc 生命周期覆盖使用区间。
const mcp::JsonValue *find_entry_in(const mcp::JsonValue &doc, ClientId id) {
  const mcp::JsonValue *servers = nullptr;
  if (id == ClientId::ZCode) {
    const auto *mcp = doc.Find("mcp");
    servers = mcp != nullptr ? mcp->Find("servers") : nullptr;
  } else {
    servers = doc.Find(expected_top_key(id));
  }
  return servers != nullptr ? servers->Find(kServerName) : nullptr;
}

mcp::JsonValue parse_rendered(ClientId id, int port) {
  return mcp::JsonValue::Parse(render_config(id, port));
}

std::string entry_type(ClientId id) {
  mcp::JsonValue doc = parse_rendered(id, 9527);
  const auto *entry = find_entry_in(doc, id);
  EXPECT_TRUE(entry != nullptr && entry->IsObject());
  const auto *type = entry != nullptr ? entry->Find("type") : nullptr;
  return type != nullptr ? type->GetString() : "";
}

bool entry_has_enabled(ClientId id) {
  mcp::JsonValue doc = parse_rendered(id, 9527);
  const auto *entry = find_entry_in(doc, id);
  EXPECT_TRUE(entry != nullptr && entry->IsObject());
  return entry != nullptr && entry->Find("enabled") != nullptr;
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
  EXPECT_STREQ(file_path(ClientId::ZCode), ".zcode/config.json");
  EXPECT_STREQ(file_path(ClientId::PiAgent), ".mcp.json");
  EXPECT_STREQ(file_path(ClientId::CommandCode), ".mcp.json");
  EXPECT_STREQ(file_path(ClientId::Kilo), ".kilo/mcp.json");
  EXPECT_STREQ(file_path(ClientId::Roo), ".roo/mcp.json");
  EXPECT_STREQ(file_path(ClientId::GrokBuild), ".grok/config.toml");
  EXPECT_STREQ(file_path(ClientId::KimiCode), ".kimi-code/mcp.json");
  EXPECT_STREQ(file_path(ClientId::Zed), ".zed/settings.json");
  EXPECT_STREQ(file_path(ClientId::CodeBuddy), ".mcp.json");
  EXPECT_STREQ(file_path(ClientId::Crush), ".crush.json");
  EXPECT_STREQ(file_path(ClientId::CopilotVSCode), ".vscode/mcp.json");
  EXPECT_STREQ(file_path(ClientId::Reasonix), "reasonix.toml");
}

TEST(ClientConfigGenTest, RenderJsonUsesPortInUrl) {
  for (int i = 0; i < static_cast<int>(ClientId::COUNT); ++i) {
    auto id = static_cast<ClientId>(i);
    if (uses_toml(id)) {
      continue;
    }
    mcp::JsonValue doc = parse_rendered(id, 9527);
    const auto *entry = find_entry_in(doc, id);
    ASSERT_TRUE(entry != nullptr && entry->IsObject());
    const auto *url = entry->Find("url");
    ASSERT_TRUE(url != nullptr && url->IsString());
    EXPECT_EQ(url->GetString(), "http://127.0.0.1:9527/mcp");

    mcp::JsonValue doc2 = parse_rendered(id, 9530);
    entry = find_entry_in(doc2, id);
    ASSERT_TRUE(entry != nullptr);
    EXPECT_EQ(entry->Find("url")->GetString(),
              "http://127.0.0.1:9530/mcp");
  }
}

TEST(ClientConfigGenTest, RenderTypeFieldPerClient) {
  EXPECT_EQ(entry_type(ClientId::OpenCode), "remote");
  EXPECT_EQ(entry_type(ClientId::ClaudeCode), "http");
  EXPECT_EQ(entry_type(ClientId::Cursor), "");
  EXPECT_EQ(entry_type(ClientId::Copilot), "http");
  EXPECT_EQ(entry_type(ClientId::Trae), "");
  EXPECT_EQ(entry_type(ClientId::Qoder), "http");
  EXPECT_EQ(entry_type(ClientId::WorkBuddy), "http");
  EXPECT_EQ(entry_type(ClientId::ZCode), "http");
  EXPECT_EQ(entry_type(ClientId::PiAgent), "");
  EXPECT_EQ(entry_type(ClientId::CommandCode), "http");
  EXPECT_EQ(entry_type(ClientId::Kilo), "streamable-http");
  EXPECT_EQ(entry_type(ClientId::Roo), "streamable-http");
  EXPECT_EQ(entry_type(ClientId::KimiCode), "");
  EXPECT_EQ(entry_type(ClientId::Zed), "");
  EXPECT_EQ(entry_type(ClientId::CodeBuddy), "http");
  EXPECT_EQ(entry_type(ClientId::Crush), "http");
  EXPECT_EQ(entry_type(ClientId::CopilotVSCode), "http");
}

TEST(ClientConfigGenTest, RenderEnabledFieldPerClient) {
  EXPECT_TRUE(entry_has_enabled(ClientId::OpenCode));
  EXPECT_TRUE(entry_has_enabled(ClientId::ZCode));
  EXPECT_TRUE(entry_has_enabled(ClientId::KimiCode));
  EXPECT_FALSE(entry_has_enabled(ClientId::ClaudeCode));
  EXPECT_FALSE(entry_has_enabled(ClientId::Crush));
  EXPECT_FALSE(entry_has_enabled(ClientId::Zed));
}

TEST(ClientConfigGenTest, RenderOpenCodeEnablesServer) {
  mcp::JsonValue doc = mcp::JsonValue::Parse(render_config(ClientId::OpenCode, 9527));
  const auto *entry = doc.Find("mcp")->Find(kServerName);
  const auto *enabled = entry->Find("enabled");
  ASSERT_TRUE(enabled != nullptr && enabled->IsBool());
  EXPECT_TRUE(enabled->GetBool());
}

TEST(ClientConfigGenTest, RenderZcodeNestsUnderMcpServers) {
  mcp::JsonValue doc =
      mcp::JsonValue::Parse(render_config(ClientId::ZCode, 9527));
  const auto *mcp = doc.Find("mcp");
  ASSERT_TRUE(mcp != nullptr && mcp->IsObject());
  const auto *servers = mcp->Find("servers");
  ASSERT_TRUE(servers != nullptr && servers->IsObject());
  const auto *entry = servers->Find(kServerName);
  ASSERT_TRUE(entry != nullptr && entry->IsObject());
  EXPECT_EQ(entry->Find("url")->GetString(),
            "http://127.0.0.1:9527/mcp");
}

TEST(ClientConfigGenTest, RenderCrushCarriesSchema) {
  mcp::JsonValue doc =
      mcp::JsonValue::Parse(render_config(ClientId::Crush, 9527));
  const auto *schema = doc.Find("$schema");
  ASSERT_TRUE(schema != nullptr && schema->IsString());
  EXPECT_EQ(schema->GetString(), "https://charm.land/crush.json");
  EXPECT_TRUE(doc.Find("mcp") != nullptr);
}

TEST(ClientConfigGenTest, SharedMcpJsonRendersIdentically) {
  std::string claude = render_config(ClientId::ClaudeCode, 9527);
  EXPECT_EQ(render_config(ClientId::CodeBuddy, 9527), claude);
  EXPECT_EQ(render_config(ClientId::CommandCode, 9527), claude);
  MergeResult merged =
      merge_json_config(ClientId::CodeBuddy, 9530, claude);
  ASSERT_EQ(merged.status, MergeResult::Status::Merged);
  EXPECT_EQ(merged.content, render_config(ClientId::CodeBuddy, 9530));
}

TEST(ClientConfigGenTest, RenderCodexToml) {
  std::string toml = render_config(ClientId::Codex, 9527);
  EXPECT_NE(toml.find("[mcp_servers.godot-autopilot]"), std::string::npos);
  EXPECT_NE(toml.find("url = \"http://127.0.0.1:9527/mcp\""),
            std::string::npos);
}

TEST(ClientConfigGenTest, RenderGrokTomlMatchesCodexShape) {
  EXPECT_EQ(render_config(ClientId::GrokBuild, 9527),
            render_config(ClientId::Codex, 9527));
}

TEST(ClientConfigGenTest, RenderReasonixTomlPluginsArray) {
  std::string toml = render_config(ClientId::Reasonix, 9527);
  EXPECT_NE(toml.find("[[plugins]]"), std::string::npos);
  EXPECT_NE(toml.find("name = \"godot-autopilot\""), std::string::npos);
  EXPECT_NE(toml.find("type = \"http\""), std::string::npos);
  EXPECT_NE(toml.find("url = \"http://127.0.0.1:9527/mcp\""),
            std::string::npos);
}

TEST(ClientConfigGenTest, MergeEmptyJsonMatchesRender) {
  for (int i = 0; i < static_cast<int>(ClientId::COUNT); ++i) {
    auto id = static_cast<ClientId>(i);
    if (uses_toml(id)) {
      continue;
    }
    MergeResult result = merge_json_config(id, 9527, "");
    EXPECT_EQ(result.status, MergeResult::Status::Merged);
    EXPECT_EQ(result.content, render_config(id, 9527));
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

TEST(ClientConfigGenTest, MergeZcodePreservesExistingMcp) {
  std::string existing =
      "{\"mcp\": {\"other\": {\"command\": \"foo\"}}, \"auth\": true}";
  MergeResult result = merge_json_config(ClientId::ZCode, 9527, existing);
  ASSERT_EQ(result.status, MergeResult::Status::Merged);
  mcp::JsonValue doc = mcp::JsonValue::Parse(result.content);
  EXPECT_TRUE(doc.Find("auth")->GetBool());
  const auto *mcp = doc.Find("mcp");
  ASSERT_TRUE(mcp != nullptr);
  EXPECT_TRUE(mcp->Find("other") != nullptr);
  const auto *servers = mcp->Find("servers");
  ASSERT_TRUE(servers != nullptr);
  EXPECT_TRUE(servers->Find(kServerName) != nullptr);
}

TEST(ClientConfigGenTest, MergeVscodeUsesServersKey) {
  std::string existing =
      "{\"servers\": {\"other\": {\"type\": \"http\", \"url\": \"x\"}}}";
  MergeResult result =
      merge_json_config(ClientId::CopilotVSCode, 9527, existing);
  ASSERT_EQ(result.status, MergeResult::Status::Merged);
  mcp::JsonValue doc = mcp::JsonValue::Parse(result.content);
  const auto *servers = doc.Find("servers");
  ASSERT_TRUE(servers != nullptr);
  EXPECT_TRUE(servers->Find("other") != nullptr);
  EXPECT_TRUE(servers->Find(kServerName) != nullptr);
  EXPECT_TRUE(doc.Find("mcpServers") == nullptr);
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
  result = merge_json_config(ClientId::ZCode, 9527, "[1]");
  EXPECT_EQ(result.status, MergeResult::Status::Unparsable);
}

TEST(ClientConfigGenTest, TomlMergeAppendsWhenAbsent) {
  TomlMergeResult result =
      merge_toml_config(ClientId::Codex, 9527, "");
  ASSERT_EQ(result.status, TomlMergeResult::Status::Merged);
  EXPECT_NE(result.content.find("url = \"http://127.0.0.1:9527/mcp\""),
            std::string::npos);

  result = merge_toml_config(ClientId::Codex, 9527, "model = \"gpt-5\"\n");
  ASSERT_EQ(result.status, TomlMergeResult::Status::Merged);
  EXPECT_NE(result.content.find("model = \"gpt-5\""), std::string::npos);
  EXPECT_NE(result.content.find("[mcp_servers.godot-autopilot]"),
            std::string::npos);
}

TEST(ClientConfigGenTest, TomlMergeSkipsWhenConfigured) {
  TomlMergeResult result = merge_toml_config(
      ClientId::Codex, 9527, "[mcp_servers.other]\nurl = \"x\"\n");
  EXPECT_EQ(result.status, TomlMergeResult::Status::AlreadyConfigured);
  result = merge_toml_config(ClientId::GrokBuild, 9527,
                             "[mcp_servers.other]\nurl = \"x\"\n");
  EXPECT_EQ(result.status, TomlMergeResult::Status::AlreadyConfigured);
}

TEST(ClientConfigGenTest, ReasonixTomlMergeAppendsAndDetects) {
  TomlMergeResult result = merge_toml_config(ClientId::Reasonix, 9527, "");
  ASSERT_EQ(result.status, TomlMergeResult::Status::Merged);
  EXPECT_NE(result.content.find("[[plugins]]"), std::string::npos);

  std::string existing = "model = \"deepseek\"\n";
  result = merge_toml_config(ClientId::Reasonix, 9527, existing);
  ASSERT_EQ(result.status, TomlMergeResult::Status::Merged);
  EXPECT_NE(result.content.find("model = \"deepseek\""), std::string::npos);
  EXPECT_NE(result.content.find("[[plugins]]"), std::string::npos);

  result = merge_toml_config(ClientId::Reasonix, 9527, result.content);
  EXPECT_EQ(result.status, TomlMergeResult::Status::AlreadyConfigured);
}

} // namespace
