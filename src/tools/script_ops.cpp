#include "script_ops.hpp"
#include "../util/scene_path.hpp"
#include "core/log_system.hpp"
#include "tools/debugger_ops.hpp"
#include "tools/scene_ops.hpp"
#include "util/gdscript_wrap.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>

namespace godot_autopilot {
namespace script_ops {

// —— 脚本取用口径（纯函数：不触碰 Godot 对象，L1 由
// tests/unit/script_freshness_test.cpp 直接覆盖）——
//
// godot-cpp 生成头的 ResourceLoader::load() 缺省 p_cache_mode = (CacheMode)1，
// 即 CACHE_MODE_REUSE：调用点只传 path 时会命中 ResourceCache 并原样返回旧实例，
// 不读盘（core/io/resource_loader.cpp:800-809）。要拿到磁盘版本必须显式传
// CACHE_MODE_IGNORE —— 该模式下 GDScript 格式加载器让 GDScriptCache 重新
// load_source_code() + reload()（modules/gdscript/gdscript_resource_format.cpp:41-42
// → modules/gdscript/gdscript_cache.cpp:372-381）。
int script_load_cache_mode(bool fresh) {
  return fresh ? static_cast<int>(godot::ResourceLoader::CACHE_MODE_IGNORE)
               : static_cast<int>(godot::ResourceLoader::CACHE_MODE_REUSE);
}

// 只读检查类工具的可选 fresh 参数：只认字面 true。缺省或非布尔值保持既有 REUSE
// 语义（不读盘），以免既有客户端在升版后取用口径发生静默变化。
bool wants_fresh_load(const mcp::JsonValue &args) {
  const mcp::JsonValue *flag = args.Find("fresh");
  return flag != nullptr && flag->IsBool() && flag->GetBool();
}

namespace {

constexpr const char *NODE_PATH_HINT =
    " — valid path forms: with scene root name (e.g. 'GameManager/HUD'), "
    "without root name (e.g. 'HUD'), with 'root/' prefix (e.g. "
    "'root/GameManager/HUD'), or absolute (e.g. '/root/GameManager/HUD')";

constexpr int PROPERTY_USAGE_CATEGORY = 0x80;
constexpr int PROPERTY_USAGE_INTERNAL = 0x08;

constexpr const char *SINGLE_EXPR_BLOCKING_KEYWORDS[] = {
    "if",   "for",   "while", "match",  "func",  "return",
    "var",  "const", "class", "static", "break", "continue",
    "pass", "await", "try",   "assert", "super", "self",
};

bool is_plain_assignment(const std::string &text) {
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '=')
      continue;
    char prev = i > 0 ? text[i - 1] : '\0';
    char next = i + 1 < text.size() ? text[i + 1] : '\0';
    bool prev_is_op = prev == '=' || prev == '!' || prev == '<' || prev == '>';
    bool next_is_op = next == '=' || next == '!' || next == '<' || next == '>';
    if (!prev_is_op && !next_is_op)
      return true;
  }
  return false;
}

bool is_single_expression(const std::string &text) {
  if (text.find('\n') != std::string::npos)
    return false;
  size_t start = text.find_first_not_of(" \t");
  if (start == std::string::npos || text[start] == '#')
    return false;
  if (is_plain_assignment(text))
    return false;
  size_t word_end = start;
  while (word_end < text.size()) {
    char c = text[word_end];
    if (c == ' ' || c == '\t' || c == '(' || c == ':')
      break;
    ++word_end;
  }
  std::string first_word = text.substr(start, word_end - start);
  for (const char *keyword : SINGLE_EXPR_BLOCKING_KEYWORDS) {
    if (first_word == keyword)
      return false;
  }
  return true;
}

mcp::JsonValue serialize_resource(const godot::Ref<godot::Resource> &res) {
  if (res.is_null())
    return mcp::JsonValue(nullptr);
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["class"] = mcp::JsonValue(util::to_std(res->get_class()));
  j["path"] = mcp::JsonValue(util::to_std(res->get_path()));
  j["object_id"] = mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
  j["object_id_str"] = mcp::JsonValue(
      std::to_string(static_cast<int64_t>(res->get_instance_id())));
  return j;
}

// 只摘除 ResourceCache 里的实例（set_path("")），不触及 GDScriptCache —— 后者仍
// 持有同一个 GDScript，REUSE 装载会直接返回它（gdscript_cache.cpp:352-358 命中
// full_gdscript_cache 即早返回，不读盘）。脚本刷新已改用 load_script_fresh （IGNORE
// 装载一次）；本函数保留给「只摘除、不重读」的场景。
[[maybe_unused]] bool invalidate_cached_resource(const std::string &path) {
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader || !loader->has_cached(godot::String::utf8(path.c_str()))) {
    return false;
  }
  auto cached = loader->get_cached_ref(godot::String::utf8(path.c_str()));
  if (cached.is_valid()) {
    cached->set_path("");
  }
  return true;
}

