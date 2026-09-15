#include "client_config_gen.hpp"

#include <mcp/JsonValue.hpp>

#include <string>

namespace godot_autopilot::client_config_gen {

namespace {

std::string server_url(int port) {
  return "http://127.0.0.1:" + std::to_string(port) + "/mcp";
}

bool uses_type_field(ClientId id) {
  return id != ClientId::Cursor && id != ClientId::Trae &&
         id != ClientId::PiAgent && id != ClientId::KimiCode &&
         id != ClientId::Zed;
}

const char *type_value(ClientId id) {
  if (id == ClientId::OpenCode) {
    return "remote";
  }
  if (id == ClientId::Roo || id == ClientId::Kilo) {
    return "streamable-http";
  }
  return "http";
}

bool uses_enabled_field(ClientId id) {
  return id == ClientId::OpenCode || id == ClientId::ZCode ||
         id == ClientId::KimiCode;
}

// 顶层 server 映射所在键;ZCode 额外嵌套一层("mcp" -> "servers"),
// 在 build_top_object / merge_json_config 中单独处理。
const char *top_key(ClientId id) {
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

mcp::JsonValue build_server_entry(ClientId id, int port) {
  mcp::JsonValue::Object entry;
  if (uses_type_field(id)) {
    entry["type"] = mcp::JsonValue(type_value(id));
  }
  entry["url"] = mcp::JsonValue(server_url(port));
  if (uses_enabled_field(id)) {
    entry["enabled"] = mcp::JsonValue(true);
  }
  return mcp::JsonValue(std::move(entry));
}

mcp::JsonValue build_top_object(ClientId id, int port) {
  mcp::JsonValue::Object root;
  mcp::JsonValue::Object servers;
  servers[kServerName] = build_server_entry(id, port);
  if (id == ClientId::ZCode) {
    mcp::JsonValue::Object mcp;
    mcp["servers"] = mcp::JsonValue(std::move(servers));
    root["mcp"] = mcp::JsonValue(std::move(mcp));
    return root;
  }
  if (id == ClientId::Crush) {
    root["$schema"] = mcp::JsonValue("https://charm.land/crush.json");
  }
  root[top_key(id)] = mcp::JsonValue(std::move(servers));
  return root;
}

std::string render_json(ClientId id, int port) {
  return mcp::JsonValue(build_top_object(id, port)).Dump(2);
}

std::string render_toml(int port) {
  return "[mcp_servers." + std::string(kServerName) + "]\nurl = \"" +
         server_url(port) + "\"\n";
}

std::string render_reasonix_toml(int port) {
  return std::string("[[plugins]]\nname = \"") + kServerName +
         "\"\ntype = \"http\"\nurl = \"" + server_url(port) + "\"\n";
}

struct ClientInfo {
  ClientId id;
  const char *display_name;
  const char *file_path;
  const char *description;
};

constexpr ClientInfo kClients[] = {
    {ClientId::OpenCode, "OpenCode", "opencode.json", "opencode.json (mcp key)"},
    {ClientId::ClaudeCode, "Claude Code", ".mcp.json",
     ".mcp.json (mcpServers)"},
    {ClientId::Codex, "Codex", ".codex/config.toml",
     ".codex/config.toml (mcp_servers)"},
    {ClientId::Cursor, "Cursor", ".cursor/mcp.json",
     ".cursor/mcp.json (mcpServers)"},
    {ClientId::Copilot, "GitHub Copilot", ".github/mcp.json",
     ".github/mcp.json (mcpServers)"},
    {ClientId::Trae, "Trae", ".trae/mcp.json", ".trae/mcp.json (mcpServers)"},
    {ClientId::Qoder, "Qoder", ".qoder/settings.json",
     ".qoder/settings.json (mcpServers)"},
    {ClientId::WorkBuddy, "WorkBuddy", ".workbuddy/mcp.json",
     ".workbuddy/mcp.json (mcpServers)"},
    {ClientId::ZCode, "ZCode", ".zcode/config.json",
     ".zcode/config.json (mcp.servers)"},
    {ClientId::PiAgent, "pi (pi-mcp-adapter)", ".pi/mcp.json",
     ".pi/mcp.json (mcpServers)"},
    {ClientId::CommandCode, "Command Code", ".mcp.json",
     ".mcp.json (mcpServers, shared with Claude Code)"},
    {ClientId::Kilo, "Kilo Code", ".kilo/mcp.json",
     ".kilo/mcp.json (mcpServers)"},
    {ClientId::Roo, "Roo Code", ".roo/mcp.json", ".roo/mcp.json (mcpServers)"},
    {ClientId::GrokBuild, "Grok Build", ".grok/config.toml",
     ".grok/config.toml (mcp_servers)"},
    {ClientId::KimiCode, "Kimi Code", ".kimi-code/mcp.json",
     ".kimi-code/mcp.json (mcpServers)"},
    {ClientId::Zed, "Zed", ".zed/settings.json",
     ".zed/settings.json (context_servers)"},
    {ClientId::CodeBuddy, "CodeBuddy", ".mcp.json",
     ".mcp.json (mcpServers, shared with Claude Code)"},
    {ClientId::Crush, "Crush", ".crush.json", ".crush.json (mcp)"},
    {ClientId::CopilotVSCode, "GitHub Copilot (VS Code)", ".vscode/mcp.json",
     ".vscode/mcp.json (servers)"},
    {ClientId::Reasonix, "Reasonix", "reasonix.toml",
     "reasonix.toml ([[plugins]] array)"},
};

const ClientInfo *find_client(ClientId id) {
  for (const ClientInfo &info : kClients) {
    if (info.id == id) {
      return &info;
    }
  }
  return nullptr;
}

} // namespace

const char *display_name(ClientId id) {
  const ClientInfo *info = find_client(id);
  return info != nullptr ? info->display_name : "";
}

const char *file_path(ClientId id) {
  const ClientInfo *info = find_client(id);
  return info != nullptr ? info->file_path : "";
}

const char *description(ClientId id) {
  const ClientInfo *info = find_client(id);
  return info != nullptr ? info->description : "";
}

bool uses_toml(ClientId id) {
  return id == ClientId::Codex || id == ClientId::GrokBuild ||
         id == ClientId::Reasonix;
}

std::string render_config(ClientId id, int port) {
  if (id == ClientId::Reasonix) {
    return render_reasonix_toml(port);
  }
  if (uses_toml(id)) {
    return render_toml(port);
  }
  return render_json(id, port);
}

MergeResult merge_json_config(ClientId id, int port,
                              const std::string &existing) {
  if (existing.empty()) {
    return {MergeResult::Status::Merged, render_config(id, port)};
  }
  mcp::JsonValue doc;
  try {
    doc = mcp::JsonValue::Parse(existing);
  } catch (...) {
    return {MergeResult::Status::Unparsable, ""};
  }
  if (!doc.IsObject()) {
    return {MergeResult::Status::Unparsable, ""};
  }
  mcp::JsonValue &servers = [&]() -> mcp::JsonValue & {
    if (id != ClientId::ZCode) {
      return doc[top_key(id)];
    }
    mcp::JsonValue &mcp_obj = doc["mcp"];
    if (!mcp_obj.IsObject()) {
      mcp_obj = mcp::JsonValue(mcp::JsonValue::object_tag);
    }
    return mcp_obj["servers"];
  }();
  if (!servers.IsObject()) {
    servers = mcp::JsonValue(mcp::JsonValue::object_tag);
  }
  servers[kServerName] = build_server_entry(id, port);
  return {MergeResult::Status::Merged, doc.Dump(2)};
}

TomlMergeResult merge_toml_config(ClientId id, int port,
                                  const std::string &existing) {
  const bool reasonix = id == ClientId::Reasonix;
  const std::string marker =
      reasonix ? "name = \"" + std::string(kServerName) + "\""
               : std::string("[mcp_servers");
  if (existing.find(marker) != std::string::npos) {
    return {TomlMergeResult::Status::AlreadyConfigured, ""};
  }
  std::string content = existing;
  if (!content.empty() && content.back() != '\n') {
    content.push_back('\n');
  }
  content += reasonix ? render_reasonix_toml(port) : render_toml(port);
  return {TomlMergeResult::Status::Merged, content};
}

} // namespace godot_autopilot::client_config_gen
