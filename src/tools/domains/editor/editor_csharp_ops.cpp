#include "tools/domains/editor/editor_ops.hpp"
#include "runtime/gda_protocol.hpp"
#include "util/scene_path.hpp"
#include "util/scene_verify.hpp"
#include "core/config.hpp"
#include "core/editor_readiness.hpp"
#include "core/observability/log_system.hpp"
#include "core/observability/sanitize_policy.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "tools/base/dispatch.hpp"
#include "util/csharp_project_setup.hpp"
#include "tools/base/runtime_ops.hpp"
#include "util/dotnet_env.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_file_system_directory.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include "util/console_encoding.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace editor_ops {

namespace {

std::vector<std::string> collect_execute_output_lines(
    const godot::Array &output) {
  std::vector<std::string> lines;
  for (int64_t i = 0; i < output.size(); ++i) {
    godot::Variant element = output[i];
    if (element.get_type() == godot::Variant::PACKED_STRING_ARRAY) {
      godot::PackedStringArray packed = element;
      for (int64_t j = 0; j < packed.size(); ++j)
        lines.push_back(util::decode_console_output(util::to_std(packed[j])));
      continue;
    }
    if (element.get_type() != godot::Variant::STRING)
      continue;
    std::string blob =
        util::decode_console_output(util::to_std(godot::String(element)));
    size_t start = 0;
    while (start <= blob.size()) {
      size_t end = blob.find('\n', start);
      std::string line = end == std::string::npos
                             ? blob.substr(start)
                             : blob.substr(start, end - start);
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (!line.empty())
        lines.push_back(line);
      if (end == std::string::npos)
        break;
      start = end + 1;
    }
  }
  return lines;
}

constexpr const char *kDotnetUiLanguageEnvName = "DOTNET_CLI_UI_LANGUAGE";
constexpr const char *kDotnetUiLanguageEnglish = "en";

struct DotnetUiLanguageGuard {
  godot::OS *os = nullptr;
  godot::String variable;
  godot::String previous_value;

  explicit DotnetUiLanguageGuard(godot::OS *p_os)
      : os(p_os), variable(kDotnetUiLanguageEnvName),
        previous_value(os->get_environment(variable)) {
    os->set_environment(variable, godot::String(kDotnetUiLanguageEnglish));
  }

  DotnetUiLanguageGuard(const DotnetUiLanguageGuard &) = delete;
  DotnetUiLanguageGuard &operator=(const DotnetUiLanguageGuard &) = delete;

  ~DotnetUiLanguageGuard() {
    if (previous_value.is_empty())
      os->unset_environment(variable);
    else
      os->set_environment(variable, previous_value);
  }
};


mcp::JsonValue probe_editor_assembly(const std::string &probe_script) {
  mcp::JsonValue probe(mcp::JsonValue::object_tag);
  probe["path"] = mcp::JsonValue(probe_script);
  const dotnet_env::DotnetProbe env = dotnet_env::probe_current();
  const bool is_dotnet_editor = dotnet_env::is_dotnet_environment(env);
  probe["is_dotnet_editor"] = mcp::JsonValue(is_dotnet_editor);
  bool can_instantiate = false;
  std::string base_class;
  std::string state = "unavailable";
  std::string hint;
  if (!is_dotnet_editor) {
    hint = "the editor is not a .NET build, so C# scripts cannot load here; "
           "build and run the game with a .NET editor to use C# classes";
  } else {
    godot::ResourceLoader *loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
      hint = "ResourceLoader not available";
    } else {
      godot::Ref<godot::Resource> loaded = loader->load(
          godot::String::utf8(probe_script.c_str()), "Script",
          godot::ResourceLoader::CACHE_MODE_IGNORE);
      godot::Script *script =
          loaded.is_valid()
              ? godot::Object::cast_to<godot::Script>(loaded.ptr())
              : nullptr;
      if (!script) {
        hint = "the script resource did not load in the editor; re-run "
               "build_csharp_assembly and read get_debugger_log for C# errors";
      } else {
        can_instantiate = script->can_instantiate();
        base_class =
            util::to_std(godot::String(script->get_instance_base_type()));
        state = "available";
        if (!can_instantiate) {
          hint = "the loaded editor assembly does not expose this class yet; "
                 "click Build in the editor toolbar or restart the editor, "
                 "then probe again";
        }
      }
    }
  }
  probe["state"] = mcp::JsonValue(state);
  probe["can_instantiate"] = mcp::JsonValue(can_instantiate);
  probe["base_class"] = mcp::JsonValue(base_class);
  if (!hint.empty())
    probe["hint"] = mcp::JsonValue(hint);
  if (state == "unavailable")
    probe["unavailable"] = mcp::JsonValue(true);
  return probe;
}

} // namespace

