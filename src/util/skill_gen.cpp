#include "util/skill_gen.hpp"

#include <version.hpp>

namespace godot_autopilot::skill_gen {

namespace {

bool needs_yaml_quotes(const std::string &value) {
  if (value.empty()) {
    return true;
  }
  switch (value.front()) {
    case '-':
    case '?':
    case ':':
    case ',':
    case '[':
    case ']':
    case '{':
    case '}':
    case '#':
    case '&':
    case '*':
    case '!':
    case '|':
    case '>':
    case '\'':
    case '"':
    case '%':
    case '@':
    case '`':
    case ' ':
      return true;
    default:
      return value.find(": ") != std::string::npos;
  }
}

std::string yaml_double_quoted(const std::string &value) {
  std::string quoted = "\"";
  for (const char c : value) {
    if (c == '"' || c == '\\') {
      quoted.push_back('\\');
    }
    quoted.push_back(c);
  }
  quoted.push_back('"');
  return quoted;
}

} // namespace

// 册集合与聚合顺序由 skill_templates/registry.json 固化
std::vector<SkillSpec> all_skills() {
  return make_embedded_skills();
}

std::string skill_file_path(const std::string &name,
                            const std::string &relative_path) {
  return ".agents/skills/" + name + "/" + relative_path;
}

std::string render_skill_md(const SkillSpec &spec) {
  std::string md = "---\n";
  md += "name: " + spec.name + "\n";
  md += "description: " +
        (needs_yaml_quotes(spec.description)
             ? yaml_double_quoted(spec.description)
             : spec.description) +
        "\n";
  md += "metadata:\n";
  md += "  author: godot-autopilot\n";
  md += "  version: \"" + std::string(GDA_VERSION) + "\"\n";
  md += "---\n\n";
  if (!spec.files.empty()) {
    md += spec.files.front().body;
  }
  return md;
}

} // namespace godot_autopilot::skill_gen
