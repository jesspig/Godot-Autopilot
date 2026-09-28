#include "analyze_ops.hpp"

#include "../util/error_util.hpp"
#include "../util/scene_path.hpp"
#include "../util/scene_verify.hpp"
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_file_system_directory.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/signal.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace analyze_ops {

namespace {

constexpr int64_t DEFAULT_MAX_DEPTH = 3;
constexpr const char *AUTOLOAD_PREFIX = "autoload/";

mcp::JsonValue make_error(const std::string &msg) {
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue(msg);
  return e;
}

bool read_string_arg(const mcp::JsonValue &args, const char *key,
                     std::string &out) {
  auto *v = args.Find(key);
  if (v && v->IsString()) {
    out = v->GetString();
    return true;
  }
  return false;
}

bool require_string_arg(const mcp::JsonValue &args, const char *key,
                        std::string &out) {
  return read_string_arg(args, key, out) && !out.empty();
}

int64_t read_int_arg(const mcp::JsonValue &args, const char *key,
                     int64_t fallback) {
  auto *v = args.Find(key);
  if (!v || !v->IsNumber())
    return fallback;
  return v->IsInt() ? v->GetInt() : static_cast<int64_t>(v->GetDouble());
}

bool begins_with(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void push_problem(mcp::JsonValue &problems, const char *kind,
                  const std::string &message) {
  mcp::JsonValue p(mcp::JsonValue::object_tag);
  p["kind"] = mcp::JsonValue(kind);
  p["message"] = mcp::JsonValue(message);
  problems.PushBack(std::move(p));
}

std::string resolve_dependency_path(const std::string &dep, bool &ok) {
  const std::string uid_prefix = "uid://";
  if (!begins_with(dep, uid_prefix)) {
    ok = true;
    return dep;
  }
  auto *uid_db = godot::ResourceUID::get_singleton();
  if (!uid_db) {
    ok = false;
    return "";
  }
  int64_t id = uid_db->text_to_id(godot::String(dep.c_str()));
  if (id == static_cast<int64_t>(godot::ResourceUID::INVALID_ID) ||
      !uid_db->has_id(id)) {
    ok = false;
    return "";
  }
  ok = true;
  return util::to_std(uid_db->get_id_path(id));
}

std::string basename_of(const std::string &path) {
  size_t pos = path.find_last_of('/');
  return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string setting_value_to_path(const godot::Variant &value) {
  if (value.get_type() != godot::Variant::STRING)
    return "";
  std::string s = util::to_std(value.operator godot::String());
  if (!s.empty() && s[0] == '*')
    s = s.substr(1);
  return s;
}

void collect_files(
    godot::EditorFileSystemDirectory *dir,
    std::vector<std::pair<std::string, std::string>> &out) {
  if (!dir)
    return;
  for (int i = 0; i < dir->get_file_count(); i++)
    out.emplace_back(util::to_std(dir->get_file_path(i)),
                     util::to_std(dir->get_file_type(i)));
  for (int i = 0; i < dir->get_subdir_count(); i++)
    collect_files(dir->get_subdir(i), out);
}

std::string extract_path_token_on_line(const std::string &line) {
  const std::string key = "path=\"";
  std::size_t pos = line.find(key);
  if (pos == std::string::npos)
    return "";
  pos += key.size();
  std::size_t end = line.find('"', pos);
  if (end == std::string::npos)
    return "";
  return line.substr(pos, end - pos);
}

int64_t supplement_references_from_text(
    const std::vector<std::pair<std::string, std::string>> &files,
    const std::vector<std::string> &empty_dep_files,
    const std::set<std::string> &unresolved_uids,
    std::set<std::string> &referenced, bool &truncated) {
  truncated = false;
  if (empty_dep_files.empty() && unresolved_uids.empty())
    return 0;
  std::vector<std::pair<std::string, std::string>> path_tokens;
  for (const std::string &p : empty_dep_files) {
    if (referenced.count(p) == 0)
      path_tokens.emplace_back("path=\"" + p + "\"", p);
  }
  std::vector<std::string> uid_tokens;
  for (const std::string &u : unresolved_uids)
    uid_tokens.push_back("uid=\"" + u + "\"");
  if (path_tokens.empty() && uid_tokens.empty())
    return 0;
  int64_t supplemented = 0;
  size_t bytes_read = 0;
  for (const auto &entry : files) {
    if (!ends_with(entry.first, ".tscn") && !ends_with(entry.first, ".tres"))
      continue;
    if (bytes_read > 32 * 1024 * 1024) {
      truncated = true;
      break;
    }
    bool ok = false;
    const std::string text = scene_verify::read_text_file(entry.first, ok);
    if (!ok)
      continue;
    if (text.size() > 2 * 1024 * 1024) {
      truncated = true;
      continue;
    }
    bytes_read += text.size();
    std::size_t pos = 0;
    while (pos < text.size()) {
      std::size_t line_end = text.find('\n', pos);
      std::string line = text.substr(pos, line_end == std::string::npos
                                                ? std::string::npos
                                                : line_end - pos);
      pos = (line_end == std::string::npos) ? text.size() : line_end + 1;
      for (const auto &tok : path_tokens) {
        if (line.find(tok.first) == std::string::npos)
          continue;
        if (referenced.insert(tok.second).second)
          supplemented++;
      }
      for (const std::string &tok : uid_tokens) {
        if (line.find(tok) == std::string::npos)
          continue;
        const std::string hit_path = extract_path_token_on_line(line);
        if (!hit_path.empty() && referenced.insert(hit_path).second)
          supplemented++;
      }
    }
  }
  return supplemented;
}

std::string describe_node(godot::Node *node, godot::Node *scene_root) {
  std::string abs = util::to_std(node->get_path());
  if (!scene_root)
    return abs;
  std::string pref = util::to_std(scene_root->get_path());
  if (abs == pref)
    return util::to_std(node->get_name());
  if (abs.find(pref + "/") == 0)
    return abs.substr(pref.size() + 1);
  return abs;
}

std::string describe_object(godot::Object *obj, godot::Node *scene_root) {
  godot::Node *node = godot::Object::cast_to<godot::Node>(obj);
  if (!node)
    return util::to_std(obj->get_class()) + "#" +
           std::to_string(static_cast<int64_t>(obj->get_instance_id()));
  return describe_node(node, scene_root);
}

void append_connection_edges(
    const godot::TypedArray<godot::Dictionary> &connections,
    godot::Node *scene_root, bool neighbor_is_target, mcp::JsonValue &edges,
    std::set<std::string> &edge_keys, std::vector<godot::Node *> &neighbors) {
  for (int i = 0; i < connections.size(); i++) {
    godot::Dictionary conn = connections[i];
    godot::Signal sig = conn["signal"].operator godot::Signal();
    if (sig.is_null())
      continue;
    godot::Object *source_obj = sig.get_object();
    if (!source_obj)
      continue;

    std::vector<godot::Callable> callables;
    godot::Variant cv = conn["callable"];
    if (cv.get_type() == godot::Variant::ARRAY) {
      godot::Array arr = cv.operator godot::Array();
      for (int j = 0; j < arr.size(); j++)
        callables.push_back(arr[j].operator godot::Callable());
    } else {
      callables.push_back(cv.operator godot::Callable());
    }

    int64_t flags = static_cast<int64_t>(conn["flags"]);
    bool persisted = (flags & static_cast<int64_t>(
                                godot::Object::CONNECT_PERSIST)) != 0;
    std::string sig_name =
        util::to_std(godot::String(sig.get_name()));

    for (const godot::Callable &call : callables) {
      godot::Object *target_obj = call.get_object();
      if (!target_obj)
        continue;
      std::string method =
          util::to_std(godot::String(call.get_method()));
      std::string key =
          std::to_string(
              static_cast<int64_t>(source_obj->get_instance_id())) +
          ":" + sig_name + ":" +
          std::to_string(
              static_cast<int64_t>(target_obj->get_instance_id())) +
          ":" + method;
      if (!edge_keys.insert(key).second)
        continue;

      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["from"] = mcp::JsonValue(describe_object(source_obj, scene_root));
      e["signal"] = mcp::JsonValue(sig_name);
      e["to"] = mcp::JsonValue(describe_object(target_obj, scene_root));
      e["method"] = mcp::JsonValue(method);
      e["persisted"] = mcp::JsonValue(persisted);
      edges.PushBack(std::move(e));

      godot::Node *neighbor = godot::Object::cast_to<godot::Node>(
          neighbor_is_target ? target_obj : source_obj);
      if (neighbor)
        neighbors.push_back(neighbor);
    }
  }
}

} // namespace

mcp::JsonValue handle_validate_scene_file(const mcp::JsonValue &args) {
  std::string path;
  if (!require_string_arg(args, "path", path))
    return make_error("missing required parameter: path (res:// scene file, "
                      "e.g. \"res://levels/level_01.tscn\")");

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader)
    return make_error("ResourceLoader is unavailable");

  godot::String res_path(path.c_str());
  mcp::JsonValue problems(mcp::JsonValue::array_tag);
  mcp::JsonValue missing(mcp::JsonValue::array_tag);

  godot::Ref<godot::PackedScene> scene =
      loader->load(res_path, "PackedScene",
                   godot::ResourceLoader::CACHE_MODE_IGNORE);

  int64_t dependency_count = 0;
  if (scene.is_null()) {
    push_problem(problems, "load_failed",
                 "failed to load \"" + path +
                     "\" as PackedScene — file missing, unreadable or not a "
                     "scene");
  } else {
    godot::PackedStringArray deps = loader->get_dependencies(res_path);
    dependency_count = static_cast<int64_t>(deps.size());
    for (int i = 0; i < deps.size(); i++) {
      std::string dep = util::to_std(deps[i]);
      bool resolved_ok = false;
      std::string resolved = resolve_dependency_path(dep, resolved_ok);
      if (!resolved_ok)
        continue;
      if (!godot::FileAccess::file_exists(godot::String(resolved.c_str()))) {
        missing.PushBack(mcp::JsonValue(dep));
        push_problem(problems, "missing_dependency",
                     "\"" + path + "\" depends on \"" + dep +
                         "\" which does not exist on disk");
      }
    }

    godot::Node *instance = scene->instantiate();
    if (!instance) {
      push_problem(problems, "instantiate_failed",
                   "PackedScene::instantiate() returned null for \"" + path +
                       "\"");
    } else {
      memdelete(instance);
    }
  }

  bool valid = problems.Size() == 0;

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["valid"] = mcp::JsonValue(valid);
  inner["problems"] = std::move(problems);
  inner["missing_dependencies"] = std::move(missing);
  inner["dependency_count"] = mcp::JsonValue(dependency_count);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

mcp::JsonValue handle_find_unused_resources(const mcp::JsonValue &args) {
  std::string directory;
  read_string_arg(args, "directory", directory);

  auto *editor = godot::EditorInterface::get_singleton();
  auto *efs = editor ? editor->get_resource_filesystem() : nullptr;
  if (!efs)
    return make_error("editor resource filesystem is unavailable");
  const bool efs_scanning = efs->is_scanning();

  godot::EditorFileSystemDirectory *start_dir = nullptr;
  if (directory.empty() || directory == "res://" || directory == "/") {
    start_dir = efs->get_filesystem();
  } else {
    std::string normalized = directory;
    if (!begins_with(normalized, "res://"))
      normalized = "res://" + normalized;
    start_dir = efs->get_filesystem_path(godot::String(normalized.c_str()));
    if (!start_dir)
      return make_error("directory not found in the editor filesystem: " +
                        directory);
  }
  if (!start_dir)
    return make_error("editor filesystem root is unavailable");

  std::vector<std::pair<std::string, std::string>> files;
  collect_files(start_dir, files);

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader)
    return make_error("ResourceLoader is unavailable");

  std::set<std::string> referenced;
  std::set<std::string> unresolved_uids;
  std::vector<std::string> empty_dep_files;
  for (const auto &entry : files) {
    godot::PackedStringArray deps =
        loader->get_dependencies(godot::String(entry.first.c_str()));
    if (deps.size() == 0)
      empty_dep_files.push_back(entry.first);
    for (int i = 0; i < deps.size(); i++) {
      std::string dep = util::to_std(deps[i]);
      bool resolved_ok = false;
      std::string resolved = resolve_dependency_path(dep, resolved_ok);
      if (!resolved_ok) {
        unresolved_uids.insert(dep);
        continue;
      }
      referenced.insert(resolved);
    }
  }
  bool text_truncated = false;
  const int64_t text_supplemented = supplement_references_from_text(
      files, empty_dep_files, unresolved_uids, referenced, text_truncated);

  std::set<std::string> exempted;
  auto exempt = [&exempted](const std::string &p) {
    if (!p.empty())
      exempted.insert(p);
  };

  exempt("res://project.godot");
  auto *settings = godot::ProjectSettings::get_singleton();
  if (settings) {
    if (settings->has_setting("application/config/icon"))
      exempt(setting_value_to_path(
          settings->get_setting("application/config/icon")));
    if (settings->has_setting("application/run/main_scene"))
      exempt(setting_value_to_path(
          settings->get_setting("application/run/main_scene")));
    godot::TypedArray<godot::Dictionary> props = settings->get_property_list();
    for (int i = 0; i < props.size(); i++) {
      godot::Dictionary d = props[i];
      std::string name = util::to_std(d["name"].operator godot::String());
      if (begins_with(name, AUTOLOAD_PREFIX))
        exempt(setting_value_to_path(
            settings->get_setting(godot::String(name.c_str()))));
    }
  }

  mcp::JsonValue unused(mcp::JsonValue::array_tag);
  for (const auto &entry : files) {
    const std::string &path = entry.first;
    if (referenced.count(path) > 0)
      continue;
    if (exempted.count(path) > 0)
      continue;
    std::string base = basename_of(path);
    if (base == "icon.svg" || ends_with(path, ".import")) {
      exempted.insert(path);
      continue;
    }
    mcp::JsonValue u(mcp::JsonValue::object_tag);
    u["path"] = mcp::JsonValue(path);
    u["type"] = mcp::JsonValue(entry.second);
    unused.PushBack(std::move(u));
  }

  mcp::JsonValue unresolved_arr(mcp::JsonValue::array_tag);
  for (const auto &uid : unresolved_uids)
    unresolved_arr.PushBack(mcp::JsonValue(uid));

  mcp::JsonValue exempted_arr(mcp::JsonValue::array_tag);
  for (const auto &e : exempted)
    exempted_arr.PushBack(mcp::JsonValue(e));

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["scanned"] = mcp::JsonValue(static_cast<int64_t>(files.size()));
  inner["filesystem_scanning"] = mcp::JsonValue(efs_scanning);
  inner["unused"] = std::move(unused);
  inner["unresolved_uids"] = std::move(unresolved_arr);
  inner["exempted"] = std::move(exempted_arr);
  if (text_supplemented > 0)
    inner["text_supplemented"] = mcp::JsonValue(text_supplemented);
  if (text_truncated)
    inner["text_scan_truncated"] = mcp::JsonValue(true);
  std::string note;
  if (efs_scanning)
    note += "editor filesystem is scanning, results may be stale — run "
            "scan_editor_file_system first and retry; ";
  if (!unresolved_uids.empty())
    note += "unresolved_uids is non-empty, results may be incomplete; ";
  if (text_supplemented > 0)
    note += "text fallback matched " + std::to_string(text_supplemented) +
            " extra reference(s) from .tscn/.tres ext_resource tokens; ";
  if (text_truncated)
    note += "text fallback hit its size budget and skipped files; ";
  if (!note.empty())
    inner["note"] = mcp::JsonValue(note);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

mcp::JsonValue handle_trace_signal_flow(const mcp::JsonValue &args) {
  std::string path;
  if (!require_string_arg(args, "path", path))
    return make_error("missing required parameter: path (edited-scene node "
                      "path, e.g. \"Player\")");

  std::string direction = "both";
  read_string_arg(args, "direction", direction);
  if (direction != "outgoing" && direction != "incoming" &&
      direction != "both")
    return make_error("direction must be \"outgoing\", \"incoming\" or "
                      "\"both\" (got \"" +
                      direction + "\")");

  int64_t max_depth = read_int_arg(args, "max_depth", DEFAULT_MAX_DEPTH);
  if (max_depth < 1)
    return make_error("max_depth must be >= 1 (got " +
                      std::to_string(max_depth) + ")");

  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  godot::Node *root_node = util::resolve_scene_node(path, scene_root, &hint);
  if (!root_node)
    return make_error("node not found: " + path + " — " + hint);

  bool do_outgoing = direction == "outgoing" || direction == "both";
  bool do_incoming = direction == "incoming" || direction == "both";

  std::vector<std::pair<godot::Node *, int64_t>> queue;
  queue.emplace_back(root_node, 0);
  std::set<int64_t> visited{
      static_cast<int64_t>(root_node->get_instance_id())};

  mcp::JsonValue edges(mcp::JsonValue::array_tag);
  std::set<std::string> edge_keys;

  while (!queue.empty()) {
    godot::Node *current = queue.front().first;
    int64_t depth = queue.front().second;
    queue.erase(queue.begin());

    std::vector<godot::Node *> neighbors;
    if (do_outgoing) {
      godot::TypedArray<godot::Dictionary> signals =
          current->get_signal_list();
      for (int i = 0; i < signals.size(); i++) {
        godot::Dictionary sig_info = signals[i];
        godot::StringName sn =
            sig_info["name"].operator godot::StringName();
        godot::TypedArray<godot::Dictionary> conns =
            current->get_signal_connection_list(sn);
        append_connection_edges(conns, scene_root, true, edges, edge_keys,
                                neighbors);
      }
    }
    if (do_incoming) {
      godot::TypedArray<godot::Dictionary> in_conns =
          current->get_incoming_connections();
      append_connection_edges(in_conns, scene_root, false, edges, edge_keys,
                              neighbors);
    }

    for (godot::Node *n : neighbors) {
      int64_t id = static_cast<int64_t>(n->get_instance_id());
      if (depth + 1 <= max_depth && visited.insert(id).second)
        queue.emplace_back(n, depth + 1);
    }
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["root"] = mcp::JsonValue(describe_node(root_node, scene_root));
  inner["edges"] = std::move(edges);
  inner["edge_count"] = mcp::JsonValue(static_cast<int64_t>(edge_keys.size()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

} // namespace analyze_ops
} // namespace godot_autopilot
