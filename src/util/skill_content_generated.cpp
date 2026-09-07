#include "util/skill_gen.hpp"

#include <skill_content_embedded.h>

namespace godot_autopilot::skill_gen {

std::vector<SkillSpec> make_embedded_skills() {
  std::vector<SkillSpec> skills;
  skills.reserve(embedded::all().size());
  for (const auto &entry : embedded::all()) {
    SkillSpec spec;
    spec.name = entry.name;
    spec.description = entry.description;
    spec.files.reserve(entry.files.size());
    for (const auto &file : entry.files) {
      spec.files.push_back({file.path, file.body});
    }
    skills.push_back(std::move(spec));
  }
  return skills;
}

} // namespace godot_autopilot::skill_gen