bool load_script_resource(const std::string &path, bool fresh,
                          godot::Ref<godot::Script> &out_script,
                          mcp::JsonValue &err_out) {
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    err_out = util::error_json("ResourceLoader not available");
    return false;
  }

  godot::Ref<godot::Resource> res = loader->load(
      godot::String::utf8(path.c_str()), "Script",
      static_cast<godot::ResourceLoader::CacheMode>(
          script_load_cache_mode(fresh)));
  if (res.is_null()) {
    err_out = util::error_json("failed to load script: " + path);
    return false;
  }

  godot::Ref<godot::Script> script = res;
  if (script.is_null()) {
    err_out = util::error_json("loaded resource is not a Script: " + path);
    return false;
  }
  out_script = script;
  return true;
}

bool load_script_or_error(const std::string &path,
                          godot::Ref<godot::Script> &out_script,
                          mcp::JsonValue &err_out) {
  return load_script_resource(path, false, out_script, err_out);
}

bool load_script_fresh(const std::string &path,
                       godot::Ref<godot::Script> &out_script,
                       mcp::JsonValue &err_out) {
  if (!load_script_resource(path, true, out_script, err_out))
    return false;
  // IGNORE 装载已让 GDScriptCache 重新 load_source_code() 并 reload()；这里再按编辑器
  // 同款姿势重编译一次（editor/script/script_editor_plugin.cpp:2562-2565），保证缓存实例
  // 与其使用者看到的是磁盘版本。
  out_script->reload(true);
  return true;
}

bool load_script_for_read(const std::string &path, const mcp::JsonValue &args,
                          godot::Ref<godot::Script> &out_script,
                          mcp::JsonValue &err_out) {
  return wants_fresh_load(args)
             ? load_script_fresh(path, out_script, err_out)
             : load_script_or_error(path, out_script, err_out);
}

} // namespace