mcp::JsonValue handle_build_csharp_assembly(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "build_csharp_assembly called");
  static const char *kNote =
      "本工具仅等效触发 C# 项目编译，不等同于在编辑器中点击 Build 后的程序集热重载。"
      "GDExtension(C++) 无公共 API 触发编辑器内的 C# 程序集重载——该能力位于引擎 "
      "modules/mono 内部，非 GDExtension 可调用。适合 CI / 命令行式编译验证；若需编辑器内"
      "类或签名变更后校验渲染，仍须用户在编辑器中点 Build 或重启编辑器。构建完成后返回 "
      "next_action 与 restart_required（success=true 即为 true，与游戏是否在运行无关）；"
      "传入 probe_script 会 load 该 .cs 并报告 editor_assembly_probe 的 can_instantiate，"
      "用于判断编辑器已加载程序集是否已更新。";
  bool wait = false;
  int64_t timeout_ms = GDA_MAX_GAME_OP_TIMEOUT_MS;
  std::string probe_script;
  if (args.IsObject()) {
    for (const auto &entry : args.GetObject()) {
      if (entry.first != "wait" && entry.first != "timeout_ms" &&
          entry.first != "probe_script") {
        return util::error_json(
            "unknown parameter for build_csharp_assembly: " + entry.first);
      }
    }
  }
  auto *wait_p = args.Find("wait");
  if (wait_p) {
    if (!wait_p->IsBool()) {
      return util::error_json("wait must be a boolean");
    }
    wait = wait_p->GetBool();
  }
  auto *timeout_p = args.Find("timeout_ms");
  if (timeout_p) {
    if (!wait) {
      return util::error_json("timeout_ms only applies when wait=true");
    }
    if (!timeout_p->IsInt() || timeout_p->GetInt() <= 0 ||
        timeout_p->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS) {
      return util::error_json("timeout_ms must be an integer between 1 and " +
                              std::to_string(GDA_MAX_GAME_OP_TIMEOUT_MS));
    }
    timeout_ms = timeout_p->GetInt();
  }
  (void)timeout_ms;
  auto *probe_p = args.Find("probe_script");
  if (probe_p) {
    if (!probe_p->IsString())
      return util::error_json("probe_script must be a string");
    probe_script = probe_p->GetString();
    const bool res_prefix = probe_script.rfind("res://", 0) == 0;
    const bool cs_suffix =
        probe_script.size() > 8 &&
        probe_script.compare(probe_script.size() - 3, 3, ".cs") == 0;
    if (!res_prefix || !cs_suffix)
      return util::error_json(
          "probe_script must be a res:// path ending in .cs, e.g. "
          "res://scripts/Player.cs");
  }
  std::string project_file;
  godot::Ref<godot::DirAccess> da = godot::DirAccess::open("res://");
  if (!da.is_null()) {
    da->list_dir_begin();
    godot::String entry = da->get_next();
    while (entry != godot::String()) {
      if (entry != "." && entry != ".." && !da->current_is_dir() &&
          (entry.ends_with(".csproj") || entry.ends_with(".sln"))) {
        project_file = "res://" + util::to_std(entry);
        break;
      }
      entry = da->get_next();
    }
    da->list_dir_end();
  }
  if (project_file.empty())
    return csharp_setup::missing_project_short_error();
  auto *os = godot::OS::get_singleton();
  if (!os)
    return util::error_json("OS singleton not available");
  auto *settings = godot::ProjectSettings::get_singleton();
  if (!settings)
    return util::error_json("ProjectSettings singleton not available");
  std::string project_path =
      util::to_std(settings->globalize_path(godot::String(project_file.c_str())));
  if (project_path.empty())
    return util::error_json("无法把工程文件路径换算为系统绝对路径: " +
                            project_file);
  std::string command = "dotnet build --nologo " + project_file;
  if (!wait) {
    godot::PackedStringArray argv;
    argv.append("build");
    argv.append("--nologo");
    argv.append(godot::String(project_path.c_str()));
    int32_t pid = os->create_process("dotnet", argv);
    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["project_file"] = mcp::JsonValue(project_file);
    inner["project_path"] = mcp::JsonValue(project_path);
    inner["command"] = mcp::JsonValue(command);
    inner["pid"] = mcp::JsonValue(static_cast<int64_t>(pid));
    inner["note"] = mcp::JsonValue(kNote);
    inner["started"] = mcp::JsonValue(pid > 0);
    const auto *editor = godot::EditorInterface::get_singleton();
    inner["game_running"] = mcp::JsonValue(
        editor != nullptr && editor->is_playing_scene());
    mcp::JsonValue next_action(mcp::JsonValue::array_tag);
    next_action.PushBack(mcp::JsonValue(
        "wait for the launch to finish, then call build_csharp_assembly again "
        "with wait=true to read exit_code, success and errors"));
    if (pid > 0) {
      next_action.PushBack(mcp::JsonValue(
          "after the build succeeds, restart the running game process and "
          "click Build in the editor toolbar or restart the editor — the "
          "loaded assemblies are not hot-reloaded"));
    }
    inner["next_action"] = std::move(next_action);
    inner["restart_required"] = mcp::JsonValue(pid > 0);
    if (!probe_script.empty())
      inner["editor_assembly_probe"] = probe_editor_assembly(probe_script);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(inner);
    if (pid <= 0) {
      r["error"] = mcp::JsonValue(
          "启动 dotnet 失败（SDK 缺失或不在 PATH？），请确认已安装 .NET SDK 且 "
          "dotnet 命令可用");
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "build_csharp_assembly completed");
    return r;
  }
  godot::PackedStringArray build_args;
  build_args.append("build");
  build_args.append("--nologo");
  build_args.append(godot::String(project_path.c_str()));
  godot::Array output;
  output.append(godot::PackedStringArray());
  int32_t exit_code = 0;
  {
    DotnetUiLanguageGuard english_ui_language(os);
    exit_code = os->execute(godot::String("dotnet"), build_args, output, true);
  }
  std::vector<std::string> output_lines = collect_execute_output_lines(output);
  std::string combined;
  for (const std::string &line : output_lines) {
    if (!combined.empty()) {
      combined += "\n";
    }
    combined += line;
  }
  mcp::JsonValue errors_arr(mcp::JsonValue::array_tag);
  const size_t kMaxErrors = 50;
  size_t error_total = 0;
  bool errors_truncated = false;
  std::string line_buf;
  line_buf.reserve(256);
  for (size_t idx = 0; idx <= combined.size(); ++idx) {
    char ch = (idx < combined.size()) ? combined[idx] : '\n';
    if (ch == '\n' || ch == '\r') {
      if (!line_buf.empty()) {
        std::string lowered = line_buf;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char c) {
                         return static_cast<char>(std::tolower(c));
                       });
        if (lowered.find("error") != std::string::npos) {
          if (error_total < kMaxErrors) {
            errors_arr.PushBack(mcp::JsonValue(line_buf));
          } else {
            errors_truncated = true;
          }
          ++error_total;
        }
      }
      line_buf.clear();
    } else {
      line_buf += ch;
    }
  }
  size_t digest_limit = sanitize_policy::enabled() ? 4000 : 64000;
  std::string digest = combined;
  bool output_truncated = false;
  if (digest.size() > digest_limit) {
    size_t total = digest.size();
    digest = digest.substr(0, digest_limit) + "\n...(truncated, total " +
             std::to_string(total) + " chars)";
    output_truncated = true;
  }
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["project_file"] = mcp::JsonValue(project_file);
  inner["project_path"] = mcp::JsonValue(project_path);
  inner["command"] = mcp::JsonValue(command);
  inner["exit_code"] = mcp::JsonValue(static_cast<int64_t>(exit_code));
  inner["success"] = mcp::JsonValue(exit_code == 0);
  const auto *editor = godot::EditorInterface::get_singleton();
  const bool game_running = editor != nullptr && editor->is_playing_scene();
  inner["game_running"] = mcp::JsonValue(game_running);
  inner["restart_required"] = mcp::JsonValue(exit_code == 0);
  mcp::JsonValue next_action(mcp::JsonValue::array_tag);
  if (exit_code == 0) {
    next_action.PushBack(mcp::JsonValue(
        "restart the running game process (stop_editor_playing, then "
        "play_editor_current_scene) so it loads the rebuilt assembly"));
    next_action.PushBack(mcp::JsonValue(
        "click Build in the editor toolbar or restart the editor to load the "
        "rebuilt assembly in the editor, then probe again with probe_script"));
  } else {
    next_action.PushBack(mcp::JsonValue(
        "fix the C# compile errors listed in errors, then call "
        "build_csharp_assembly again with wait=true"));
    next_action.PushBack(mcp::JsonValue(
        "when output_digest is empty, check dotnet_environment and "
        "get_debugger_log for a missing SDK or project file"));
  }
  inner["next_action"] = std::move(next_action);
  if (!probe_script.empty())
    inner["editor_assembly_probe"] = probe_editor_assembly(probe_script);
  inner["errors"] = std::move(errors_arr);
  inner["output_digest"] = mcp::JsonValue(digest);
  inner["output_empty"] = mcp::JsonValue(combined.empty());
  if (combined.empty()) {
    inner["output_hint"] = mcp::JsonValue(
        "dotnet 未产生任何可捕获输出：exit_code=" +
        std::to_string(static_cast<int64_t>(exit_code)) +
        "（-1 表示进程未能启动，常见于 dotnet 不在 PATH；0 且无输出说明构建"
        "未产生日志）");
  }
  if (output_truncated) {
    inner["output_truncated"] = mcp::JsonValue(true);
  }
  if (errors_truncated) {
    inner["errors_truncated"] = mcp::JsonValue(true);
  }
  inner["note"] = mcp::JsonValue(kNote);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "build_csharp_assembly completed");
  return r;
}

