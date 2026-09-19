#ifndef GODOT_AUTOPILOT_SKILL_GEN_HPP
#define GODOT_AUTOPILOT_SKILL_GEN_HPP

#include <string>
#include <vector>

namespace godot_autopilot::skill_gen {

struct SkillFile {
  std::string relative_path;
  std::string body;
};

struct SkillSpec {
  std::string name;
  std::string description;
  std::vector<SkillFile> files;
};

std::vector<SkillSpec> make_embedded_skills();

std::vector<SkillSpec> all_skills();

std::string skill_file_path(const std::string &name,
                            const std::string &relative_path);

std::string render_skill_md(const SkillSpec &spec);

} // namespace godot_autopilot::skill_gen

#endif
