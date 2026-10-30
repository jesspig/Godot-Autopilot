#include "core/instance_registry.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/log_system.hpp"
#include "core/path_util.hpp"
#include "core/project_store.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kRegistryProjectsDirName = "projects";
constexpr const char *kRegistryInstancesDirName = "instances";
constexpr const char *kRegistryJsonSuffix = ".json";

std::string registry_instance_file_name(const std::string &instance_id) {
  return sanitize_path_component(instance_id) + kRegistryJsonSuffix;
}

bool write_registry_file(const std::string &path, const std::string &content) {
  std::ofstream out(to_fs_path(path), std::ios::binary | std::ios::trunc);
  if (!out) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                               "Instance registry write failed: " + path);
    return false;
  }
  out << content;
  out.close();
  if (!out) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                               "Instance registry write failed: " + path);
    return false;
  }
  return true;
}

bool read_registry_file(const std::string &path, std::string &content) {
  std::ifstream in(to_fs_path(path), std::ios::binary);
  if (!in) {
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  content = buffer.str();
  return true;
}

bool read_registry_endpoint(const mcp::JsonValue &doc, EndpointEntry &out) {
  const mcp::JsonValue *host = doc.Find("host");
  const mcp::JsonValue *port = doc.Find("port");
  const mcp::JsonValue *path = doc.Find("path");
  const mcp::JsonValue *protocol = doc.Find("protocol");
  const mcp::JsonValue *auth = doc.Find("auth");
  if (host == nullptr || !host->IsString()) {
    return false;
  }
  if (port == nullptr || !port->IsInt()) {
    return false;
  }
  if (path == nullptr || !path->IsString()) {
    return false;
  }
  if (protocol == nullptr || !protocol->IsString()) {
    return false;
  }
  if (auth == nullptr || !auth->IsString()) {
    return false;
  }
  out.host = host->GetString();
  out.port = static_cast<int>(port->GetInt());
  out.path = path->GetString();
  out.protocol = protocol->GetString();
  out.auth = auth->GetString();
  return true;
}

mcp::JsonValue registry_endpoint_to_json(const EndpointEntry &entry) {
  mcp::JsonValue::Object doc;
  doc["host"] = entry.host;
  doc["port"] = static_cast<int64_t>(entry.port);
  doc["path"] = entry.path;
  doc["protocol"] = entry.protocol;
  doc["auth"] = entry.auth;
  return mcp::JsonValue(std::move(doc));
}

std::vector<std::string> registry_instance_file_paths(const std::string &root) {
  std::vector<std::string> paths;
  const std::string projects_dir = join_path(root, kRegistryProjectsDirName);
  std::error_code ec;
  std::filesystem::directory_iterator projects(to_fs_path(projects_dir), ec);
  if (ec) {
    return paths;
  }
  for (const std::filesystem::directory_entry &project : projects) {
    std::error_code project_ec;
    if (!project.is_directory(project_ec) || project_ec) {
      continue;
    }
    const std::string instances_dir =
        join_path(normalize_path_separators(project.path().u8string()),
                  kRegistryInstancesDirName);
    std::error_code instance_ec;
    std::filesystem::directory_iterator instances(to_fs_path(instances_dir),
                                                  instance_ec);
    if (instance_ec) {
      continue;
    }
    for (const std::filesystem::directory_entry &entry : instances) {
      std::error_code entry_ec;
      if (!entry.is_regular_file(entry_ec) || entry_ec) {
        continue;
      }
      const std::string path = entry.path().u8string();
      if (path.size() < 5 ||
          path.compare(path.size() - 5, 5, kRegistryJsonSuffix) != 0) {
        continue;
      }
      paths.push_back(path);
    }
  }
  return paths;
}

} // namespace

mcp::JsonValue record_to_json(const InstanceRecord &record) {
  mcp::JsonValue::Object endpoints;
  endpoints["mcp"] = registry_endpoint_to_json(record.endpoints.mcp);
  endpoints["api"] = registry_endpoint_to_json(record.endpoints.api);

  mcp::JsonValue::Object doc;
  doc["instance_id"] = record.instance_id;
  doc["port"] = static_cast<int64_t>(record.port);
  doc["pid"] = static_cast<int64_t>(record.pid);
  doc["project_path"] = record.project_path;
  doc["api_token"] = record.api_token;
  doc["endpoints"] = mcp::JsonValue(std::move(endpoints));
  doc["gda_home"] = record.gda_home;
  doc["started_at"] = static_cast<int64_t>(record.started_at);
  doc["version"] = record.version;
  return mcp::JsonValue(std::move(doc));
}