mcp::JsonValue handle_dotnet_environment(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "dotnet_environment called");
  std::string project_file;
  godot::Ref<godot::DirAccess> da = godot::DirAccess::open("res://");
  if (!da.is_null()) {
    da->list_dir_begin();
    godot::String entry = da->get_next();
    while (entry != godot::String()) {
      if (entry != "." && entry != ".." && !da->current_is_dir() &&
          (entry.ends_with(".csproj") || entry.ends_with(".sln"))) {
        project_file = "res://" + util::to_std(entry);
        break;
      }
      entry = da->get_next();
    }
    da->list_dir_end();
  }
  auto *os = godot::OS::get_singleton();
  if (!os)
    return util::error_json("OS singleton not available");
  godot::PackedStringArray version_args;
  version_args.append(godot::String("--version"));
  godot::Array output;
  output.append(godot::PackedStringArray());
  int32_t exit_code =
      os->execute(godot::String("dotnet"), version_args, output, true);
  if (exit_code != 0)
    return util::error_json("启动 dotnet 失败（SDK 缺失或不在 PATH？），请确认已安装 .NET SDK 且 dotnet 命令可用");
  std::string dotnet_version;
  {
    std::vector<std::string> version_lines = collect_execute_output_lines(output);
    if (!version_lines.empty()) {
      godot::String first = godot::String(version_lines.front().c_str());
      dotnet_version = util::to_std(first.strip_edges());
    }
  }
  if (project_file.empty())
    return csharp_setup::missing_project_error(dotnet_version);
  dotnet_env::DotnetProbe probe = dotnet_env::probe_current();
  const bool is_dotnet_editor = dotnet_env::is_dotnet_environment(probe);
  const bool assembly_loaded = probe.csharp_script_class;
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["dotnet_available"] = mcp::JsonValue(true);
  inner["dotnet_version"] = mcp::JsonValue(dotnet_version);
  inner["project_file"] = mcp::JsonValue(project_file);
  inner["is_dotnet_editor"] = mcp::JsonValue(is_dotnet_editor);
  inner["assembly_loaded"] = mcp::JsonValue(assembly_loaded);
  inner["engine_version"] = mcp::JsonValue(
      std::to_string(probe.engine_major) + "." +
      std::to_string(probe.engine_minor));
  inner["mono_feature"] = mcp::JsonValue(probe.mono_feature);
  inner["dotnet_feature"] = mcp::JsonValue(probe.dotnet_feature);
  inner["hint"] = mcp::JsonValue(
      "is_dotnet_editor 由 CSharpScript 类注册（Godot 4.x 的 .NET 模块装载时"
      "注册该类）判定，并以 OS.has_feature(dotnet)（4.4+）或 OS.has_feature("
      "mono)（4.4 之前）按引擎版本兜底；Godot 4.x 未注册 mono/dotnet 特性"
      "标签，特性名探测在多数版本下恒为 false；assembly_loaded 同样源于 "
      "CSharpScript 类存在性，反映 .NET 模块已装载而非项目程序集已加载");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "dotnet_environment completed");
  return r;
}

