#ifndef GODOT_AUTOPILOT_SIDE_EFFECT_POLICY_HPP
#define GODOT_AUTOPILOT_SIDE_EFFECT_POLICY_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace godot_autopilot {
namespace side_effect_policy {

struct TierCriterion {
  const char *tier;
  const char *capabilities;
  const char *criterion;
};

inline constexpr TierCriterion kTierCriteria[] = {
    {"read", "read",
     "reads state only, without changing any external state"},
    {"write", "write",
     "changes external state in a recoverable way: writes files, project or "
     "editor config, windows and display, injected input, scene properties, "
     "saved resources"},
    {"destructive", "destructive",
     "irreversibly destroys an object or entity that already exists inside "
     "the host, with no durable recovery path"},
    {"danger", "process / code_execute / game_runtime",
     "runs external programs, executes scripts, or drives the game process"},
};

inline constexpr const char *kBoundaryDiskDeletion =
    "deleting a file or a directory on disk belongs to the write tier "
    "(project content usually has version control as a fallback)";

inline constexpr const char *kBoundaryHostObject =
    "destroying an object that lives inside the host belongs to the "
    "destructive tier";

inline constexpr const char *kDestructiveNameKeywords[] = {
    "delete", "destroy", "remove",  "erase",  "kill",   "terminat",
    "unregister", "dispose", "purge", "wipe", "truncat"};

inline constexpr const char *kDestructiveDescriptionPhrases[] = {
    "destroys", "destroyed", "freed at the end of", "cannot be undone",
    "irreversible", "unregistered", "kill "};

struct Exemption {
  const char *tool;
  const char *reason;
};

inline constexpr Exemption kDestructiveExemptions[] = {
    {"create_os_process",
     "only mentions kill_os_process in a cross reference; starting an "
     "external program is the danger tier"},
    {"erase_input_map_action",
     "clears a project setting; project content is versioned (write tier)"},
    {"erase_input_map_action_event",
     "clears a project setting; project content is versioned (write tier)"},
    {"kill_os_process",
     "terminates an external process, not an object that lives inside the "
     "host (danger tier)"},
    {"remove_autoload",
     "clears a project setting; project content is versioned (write tier)"},
    {"remove_group_node",
     "removes the group membership only; the node object itself is left "
     "untouched and the change is registered with the editor undo/redo"},
    {"remove_physics_3d_body_collision_exception",
     "removes a relation entry between two physics bodies; no object is "
     "destroyed"},
    {"remove_resource_file",
     "deletes a file on disk; project content is versioned (write tier)"},
    {"control_game_tween",
     "the tween lives in the game process, not in the host (game_runtime "
     "tier)"},
};

struct ToolDeclaration {
  std::string name;
  std::string description;
  bool destructive = false;
};

inline std::string to_lower(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    out.push_back((c >= 'A' && c <= 'Z')
                      ? static_cast<char>(c - 'A' + 'a')
                      : c);
  }
  return out;
}

inline bool contains_any(std::string_view text, const char *const *keywords,
                         std::size_t count) {
  const std::string haystack = to_lower(text);
  for (std::size_t i = 0; i < count; ++i) {
    if (haystack.find(keywords[i]) != std::string::npos)
      return true;
  }
  return false;
}

inline bool name_looks_destructive(std::string_view name) {
  return contains_any(name, kDestructiveNameKeywords,
                      sizeof(kDestructiveNameKeywords) /
                          sizeof(kDestructiveNameKeywords[0]));
}

inline bool description_looks_destructive(std::string_view description) {
  return contains_any(description, kDestructiveDescriptionPhrases,
                      sizeof(kDestructiveDescriptionPhrases) /
                          sizeof(kDestructiveDescriptionPhrases[0]));
}

inline bool looks_destructive(const ToolDeclaration &tool) {
  return name_looks_destructive(tool.name) ||
         description_looks_destructive(tool.description);
}

inline const Exemption *find_exemption(std::string_view tool_name) {
  for (const Exemption &entry : kDestructiveExemptions) {
    if (tool_name == entry.tool)
      return &entry;
  }
  return nullptr;
}

inline std::vector<std::string> destructive_suspects(
    const std::vector<ToolDeclaration> &tools) {
  std::vector<std::string> suspects;
  for (const ToolDeclaration &tool : tools) {
    if (tool.destructive)
      continue;
    if (!looks_destructive(tool))
      continue;
    if (find_exemption(tool.name) != nullptr)
      continue;
    suspects.push_back(tool.name);
  }
  return suspects;
}

} // namespace side_effect_policy
} // namespace godot_autopilot

#endif