mcp::JsonValue handle_execute_gdscript(const mcp::JsonValue &args) {
  auto *it_expr = args.Find("expression");
  if (!it_expr || !it_expr->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: expression");
    return e;
  }
  std::string expression = it_expr->GetString();
  if (expression.empty())
    return util::error_json("expression must not be empty");

  std::string cleaned = gdscript_wrap::strip_extends_lines(expression);

  std::string wrapped;
  if (gdscript_wrap::has_top_level_func_def(cleaned)) {

    if (!gdscript_wrap::defines_function_named(cleaned, "_run")) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("multi-function mode requires a func _run() "
                                  "entry point（或将函数改为 lambda 变量）");
      return e;
    }
    wrapped = "@tool\nextends Node\n\nvar SceneRoot := "
              "EditorInterface.get_edited_scene_root()\n\n" +
              cleaned + "\n";
  } else if (is_single_expression(expression)) {
    wrapped = "@tool\nextends Node\n\nfunc _run():\n";

    wrapped += "    var SceneRoot := EditorInterface.get_edited_scene_root()\n";
    wrapped += "    return " + cleaned + "\n";
    wrapped += "\n";
  } else {

    gdscript_wrap::IndentStyle indent =
        gdscript_wrap::scan_indent_style(cleaned);
    if (indent.uses_tabs && indent.uses_spaces) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "mixed tab/space indentation detected in source — reindent source "
          "with only tabs or only spaces; note the wrapper requires the same "
          "indentation style throughout");
      return e;
    }
    std::string prefix = gdscript_wrap::indent_prefix(indent);
    wrapped = "@tool\nextends Node\n\nfunc _run():\n";

    wrapped +=
        prefix + "var SceneRoot := EditorInterface.get_edited_scene_root()\n";
    wrapped += gdscript_wrap::reindent_lines(cleaned, prefix);
    wrapped += "\n";
  }

  godot::Ref<godot::GDScript> script;
  script.instantiate();
  if (script.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to create GDScript instance");
    return e;
  }

  script->set_source_code(godot::String::utf8(wrapped.c_str()));
  size_t compile_log_before = debugger_ops::capture_log_count();
  godot::Error parse_err2 = script->reload();
  if (parse_err2 != godot::OK) {
    std::string message = gdscript_wrap::compose_compile_failure_message(
        "GDScript compilation failed: ERR_PARSE_ERROR (code " +
            std::to_string(static_cast<int>(parse_err2)) + ")",
        debugger_ops::capture_new_error_text(compile_log_before), wrapped,
        gdscript_wrap::truncate_capture_text);
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(message);
    return e;
  }

  godot::Node *temp_node = memnew(godot::Node);
  temp_node->set_script(godot::Variant(script));

  bool temp_added = false;
  godot::Node *parent_node = nullptr;
  {
    auto *engine = godot::Engine::get_singleton();
    auto *main_loop = engine ? engine->get_main_loop() : nullptr;
    auto *tree = godot::Object::cast_to<godot::SceneTree>(main_loop);
    if (tree) {
      parent_node = godot::Object::cast_to<godot::Node>(tree->get_root());
    }
  }
  if (!parent_node) {
    auto *editor = godot::EditorInterface::get_singleton();
    parent_node = editor ? editor->get_edited_scene_root() : nullptr;
  }
  if (parent_node) {
    parent_node->add_child(temp_node);
    temp_added = true;
  }

  godot::StringName fn_name("_run");
  if (!temp_node->has_method(fn_name)) {
    if (temp_added && temp_node->get_parent())
      temp_node->get_parent()->remove_child(temp_node);
    memdelete(temp_node);
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("unexpected error: compiled script missing _run method");
    return e;
  }

  size_t log_before = debugger_ops::capture_log_count();
  godot::Variant result = temp_node->call(fn_name);
  std::string new_output_text =
      debugger_ops::capture_new_output_text(log_before);
  std::string new_error_text = debugger_ops::capture_new_error_text(log_before);

  if (temp_added && temp_node->get_parent())
    temp_node->get_parent()->remove_child(temp_node);
  memdelete(temp_node);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(result);
  if (!new_output_text.empty()) {
    r["output"] =
        mcp::JsonValue(gdscript_wrap::truncate_capture_text(new_output_text));
  }
  if (!new_error_text.empty()) {
    std::string errors = gdscript_wrap::truncate_capture_text(new_error_text);
    if (new_error_text.find("Node not found") != std::string::npos) {
      errors += gdscript_wrap::NODE_NOT_FOUND_HINT;
    }
    r["errors"] = mcp::JsonValue(errors);
  }
  return r;
}

mcp::JsonValue handle_load(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();

  godot::Ref<godot::Script> script;
  mcp::JsonValue load_err;
  if (!load_script_for_read(path, args, script, load_err))
    return load_err;

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = serialize_resource(script);
  return r;
}