mcp::JsonValue handle_verify_scene_saved(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "verify_scene_saved called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no scene open");
    return e;
  }
  std::string target_path;
  auto *sp = args.Find("scene_path");
  if (sp && sp->IsString() && !sp->GetString().empty()) {
    target_path = sp->GetString();
  } else {
    target_path = util::to_std(root->get_scene_file_path());
  }
  if (target_path.empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "current scene has no file path — save it first (save_editor_scene), "
        "or provide scene_path");
    return e;
  }
  if (!godot::FileAccess::file_exists(godot::String(target_path.c_str()))) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("scene file not found on disk: " + target_path);
    e["path"] = mcp::JsonValue(target_path);
    return e;
  }
  bool disk_ok = false;
  const std::string text = scene_verify::read_text_file(target_path, disk_ok);
  if (!disk_ok) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("scene file could not be read: " + target_path);
    e["path"] = mcp::JsonValue(target_path);
    return e;
  }
  const bool want_hash = save_receipt_flag(args, "include_hash");
  const scene_verify::VerifyResult vr =
      scene_verify::compare_tree_with_text(root, text, want_hash);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(vr.match ? "match" : "mismatch");
  r["match"] = mcp::JsonValue(vr.match);
  r["path"] = mcp::JsonValue(target_path);
  r["memory_nodes"] = mcp::JsonValue(vr.memory_nodes);
  r["disk_nodes"] = mcp::JsonValue(vr.disk_nodes);
  r["memory_connections"] = mcp::JsonValue(vr.memory_connections);
  r["disk_connections"] = mcp::JsonValue(vr.disk_connections);
  r["connections_match"] =
      mcp::JsonValue(vr.memory_connections == vr.disk_connections);
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (const std::string &p : vr.missing_paths)
    arr.PushBack(mcp::JsonValue(p));
  r["missing_paths"] = std::move(arr);
  if (vr.missing_truncated)
    r["missing_truncated"] = mcp::JsonValue(true);
  r["inherited_nodes"] = mcp::JsonValue(vr.inherited_nodes);
  r["inherited_connections"] = mcp::JsonValue(vr.inherited_connections);
  if (!vr.inherited_paths.empty()) {
    mcp::JsonValue inherited_arr(mcp::JsonValue::array_tag);
    for (const std::string &p : vr.inherited_paths)
      inherited_arr.PushBack(mcp::JsonValue(p));
    r["inherited_paths"] = std::move(inherited_arr);
  }
  if (vr.inherited_truncated)
    r["inherited_truncated"] = mcp::JsonValue(true);
  if (want_hash && vr.hash_computed) {
    r["memory_hash"] = mcp::JsonValue(hash_hex(vr.memory_hash));
    r["disk_hash"] = mcp::JsonValue(hash_hex(vr.disk_hash));
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "verify_scene_saved completed");
  return r;
}

} // namespace editor_ops
} // namespace godot_autopilot
