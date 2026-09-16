#ifndef GODOT_AUTOPILOT_SKILL_RESOURCES_HPP
#define GODOT_AUTOPILOT_SKILL_RESOURCES_HPP

#include "util/skill_gen.hpp"

#include <mcp/JsonValue.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace godot_autopilot::skill_resources {

enum class UriKind { invalid, catalog, skill, file };

struct ParsedUri {
  UriKind kind = UriKind::invalid;
  std::string name;
  std::string file;
};

struct CatalogEntry {
  std::string name;
  std::string description;
  std::vector<std::string> files;
};

struct ResolvedUri {
  ParsedUri parsed;
  const skill_gen::SkillSpec *skill = nullptr;
  const skill_gen::SkillFile *file = nullptr;
};

struct ContentResult {
  UriKind kind = UriKind::invalid;
  std::string mime_type;
  std::string text;
  std::string error;
};

inline char ascii_lower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

inline bool ascii_iequals(const std::string &lhs, const std::string &rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (ascii_lower(lhs[i]) != ascii_lower(rhs[i])) {
      return false;
    }
  }
  return true;
}

inline ParsedUri parse_uri(const std::string &uri) {
  static const std::string kPrefix = "godot://skills";
  ParsedUri parsed;
  if (uri.size() < kPrefix.size() ||
      !ascii_iequals(uri.substr(0, kPrefix.size()), kPrefix)) {
    return parsed;
  }
  const std::string rest = uri.substr(kPrefix.size());
  if (rest.empty()) {
    parsed.kind = UriKind::catalog;
    return parsed;
  }
  if (rest[0] != '/') {
    return parsed;
  }
  const std::string tail = rest.substr(1);
  if (tail.empty() || tail[0] == '/') {
    return parsed;
  }

  std::vector<std::string> segments;
  std::size_t start = 0;
  while (true) {
    const std::size_t slash = tail.find('/', start);
    if (slash == std::string::npos) {
      segments.push_back(tail.substr(start));
      break;
    }
    if (slash == start) {
      return ParsedUri{};
    }
    segments.push_back(tail.substr(start, slash - start));
    start = slash + 1;
  }
  if (segments.back().empty()) {
    return ParsedUri{};
  }

  parsed.name = segments.front();
  if (segments.size() == 1) {
    parsed.kind = UriKind::skill;
    return parsed;
  }

  parsed.kind = UriKind::file;
  for (std::size_t i = 1; i < segments.size(); ++i) {
    if (i > 1) {
      parsed.file.push_back('/');
    }
    parsed.file += segments[i];
  }
  return parsed;
}

inline std::vector<CatalogEntry> build_catalog(
    const std::vector<skill_gen::SkillSpec> &skills) {
  std::vector<CatalogEntry> catalog;
  catalog.reserve(skills.size());
  for (const skill_gen::SkillSpec &skill : skills) {
    CatalogEntry entry;
    entry.name = skill.name;
    entry.description = skill.description;
    entry.files.reserve(skill.files.size());
    for (const skill_gen::SkillFile &file : skill.files) {
      entry.files.push_back(file.relative_path);
    }
    catalog.push_back(std::move(entry));
  }
  return catalog;
}

inline std::string catalog_to_json(const std::vector<CatalogEntry> &catalog) {
  mcp::JsonValue items(mcp::JsonValue::array_tag);
  for (const CatalogEntry &entry : catalog) {
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["name"] = mcp::JsonValue(entry.name);
    item["description"] = mcp::JsonValue(entry.description);
    mcp::JsonValue files(mcp::JsonValue::array_tag);
    for (const std::string &file : entry.files) {
      files.PushBack(mcp::JsonValue(file));
    }
    item["files"] = std::move(files);
    items.PushBack(std::move(item));
  }

  mcp::JsonValue root(mcp::JsonValue::object_tag);
  root["count"] = mcp::JsonValue(static_cast<std::int64_t>(catalog.size()));
  root["skills"] = std::move(items);
  return root.Dump(-1);
}

inline const skill_gen::SkillSpec *find_skill(
    const std::vector<skill_gen::SkillSpec> &skills, const std::string &name) {
  for (const skill_gen::SkillSpec &skill : skills) {
    if (skill.name == name) {
      return &skill;
    }
  }
  return nullptr;
}

inline const skill_gen::SkillFile *find_file(const skill_gen::SkillSpec &skill,
                                             const std::string &relative_path) {
  for (const skill_gen::SkillFile &file : skill.files) {
    if (file.relative_path == relative_path) {
      return &file;
    }
  }
  return nullptr;
}

inline ResolvedUri resolve_uri(const std::vector<skill_gen::SkillSpec> &skills,
                               const std::string &uri) {
  ResolvedUri resolved;
  resolved.parsed = parse_uri(uri);
  if (resolved.parsed.kind != UriKind::skill &&
      resolved.parsed.kind != UriKind::file) {
    return resolved;
  }
  resolved.skill = find_skill(skills, resolved.parsed.name);
  if (resolved.skill == nullptr || resolved.parsed.kind == UriKind::skill) {
    return resolved;
  }
  resolved.file = find_file(*resolved.skill, resolved.parsed.file);
  return resolved;
}

inline ContentResult resolve_content(
    const std::vector<skill_gen::SkillSpec> &skills, const std::string &uri) {
  ContentResult out;
  const ResolvedUri resolved = resolve_uri(skills, uri);
  out.kind = resolved.parsed.kind;
  switch (resolved.parsed.kind) {
  case UriKind::catalog:
    out.mime_type = "application/json";
    out.text = catalog_to_json(build_catalog(skills));
    return out;
  case UriKind::skill:
    if (resolved.skill == nullptr) {
      out.error = "Skill not found: " + resolved.parsed.name;
      return out;
    }
    out.mime_type = "text/markdown";
    out.text = skill_gen::render_skill_md(*resolved.skill);
    return out;
  case UriKind::file:
    if (resolved.skill == nullptr) {
      out.error = "Skill not found: " + resolved.parsed.name;
      return out;
    }
    if (resolved.file == nullptr) {
      out.error = "Skill file not found: " + resolved.parsed.name + "/" +
                  resolved.parsed.file;
      return out;
    }
    out.mime_type = "text/markdown";
    out.text = resolved.file->relative_path == "SKILL.md"
                   ? skill_gen::render_skill_md(*resolved.skill)
                   : resolved.file->body;
    return out;
  case UriKind::invalid:
    break;
  }
  out.error = "Unknown skill resource URI: " + uri;
  return out;
}

} // namespace godot_autopilot::skill_resources

#endif