mcp::JsonValue handle_create(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  auto *it_code = args.Find("source_code");
  if (!it_code || !it_code->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: source_code");
    return e;
  }
  std::string path = it_path->GetString();
  std::string source_code = it_code->GetString();

  bool overwrite = false;
  auto *it_overwrite = args.Find("overwrite");
  if (it_overwrite && it_overwrite->IsBool())
    overwrite = it_overwrite->GetBool();

  bool file_exists_on_disk =
      godot::FileAccess::file_exists(godot::String(path.c_str()));
  if (!overwrite && file_exists_on_disk) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("file already exists: " + path +
                                " — pass overwrite=true to replace it");
    return e;
  }

  godot::Ref<godot::GDScript> script;
  script.instantiate();
  if (script.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to create GDScript");
    return e;
  }

  // 源文件按 UTF-8 落盘（ResourceFormatSaverGDScript::save → FileAccess::store_string
  // 写入 String 的 UTF-8 字节），故必须用 String::utf8 构造；latin1 构造会让每个字节
  // 变成一个字符，CJK 注释写出后变乱码且 readback 校验失败。
  script->set_source_code(godot::String::utf8(source_code.c_str()));
  script->set_path_cache(godot::String(path.c_str()));
  size_t compile_log_before = debugger_ops::capture_log_count();
  godot::Error reload_err = script->reload();
  if (reload_err != godot::OK) {
    std::string message = "script compilation failed: ERR_PARSE_ERROR (code " +
                          std::to_string(static_cast<int>(reload_err)) + ")";
    std::string compile_err =
        debugger_ops::capture_new_error_text(compile_log_before);
    if (!compile_err.empty()) {
      message += "\n" + gdscript_wrap::truncate_capture_text(compile_err);
    }
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(message);
    return e;
  }

  auto *saver = godot::ResourceSaver::get_singleton();
  if (!saver) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceSaver not available");
    return e;
  }

  std::string dir_path = path;
  size_t last_slash = dir_path.find_last_of('/');
  if (last_slash != std::string::npos) {
    dir_path = dir_path.substr(0, last_slash);
    auto dir = godot::DirAccess::open(godot::String("res://"));
    if (dir.is_valid()) {
      dir->make_dir_recursive(godot::String(dir_path.c_str()));
    }
  }

  godot::Error err = saver->save(script, godot::String(path.c_str()));
  if (err != godot::OK) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to save script, error code: " +
                                std::to_string(static_cast<int>(err)));
    return e;
  }

  bool verified = false;
  bool readback = false;
  std::string write_issue;
  std::string write_warning;
  auto readback_file = godot::FileAccess::open(
      godot::String(path.c_str()), godot::FileAccess::READ);
  if (readback_file.is_valid() && readback_file->is_open()) {
    readback = true;
    std::string read_back = util::to_std(readback_file->get_as_text());
    godot::Error read_err = readback_file->get_error();
    if (read_err != godot::OK && read_err != godot::ERR_FILE_EOF) {
      write_issue = "readback read error";
    } else if (read_back == source_code) {
      verified = true;
    } else {
      write_issue = "readback mismatch (written " +
                    std::to_string(source_code.size()) + " bytes, read back " +
                    std::to_string(read_back.size()) + " bytes)";
      write_warning =
          "file may not have been updated on disk; retry or check file locks "
          "(e.g. the running game or antivirus)";
    }
  } else {
    write_issue = "readback open failed";
  }

  // 写盘后用 IGNORE 装载刷新一次：GDScriptCache 里的旧实例被重新读盘 + 重编译，
  // 替代原先「从 ResourceCache 摘除」（摘除只动 ResourceCache，REUSE 装载仍会从
  // GDScriptCache 取回旧实例）。刷新失败不使本次写入失败，但在响应中回报。
  godot::Ref<godot::Script> refreshed;
  mcp::JsonValue refresh_err;
  bool cache_refreshed = load_script_fresh(path, refreshed, refresh_err);
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *efs = editor->get_resource_filesystem();
    if (efs) {

      efs->update_file(godot::String(path.c_str()));
    }
  }

  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["path"] = mcp::JsonValue(path);
  j["class"] = mcp::JsonValue("GDScript");
  j["overwritten"] = mcp::JsonValue(file_exists_on_disk);
  j["verified"] = mcp::JsonValue(verified);
  j["readback"] = mcp::JsonValue(readback);
  if (!write_issue.empty()) {
    j["write_issue"] = mcp::JsonValue(write_issue);
  }
  if (!write_warning.empty()) {
    j["warning"] = mcp::JsonValue(write_warning);
  }
  j["cache_refreshed"] = mcp::JsonValue(cache_refreshed);
  if (!cache_refreshed) {
    const mcp::JsonValue *refresh_msg = refresh_err.Find("error");
    j["cache_refresh_error"] =
        mcp::JsonValue(refresh_msg && refresh_msg->IsString()
                           ? refresh_msg->GetString()
                           : std::string("script cache refresh failed"));
  }
  godot::StringName global_name = script->get_global_name();
  if (global_name != godot::StringName()) {
    j["global_class_name"] =
        mcp::JsonValue(util::to_std(godot::String(global_name)));

    j["global_class_hint"] = mcp::JsonValue(
        "global class registration may be delayed — cross-script references "
        "may need preload() or a filesystem rescan");
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  return r;
}

