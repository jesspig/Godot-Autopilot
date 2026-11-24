#ifndef GODOT_AUTOPILOT_CSHARP_PROJECT_PROBE_HPP
#define GODOT_AUTOPILOT_CSHARP_PROJECT_PROBE_HPP

#include <cctype>
#include <string>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>

#include "util/dotnet_env.hpp"
#include "util/error_util.hpp"

namespace godot_autopilot {
namespace csharp_project_probe {

struct Result {
  bool engine_is_dotnet = false;
  bool project_is_csharp = false;
  bool addon_file_present = false;
  bool csproj_covers_addon = false;
  bool autoload_present = false;
  std::string project_file;
  std::string reason;
};

inline bool text_contains_ci(const std::string &text, const std::string &needle) {
  if (needle.empty())
    return true;
  if (text.size() < needle.size())
    return false;
  for (size_t i = 0; i + needle.size() <= text.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      const unsigned char a = static_cast<unsigned char>(text[i + j]);
      const unsigned char b = static_cast<unsigned char>(needle[j]);
      if (std::tolower(a) != std::tolower(b)) {
        match = false;
        break;
      }
    }
    if (match)
      return true;
  }
  return false;
}

inline std::string find_project_file() {
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open("res://");
  if (dir.is_null())
    return std::string();
  std::string sln;
  dir->list_dir_begin();
  godot::String entry = dir->get_next();
  while (entry != godot::String()) {
    if (!dir->current_is_dir()) {
      const std::string name = util::to_std(entry);
      if (name.size() >= 7 && name.substr(name.size() - 7) == ".csproj") {
        dir->list_dir_end();
        return "res://" + name;
      }
      if (sln.empty() && name.size() >= 4 && name.substr(name.size() - 4) == ".sln")
        sln = "res://" + name;
    }
    entry = dir->get_next();
  }
  dir->list_dir_end();
  return sln;
}

inline bool csproj_covers(const std::string &project_file,
                          const std::string &target_name) {
  if (project_file.empty())
    return false;
  if (project_file.size() < 7 ||
      project_file.substr(project_file.size() - 7) != ".csproj")
    return true;
  godot::Ref<godot::FileAccess> file = godot::FileAccess::open(
      godot::String::utf8(project_file.c_str()), godot::FileAccess::READ);
  if (file.is_null())
    return false;
  const std::string content = util::to_std(file->get_as_text());
  file->close();
  const bool sdk_style = text_contains_ci(content, "Sdk=");
  size_t marker = content.find("EnableDefaultCompileItems");
  if (marker == std::string::npos)
    marker = content.find("enabledefaultcompileitems");
  bool disables_glob = false;
  if (marker != std::string::npos) {
    const size_t hit = content.find("false", marker);
    disables_glob = hit != std::string::npos && hit < marker + 120;
  }
  if (sdk_style && !disables_glob)
    return true;
  return text_contains_ci(content, "Compile") &&
         text_contains_ci(content, target_name);
}

inline Result probe() {
  Result out;
  const dotnet_env::DotnetProbe env = dotnet_env::probe_current();
  out.engine_is_dotnet = dotnet_env::is_dotnet_environment(env);
  out.project_file = find_project_file();
  auto *settings = godot::ProjectSettings::get_singleton();
  bool has_assembly_setting = false;
  if (settings != nullptr)
    has_assembly_setting =
        settings->has_setting(godot::String("dotnet/project/assembly_name"));
  out.project_is_csharp = !out.project_file.empty() || has_assembly_setting;
  out.addon_file_present = godot::FileAccess::file_exists(godot::String(
      "res://addons/godot-autopilot/GdaCSharpRuntime.cs"));
  out.csproj_covers_addon =
      csproj_covers(out.project_file, "GdaCSharpRuntime.cs");
  if (settings != nullptr)
    out.autoload_present = settings->has_setting(
        godot::String("autoload/GdaCSharpRuntime"));
  if (!out.engine_is_dotnet)
    out.reason = "standard_engine";
  else if (!out.project_is_csharp)
    out.reason = "gdscript_project";
  else if (!out.addon_file_present)
    out.reason = "addon_missing";
  else if (!out.csproj_covers_addon)
    out.reason = "csharp_script_not_compiled";
  else if (out.autoload_present)
    out.reason = "already_configured";
  else
    out.reason = "needs_configuration";
  return out;
}

} // namespace csharp_project_probe
} // namespace godot_autopilot

#endif
