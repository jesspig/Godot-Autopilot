#include "client_config_gen.hpp"

#include <mcp/JsonValue.hpp>

#include <string>

namespace godot_autopilot::client_config_gen {

namespace {

std::string server_url(int port) {
  return "http://127.0.0.1:" + std::to_string(port) + "/mcp";
}

bool uses_type_field(ClientId id) {
  return id != ClientId::Cursor && id != ClientId::Trae;
}

const char *type_value(ClientId id) {
  return id == ClientId::OpenCode ? "remote" : "http";
}

mcp::JsonValue build_server_entry(ClientId id, int port) {
  mcp::JsonValue::Object entry;
  if (uses_type_field(id)) {
    entry["type"] = mcp::JsonValue(type_value(id));
  }
  entry["url"] = mcp::JsonValue(server_url(port));
  if (id == ClientId::OpenCode) {
    entry["enabled"] = mcp::JsonValue(true);
  }
  return mcp::JsonValue(std::move(entry));
}

mcp::JsonValue::Object build_top_object(ClientId id, int port) {
  mcp::JsonValue::Object root;
  mcp::JsonValue::Object servers;
  servers[kServerName] = build_server_entry(id, port);
  root[id == ClientId::OpenCode ? "mcp" : "mcpServers"] =
      mcp::JsonValue(std::move(servers));
  return root;
}

std::string render_json(ClientId id, int port) {
  return mcp::JsonValue(build_top_object(id, port)).Dump(2);
}

std::string render_toml(int port) {
  return "[mcp_servers." + std::string(kServerName) + "]\nurl = \"" +
         server_url(port) + "\"\n";
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

std::string render_config(ClientId id, int port) {
  if (id == ClientId::Codex) {
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
  const char *top_key = id == ClientId::OpenCode ? "mcp" : "mcpServers";
  mcp::JsonValue &servers = doc[top_key];
  if (!servers.IsObject()) {
    servers = mcp::JsonValue(mcp::JsonValue::object_tag);
  }
  servers[kServerName] = build_server_entry(id, port);
  return {MergeResult::Status::Merged, doc.Dump(2)};
}

TomlMergeResult merge_toml_config(int port, const std::string &existing) {
  if (existing.find("[mcp_servers") != std::string::npos) {
    return {TomlMergeResult::Status::AlreadyConfigured, ""};
  }
  std::string content = existing;
  if (!content.empty() && content.back() != '\n') {
    content.push_back('\n');
  }
  content += render_toml(port);
  return {TomlMergeResult::Status::Merged, content};
}

} // namespace godot_autopilot::client_config_gen