mcp::JsonValue handle_attach_to_node(const mcp::JsonValue &args) {
  auto *it_node = args.Find("node_path");
  auto *it_script = args.Find("script_path");
  if (!it_node || !it_node->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  if (!it_script || !it_script->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: script_path");
    return e;
  }
  std::string node_path = it_node->GetString();
  std::string script_path = it_script->GetString();

  godot::Node *root = nullptr;
  if (auto *editor = godot::EditorInterface::get_singleton()) {
    root = editor->get_edited_scene_root();
  }
  std::string hint;
  godot::Node *node = util::resolve_scene_node(node_path, root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path +
                                NODE_PATH_HINT + " — " + hint);
    return e;
  }

  godot::Ref<godot::Script> script;
  mcp::JsonValue load_err;
  if (!load_script_fresh(script_path, script, load_err))
    return load_err;

  auto *editor = godot::EditorInterface::get_singleton();
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  if (undo_redo) {
    undo_redo->create_action("Attach Script");
    undo_redo->add_do_method(node, godot::StringName("set_script"),
                             godot::Variant(script));
    undo_redo->add_undo_method(node, godot::StringName("set_script"),
                               node->get_script());
    undo_redo->commit_action();
  } else {
    node->set_script(godot::Variant(script));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("script attached to " + node_path);
  bool instantiable = script->can_instantiate();
  r["instantiated"] = mcp::JsonValue(instantiable);
  util::add_scene_info_fields(r, util::edited_scene_info());
  if (!instantiable) {
    r["note"] =
        mcp::JsonValue("script is not instantiable in the editor: method calls "
                       "will fail until the game is run — mark the script with "
                       "@tool to run in the editor");
  }
  return r;
}

mcp::JsonValue handle_detach_from_node(const mcp::JsonValue &args) {
  auto *it_node = args.Find("node_path");
  if (!it_node || !it_node->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  std::string node_path = it_node->GetString();

  godot::Node *root = nullptr;
  if (auto *editor = godot::EditorInterface::get_singleton()) {
    root = editor->get_edited_scene_root();
  }
  std::string hint;
  godot::Node *node = util::resolve_scene_node(node_path, root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path +
                                NODE_PATH_HINT + " — " + hint);
    return e;
  }

  auto *editor = godot::EditorInterface::get_singleton();
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  if (undo_redo) {
    undo_redo->create_action("Detach Script");
    undo_redo->add_do_method(node, godot::StringName("set_script"),
                             godot::Variant());
    undo_redo->add_undo_method(node, godot::StringName("set_script"),
                               node->get_script());
    undo_redo->commit_action();
  } else {
    node->set_script(godot::Variant());
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("script detached from " + node_path);
  return r;
}

mcp::JsonValue handle_get_property(const mcp::JsonValue &args) {
  auto *it_prop = args.Find("property");
  if (!it_prop || !it_prop->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
    return e;
  }
  std::string property = it_prop->GetString();

  auto *it_path = args.Find("script_path");
  if (it_path && it_path->IsString()) {
    std::string script_path = it_path->GetString();
    godot::Ref<godot::Script> script;
    mcp::JsonValue load_err;
    if (!load_script_for_read(script_path, args, script, load_err))
      return load_err;

    godot::StringName prop_name(property.c_str());
    godot::Variant val = script->get_property_default_value(prop_name);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(val);
    return r;
  }

  auto *it_node = args.Find("node_path");
  if (it_node && it_node->IsString()) {
    std::string node_path = it_node->GetString();
    godot::Node *root = nullptr;
    if (auto *editor = godot::EditorInterface::get_singleton()) {
      root = editor->get_edited_scene_root();
    }
    std::string hint;
    godot::Node *node = util::resolve_scene_node(node_path, root, &hint);
    if (!node) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("node not found: " + node_path +
                                  NODE_PATH_HINT + " — " + hint);
      return e;
    }
    godot::StringName prop_name(property.c_str());
    godot::Variant val = node->get(prop_name);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(val);
    return r;
  }

  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue("must provide either script_path or node_path");
  return e;
}

mcp::JsonValue handle_set_property(const mcp::JsonValue &args) {
  auto *it_node = args.Find("node_path");
  auto *it_prop = args.Find("property");
  auto *it_val = args.Find("value");
  if (!it_node || !it_node->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  if (!it_prop || !it_prop->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
    return e;
  }
  if (!it_val) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: value");
    return e;
  }
  std::string node_path = it_node->GetString();
  std::string property = it_prop->GetString();

  godot::Node *root = nullptr;
  if (auto *editor = godot::EditorInterface::get_singleton()) {
    root = editor->get_edited_scene_root();
  }
  std::string hint;
  godot::Node *node = util::resolve_scene_node(node_path, root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path +
                                NODE_PATH_HINT + " — " + hint);
    return e;
  }

  std::string type_hint;
  auto *it_hint = args.Find("type_hint");
  if (it_hint && it_hint->IsString()) {
    type_hint = it_hint->GetString();
  }

  godot::StringName prop_name(property.c_str());
  godot::Variant value = VariantJson::deserialize(*it_val, type_hint);
  node->set(prop_name, value);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
}

