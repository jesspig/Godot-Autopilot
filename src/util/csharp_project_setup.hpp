#ifndef GODOT_AUTOPILOT_TOOLS_CSHARP_PROJECT_SETUP_HPP
#define GODOT_AUTOPILOT_TOOLS_CSHARP_PROJECT_SETUP_HPP

#include <mcp/JsonValue.hpp>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/project_settings.hpp>

#include <cctype>
#include <string>

#include "util/error_util.hpp"

namespace godot_autopilot {
namespace csharp_setup {

inline std::string engine_version_short() {
  auto *engine = godot::Engine::get_singleton();
  if (engine == nullptr)
    return "";
  const godot::Dictionary info = engine->get_version_info();
  if (!info.has("string"))
    return "";
  const std::string full =
      util::to_std(godot::String(info["string"]));
  const size_t dash = full.find('-');
  return dash == std::string::npos ? full : full.substr(0, dash);
}

inline std::string project_name() {
  auto *settings = godot::ProjectSettings::get_singleton();
  if (settings == nullptr)
    return "Game";
  if (!settings->has_setting("application/config/name"))
    return "Game";
  const std::string name =
      util::to_std(godot::String(settings->get_setting("application/config/name")));
  return name.empty() ? "Game" : name;
}

inline std::string assembly_name() {
  std::string out;
  for (const char raw : project_name()) {
    const unsigned char c = static_cast<unsigned char>(raw);
    if (std::isalnum(c) != 0 || c == '_') {
      out.push_back(static_cast<char>(c));
      continue;
    }
    if (!out.empty() && out.back() != '_')
      out.push_back('_');
  }
  while (!out.empty() && out.back() == '_')
    out.pop_back();
  if (out.empty())
    return "Game";
  if (std::isdigit(static_cast<unsigned char>(out[0])) != 0)
    out = "Game_" + out;
  return out;
}

inline std::string target_framework(const std::string &dotnet_version) {
  const size_t dot = dotnet_version.find('.');
  const std::string major =
      dot == std::string::npos ? dotnet_version : dotnet_version.substr(0, dot);
  if (major.empty())
    return "";
  for (const char c : major) {
    if (std::isdigit(static_cast<unsigned char>(c)) == 0)
      return "";
  }
  return "net" + major + ".0";
}

inline std::string project_file_name() { return project_name() + ".csproj"; }

inline std::string project_template(const std::string &target_framework_value) {
  const std::string sdk = engine_version_short();
  const std::string assembly = assembly_name();
  std::string out = "<Project Sdk=\"Godot.NET.Sdk/" + sdk + "\">\n";
  out += "  <PropertyGroup>\n";
  out += "    <TargetFramework>" + target_framework_value +
         "</TargetFramework>\n";
  out += "    <EnableDynamicLoading>true</EnableDynamicLoading>\n";
  out += "    <AssemblyName>" + assembly + "</AssemblyName>\n";
  out += "    <RootNamespace>" + assembly + "</RootNamespace>\n";
  out += "  </PropertyGroup>\n";
  out += "</Project>\n";
  return out;
}

inline mcp::JsonValue missing_project_error(const std::string &dotnet_version) {
  const std::string assembly = assembly_name();
  const std::string tfm = target_framework(dotnet_version);
  mcp::JsonValue error(mcp::JsonValue::object_tag);
  const std::string project_file = "res://" + project_file_name();
  error["error"] = mcp::JsonValue(
      "当前项目不是 C# 工程，未发现 res:// 根 .csproj/.sln");
  error["hint"] = mcp::JsonValue(
      "用 write_file 创建 " + project_file +
      "，并把 project.godot 的 [dotnet] project/assembly_name 设为 \"" +
      assembly + "\"；随后重试 dotnet_environment，再执行 create_csharp_script"
      " 与 build_csharp_assembly。");
  mcp::JsonValue template_info(mcp::JsonValue::object_tag);
  template_info["path"] = mcp::JsonValue(project_file);
  template_info["content"] =
      mcp::JsonValue(project_template(tfm.empty() ? "net8.0" : tfm));
  template_info["target_framework"] =
      mcp::JsonValue(tfm.empty() ? "net8.0" : tfm);
  template_info["assembly_name"] = mcp::JsonValue(assembly);
  error["template"] = std::move(template_info);
  return error;
}

inline mcp::JsonValue missing_project_short_error() {
  mcp::JsonValue error(mcp::JsonValue::object_tag);
  error["error"] = mcp::JsonValue(
      "当前项目不是 C# 工程，未发现 res:// 根 .csproj/.sln");
  error["hint"] = mcp::JsonValue(
      "先运行 dotnet_environment：它会根据已安装的 .NET SDK 返回可直接写入的 "
      "csproj 模板（path/content/target_framework/assembly_name）。");
  return error;
}

} // namespace csharp_setup
} // namespace godot_autopilot

#endif