bool record_from_json(const mcp::JsonValue &doc, InstanceRecord &out) {
  if (!doc.IsObject()) {
    return false;
  }
  const mcp::JsonValue *instance_id = doc.Find("instance_id");
  const mcp::JsonValue *port = doc.Find("port");
  const mcp::JsonValue *pid = doc.Find("pid");
  const mcp::JsonValue *project_path = doc.Find("project_path");
  const mcp::JsonValue *api_token = doc.Find("api_token");
  const mcp::JsonValue *endpoints = doc.Find("endpoints");
  const mcp::JsonValue *gda_home = doc.Find("gda_home");
  const mcp::JsonValue *started_at = doc.Find("started_at");
  const mcp::JsonValue *version = doc.Find("version");
  if (instance_id == nullptr || !instance_id->IsString()) {
    return false;
  }
  if (port == nullptr || !port->IsInt()) {
    return false;
  }
  if (pid == nullptr || !pid->IsInt()) {
    return false;
  }
  if (project_path == nullptr || !project_path->IsString()) {
    return false;
  }
  if (api_token == nullptr || !api_token->IsString()) {
    return false;
  }
  if (endpoints == nullptr || !endpoints->IsObject()) {
    return false;
  }
  if (gda_home == nullptr || !gda_home->IsString()) {
    return false;
  }
  if (started_at == nullptr || !started_at->IsInt()) {
    return false;
  }
  if (version == nullptr || !version->IsString()) {
    return false;
  }
  const mcp::JsonValue *mcp_endpoint = endpoints->Find("mcp");
  const mcp::JsonValue *api_endpoint = endpoints->Find("api");
  if (mcp_endpoint == nullptr || !mcp_endpoint->IsObject()) {
    return false;
  }
  if (api_endpoint == nullptr || !api_endpoint->IsObject()) {
    return false;
  }

  InstanceRecord parsed;
  parsed.instance_id = instance_id->GetString();
  parsed.port = static_cast<int>(port->GetInt());
  parsed.pid = static_cast<int>(pid->GetInt());
  parsed.project_path = project_path->GetString();
  parsed.api_token = api_token->GetString();
  parsed.gda_home = gda_home->GetString();
  parsed.started_at = started_at->GetInt();
  parsed.version = version->GetString();
  if (!read_registry_endpoint(*mcp_endpoint, parsed.endpoints.mcp)) {
    return false;
  }
  if (!read_registry_endpoint(*api_endpoint, parsed.endpoints.api)) {
    return false;
  }
  out = std::move(parsed);
  return true;
}

bool record_from_json_text(const std::string &json_text, InstanceRecord &out) {
  mcp::JsonValue doc;
  try {
    doc = mcp::JsonValue::Parse(json_text);
  } catch (const std::exception &) {
    return false;
  } catch (...) {
    return false;
  }
  return record_from_json(doc, out);
}

bool InstanceRegistry::write(const InstanceRecord &record) {
  const std::string dir = ProjectStore::instance_dir(record.project_path);
  if (dir.empty()) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                               "Instance registry write rejected: GDA home "
                               "directory is unresolved");
    return false;
  }
  if (record.instance_id.empty()) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                               "Instance registry write rejected: empty "
                               "instance id");
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories(to_fs_path(dir), ec);
  if (ec) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::System,
        "Instance registry directory creation failed: " + dir);
    return false;
  }
  const std::string path =
      join_path(dir, registry_instance_file_name(record.instance_id));
  if (!write_registry_file(path, record_to_json(record).Dump(2))) {
    return false;
  }
  return update_global_index();
}

bool InstanceRegistry::remove(const std::string &instance_id) {
  if (instance_id.empty()) {
    return false;
  }
  const std::string root = ProjectStore::root_dir();
  if (root.empty()) {
    return false;
  }
  const std::string file_name = registry_instance_file_name(instance_id);
  bool removed = false;
  for (const std::string &path : registry_instance_file_paths(root)) {
    if (path_basename(path) != file_name) {
      continue;
    }
    std::error_code ec;
    if (std::filesystem::remove(to_fs_path(path), ec) && !ec) {
      removed = true;
    }
  }
  if (!removed) {
    return false;
  }
  return update_global_index();
}

std::vector<InstanceRecord> InstanceRegistry::list_all() {
  std::vector<InstanceRecord> records;
  const std::string root = ProjectStore::root_dir();
  if (root.empty()) {
    return records;
  }
  for (const std::string &path : registry_instance_file_paths(root)) {
    std::string content;
    if (!read_registry_file(path, content)) {
      LogSystem::instance().log(LogLevel::Warning, LogCategory::System,
                                 "Instance registry entry unreadable: " + path);
      continue;
    }
    InstanceRecord record;
    if (!record_from_json_text(content, record)) {
      LogSystem::instance().log(LogLevel::Warning, LogCategory::System,
                                 "Instance registry entry malformed: " + path);
      continue;
    }
    records.push_back(std::move(record));
  }
  return records;
}

bool InstanceRegistry::update_global_index() {
  const std::string path = ProjectStore::instances_index_path();
  if (path.empty()) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                               "Instance registry index rejected: GDA home "
                               "directory is unresolved");
    return false;
  }
  const std::vector<InstanceRecord> records = list_all();
  mcp::JsonValue::Array items;
  for (const InstanceRecord &record : records) {
    items.push_back(record_to_json(record));
  }
  mcp::JsonValue::Object doc;
  doc["instances"] = mcp::JsonValue(std::move(items));
  return write_registry_file(path, mcp::JsonValue(std::move(doc)).Dump(2));
}

} // namespace godot_autopilot