mcp::JsonValue handle_call_function(const mcp::JsonValue &args) {
  auto *it_node = args.Find("node_path");
  if (!it_node || !it_node->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  auto *it_func = args.Find("function");
  if (!it_func || !it_func->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: function");
    return e;
  }
  std::string node_path = it_node->GetString();
  std::string function = it_func->GetString();

  godot::Node *root = nullptr;
  if (auto *editor = godot::EditorInterface::get_singleton()) {
    root = editor->get_edited_scene_root();
  }
  std::string hint;
  godot::Node *node = util::resolve_scene_node(node_path, root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path +
                                NODE_PATH_HINT + " — " + hint);
    return e;
  }

  godot::Ref<godot::Script> script = node->get_script();
  if (script.is_valid() && !script->can_instantiate()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("script is not instantiable in the editor: " +
                       util::to_std(script->get_path()) +
                       " — scripts must be marked @tool to run in the editor, "
                       "or run the game to execute non-tool scripts");
    return e;
  }

  godot::StringName func_name(function.c_str());
  if (!node->has_method(func_name)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("method not found: " + function + " on node " +
                                node_path);
    return e;
  }

  godot::Array args_arr;
  auto *it_arr = args.Find("args");
  if (!it_arr || !it_arr->IsArray()) {
    it_arr = args.Find("arguments");
  }
  if (it_arr && it_arr->IsArray()) {
    const auto &arr = it_arr->GetArray();
    for (const auto &a : arr) {
      args_arr.append(VariantJson::deserialize(a));
    }
  }

  godot::Variant result = node->callv(func_name, args_arr);
  if (result.get_type() == godot::Variant::NIL && script.is_valid() &&
      !script->can_instantiate()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("method call failed on node " + node_path +
                       ": script instance unavailable in the editor — scripts "
                       "must be marked @tool to run in the editor, or run the "
                       "game to execute non-tool scripts");
    return e;
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(result);
  return r;
}

mcp::JsonValue handle_reload(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();

  bool keep_state = false;
  auto *ks = args.Find("keep_state");
  if (ks && ks->IsBool())
    keep_state = ks->GetBool();

  godot::Ref<godot::Script> script;
  mcp::JsonValue load_err;
  if (!load_script_fresh(path, script, load_err))
    return load_err;

  godot::Error reload_result = script->reload(keep_state);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(reload_result));
  return r;
}

mcp::JsonValue handle_get_variable_list(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();

  godot::Ref<godot::Script> script;
  mcp::JsonValue load_err;
  if (!load_script_for_read(path, args, script, load_err))
    return load_err;

  godot::TypedArray<godot::Dictionary> props = script->get_script_property_list();
  mcp::JsonValue result(mcp::JsonValue::array_tag);
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    int usage = 0;
    if (dict.has("usage")) {
      usage = static_cast<int>(dict["usage"]);
    }
    if ((usage & PROPERTY_USAGE_CATEGORY) != 0 ||
        (usage & PROPERTY_USAGE_INTERNAL) != 0) {
      continue;
    }
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["usage"] = mcp::JsonValue(static_cast<int64_t>(usage));
    if (dict.has("name")) {
      item["name"] =
          mcp::JsonValue(util::to_std(dict["name"].operator godot::String()));
    }
    if (dict.has("type")) {
      int type_id = static_cast<int>(dict["type"]);
      item["type_id"] = mcp::JsonValue(static_cast<int64_t>(type_id));
      item["type"] = mcp::JsonValue(util::to_std(godot::Variant::get_type_name(
          static_cast<godot::Variant::Type>(type_id))));
    }
    result.PushBack(std::move(item));
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  return r;
}

} // namespace script_ops
} // namespace godot_autopilot
