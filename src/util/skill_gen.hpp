#ifndef GODOT_AUTOPILOT_SKILL_GEN_HPP
#define GODOT_AUTOPILOT_SKILL_GEN_HPP

#include <string>
#include <vector>

namespace godot_autopilot::skill_gen {

struct SkillFile {
  std::string relative_path; // "SKILL.md" 或 "references/XXX.md"
  std::string body;
};

struct SkillSpec {
  std::string name;          // 必须与目标目录名一致
  std::string description;   // 英文，1-1024 字符
  std::vector<SkillFile> files; // files[0] 必须是 SKILL.md
};

std::vector<SkillSpec> make_embedded_skills(); // 实现在 skill_content_generated.cpp；内容由 skill_templates/ 构建期嵌入

std::vector<SkillSpec> all_skills();

// 项目根相对路径：" .agents/skills/<name>/<relative_path>"（无前导空格）
std::string skill_file_path(const std::string &name,
                            const std::string &relative_path);

// 完整 SKILL.md：YAML frontmatter + 正文；metadata.version 注入 GDA_VERSION
std::string render_skill_md(const SkillSpec &spec);

} // namespace godot_autopilot::skill_gen

#endif
