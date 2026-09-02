#include "resources/resource_handlers.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <ctime>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_file_system_directory.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <map>
#include <string>

namespace godot_autopilot {

namespace {

mcp::ReadResourceResult make_json_result(const std::string &uri,
                                                const std::string &json) {
  mcp::TextResourceContents trc;
  trc.uri = uri;
  trc.text = json;
  trc.mime_type = "application/json";
  mcp::ReadResourceResult rr;
  rr.contents = {mcp::ResourceContents{trc}};
  return rr;
}

void set_error(std::string &json_str, const std::string &msg) {
  mcp::JsonValue err(mcp::JsonValue::object_tag);
  err["error"] = mcp::JsonValue(msg);
  json_str = err.Dump(-1);
}

const char *level_to_string(LogLevel level) {
  switch (level) {
  case LogLevel::Debug:
    return "debug";
  case LogLevel::Info:
    return "info";
  case LogLevel::Warning:
    return "warning";
  case LogLevel::Error:
    return "error";
  default:
    return "unknown";
  }
}

const char *category_to_string(LogCategory cat) {
  switch (cat) {
  case LogCategory::System:
    return "system";
  case LogCategory::Transport:
    return "transport";
  case LogCategory::Tools:
    return "tools";
  case LogCategory::Resources:
    return "resources";
  case LogCategory::Prompts:
    return "prompts";
  default:
    return "unknown";
  }
}

std::string compute_relative_path(godot::Node *node,
                                         const std::string &root_prefix) {
  std::string abs_path = util::to_std(node->get_path());
  if (abs_path == root_prefix) {
    return util::to_std(node->get_name());
  }
  if (!root_prefix.empty() && abs_path.find(root_prefix + "/") == 0) {
    return abs_path.substr(root_prefix.size() + 1);
  }
  return abs_path;
}

void node_to_json(godot::Node *node, const std::string &root_prefix,
                         mcp::JsonValue &j) {
  j["name"] = mcp::JsonValue(util::to_std(node->get_name()));
  j["class"] = mcp::JsonValue(util::to_std(node->get_class()));
  j["path"] = mcp::JsonValue(compute_relative_path(node, root_prefix));
  j["children"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  auto children = node->get_children();
  for (int i = 0; i < children.size(); i++) {
    auto *child = godot::Object::cast_to<godot::Node>(children[i]);
    if (child) {
      mcp::JsonValue child_j(mcp::JsonValue::object_tag);
      node_to_json(child, root_prefix, child_j);
      j["children"].PushBack(std::move(child_j));
    }
  }
}

mcp::JsonValue node_detail_to_json(godot::Node *node,
                                          const std::string &root_prefix) {
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["name"] = mcp::JsonValue(util::to_std(node->get_name()));
  j["class"] = mcp::JsonValue(util::to_std(node->get_class()));
  j["path"] = mcp::JsonValue(compute_relative_path(node, root_prefix));

  mcp::JsonValue props(mcp::JsonValue::object_tag);
  auto prop_list = node->get_property_list();
  for (int i = 0; i < prop_list.size(); i++) {
    godot::Dictionary prop = prop_list[i];
    godot::String prop_name = prop["name"];
    int usage = static_cast<int>(prop["usage"]);

    int skip_mask = godot::PROPERTY_USAGE_INTERNAL | godot::PROPERTY_USAGE_GROUP |
                    godot::PROPERTY_USAGE_CATEGORY | godot::PROPERTY_USAGE_SUBGROUP;
    if (usage & skip_mask)
      continue;

    std::string name_str = util::to_std(prop_name);
    if (name_str.empty())
      continue;

    godot::Variant val = node->get(prop_name);
    props[name_str] = godot_autopilot::VariantJson::serialize(val);
  }
  j["properties"] = std::move(props);

  j["children"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  auto children = node->get_children();
  for (int i = 0; i < children.size(); i++) {
    auto *child = godot::Object::cast_to<godot::Node>(children[i]);
    if (child) {
      mcp::JsonValue child_j(mcp::JsonValue::object_tag);
      child_j["name"] = mcp::JsonValue(util::to_std(child->get_name()));
      child_j["class"] = mcp::JsonValue(util::to_std(child->get_class()));
      j["children"].PushBack(std::move(child_j));
    }
  }

  auto groups = node->get_groups();
  mcp::JsonValue groups_arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < groups.size(); i++) {
    godot::String gs = godot::String(groups[i]);
    groups_arr.PushBack(mcp::JsonValue(util::to_std(gs)));
  }
  j["groups"] = std::move(groups_arr);

  godot::Variant script_var = node->get_script();
  if (script_var.get_type() != godot::Variant::NIL) {
    auto *obj = script_var.operator godot::Object *();
    if (obj) {
      mcp::JsonValue script_j(mcp::JsonValue::object_tag);
      script_j["class"] = mcp::JsonValue(util::to_std(obj->get_class()));
      auto *res = godot::Object::cast_to<godot::Resource>(obj);
      if (res && !res->get_path().is_empty()) {
        script_j["path"] = mcp::JsonValue(util::to_std(res->get_path()));
      }
      j["script"] = std::move(script_j);
    }
  }

  return j;
}

void dir_to_json(godot::EditorFileSystemDirectory *dir,
                        mcp::JsonValue &j) {
  j["name"] = mcp::JsonValue(util::to_std(dir->get_name()));
  j["path"] = mcp::JsonValue(util::to_std(dir->get_path()));

  j["subdirs"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  int subdir_count = dir->get_subdir_count();
  for (int i = 0; i < subdir_count; i++) {
    auto *subdir = dir->get_subdir(i);
    if (subdir) {
      mcp::JsonValue subdir_j(mcp::JsonValue::object_tag);
      dir_to_json(subdir, subdir_j);
      j["subdirs"].PushBack(std::move(subdir_j));
    }
  }

  j["files"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  int file_count = dir->get_file_count();
  for (int i = 0; i < file_count; i++) {
    mcp::JsonValue file_j(mcp::JsonValue::object_tag);
    file_j["name"] = mcp::JsonValue(util::to_std(dir->get_file(i)));
    file_j["path"] = mcp::JsonValue(util::to_std(dir->get_file_path(i)));
    godot::StringName type = dir->get_file_type(i);
    file_j["type"] = mcp::JsonValue(util::to_std(godot::String(type)));
    j["files"].PushBack(std::move(file_j));
  }
}

godot::String to_godot_path(const std::string &path) {
  if (path.find("res://") == 0) {
    return godot::String(path.c_str());
  }
  return godot::String(("res://" + path).c_str());
}

} // namespace

void register_all_resources(mcp::McpServer &server, CommandQueue &queue) {
  server.RegisterResource(
      "engine-version", "godot://engine/version",
      mcp::ResourceOptions{}
          .Description("Godot engine version information")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        std::string json_str;
         queue.execute_sync([&json_str]() {
              auto *engine = godot::Engine::get_singleton();
              if (!engine) {
                set_error(json_str, "Engine singleton not available");
                return;
              }
              godot::Dictionary v = engine->get_version_info();
              auto jv = VariantJson::serialize(v);
              json_str = jv.Dump(-1);
             });
        return make_json_result(uri, json_str);
      });

  server.RegisterResource(
      "scene-tree", "godot://scene/tree",
      mcp::ResourceOptions{}
          .Description("Current edited scene node tree")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        std::string json_str;
         queue.execute_sync([&json_str]() {
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(json_str, "Not in editor mode");
                return;
              }
              auto *root = editor->get_edited_scene_root();
              if (!root) {
                set_error(json_str, "No scene open");
                return;
              }
              std::string root_prefix = util::to_std(root->get_path());
              mcp::JsonValue result(mcp::JsonValue::object_tag);
              node_to_json(root, root_prefix, result);
              json_str = result.Dump(-1);
             });
        return make_json_result(uri, json_str);
      });

  server.RegisterResourceTemplate(
      "scene-node", "godot://scene/{path}",
      mcp::ResourceOptions{}
          .Description("Get properties of a scene node by path")
          .MimeType("application/json"),
      [&queue](const std::string &uri,
               const std::map<std::string, std::string> &vars)
          -> mcp::ReadResourceResult {
        std::string path = vars.at("path");
        std::string json_str;
         queue.execute_sync([&json_str, &path]() {
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(json_str, "Not in editor mode");
                return;
              }
              auto *root = editor->get_edited_scene_root();
              if (!root) {
                set_error(json_str, "No scene open");
                return;
              }

              std::string clean = path;
              if (!clean.empty() && clean[0] == '/')
                clean = clean.substr(1);

              godot::Node *node = nullptr;
              if (clean.empty() || clean == util::to_std(root->get_name())) {
                node = root;
              } else {
                godot::NodePath np(godot::String(clean.c_str()));
                node = root->get_node_or_null(np);
              }

              if (!node) {
                set_error(json_str, "Node not found: " + path);
                return;
              }

              std::string root_prefix = util::to_std(root->get_path());
              auto jv = node_detail_to_json(node, root_prefix);
              json_str = jv.Dump(-1);
             });
        return make_json_result(uri, json_str);
      });

  server.RegisterResource(
      "filesystem-tree", "godot://filesystem/tree",
      mcp::ResourceOptions{}
          .Description("Project file system structure")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        std::string json_str;
         queue.execute_sync([&json_str]() {
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(json_str, "Not in editor mode");
                return;
              }
              auto *fs = editor->get_resource_filesystem();
              if (!fs) {
                set_error(json_str, "File system not available");
                return;
              }
              auto *root_dir = fs->get_filesystem();
              if (!root_dir) {
                set_error(json_str, "File system still scanning");
                return;
              }

              mcp::JsonValue result(mcp::JsonValue::object_tag);
              dir_to_json(root_dir, result);
              json_str = result.Dump(-1);
             });
        return make_json_result(uri, json_str);
      });

  server.RegisterResourceTemplate(
      "filesystem-path", "godot://filesystem/{path}",
      mcp::ResourceOptions{}
          .Description("Get file info or directory listing by path")
          .MimeType("application/json"),
      [&queue](const std::string &uri,
               const std::map<std::string, std::string> &vars)
          -> mcp::ReadResourceResult {
        std::string path = vars.at("path");
        std::string json_str;
         queue.execute_sync([&json_str, &path]() {
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(json_str, "Not in editor mode");
                return;
              }
              auto *fs = editor->get_resource_filesystem();
              if (!fs) {
                set_error(json_str, "File system not available");
                return;
              }

              godot::String gd_path = to_godot_path(path);

              auto *dir = fs->get_filesystem_path(gd_path);
              if (dir) {
                mcp::JsonValue result(mcp::JsonValue::object_tag);
                dir_to_json(dir, result);
                result["path"] = mcp::JsonValue(path);
                json_str = result.Dump(-1);
                return;
              }

              godot::String file_type = fs->get_file_type(gd_path);
              if (!file_type.is_empty()) {
                mcp::JsonValue result(mcp::JsonValue::object_tag);
                auto pos = path.rfind('/');
                std::string fname =
                    (pos != std::string::npos) ? path.substr(pos + 1) : path;
                result["name"] = mcp::JsonValue(fname);
                result["path"] = mcp::JsonValue(path);
                result["type"] = mcp::JsonValue(util::to_std(file_type));
                json_str = result.Dump(-1);
                return;
              }

              set_error(json_str, "Path not found in file system: " + path);
             });
        return make_json_result(uri, json_str);
      });

  server.RegisterResource(
      "editor-selection", "godot://editor/selection",
      mcp::ResourceOptions{}
          .Description("Currently selected nodes in the editor")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        std::string json_str;
         queue.execute_sync([&json_str]() {
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(json_str, "Not in editor mode");
                return;
              }
              auto *sel = editor->get_selection();
              if (!sel) {
                json_str = "[]";
                return;
              }

              auto nodes = sel->get_selected_nodes();
              auto *root = editor->get_edited_scene_root();
              std::string root_prefix;
              if (root)
                root_prefix = util::to_std(root->get_path());

              mcp::JsonValue arr(mcp::JsonValue::array_tag);
              for (int i = 0; i < nodes.size(); i++) {
                auto *node = godot::Object::cast_to<godot::Node>(nodes[i]);
                if (!node)
                  continue;

                mcp::JsonValue item(mcp::JsonValue::object_tag);
                item["name"] = mcp::JsonValue(util::to_std(node->get_name()));
                item["class"] = mcp::JsonValue(util::to_std(node->get_class()));
                item["path"] =
                    mcp::JsonValue(compute_relative_path(node, root_prefix));
                arr.PushBack(std::move(item));
              }
              json_str = arr.Dump(-1);
             });
        return make_json_result(uri, json_str);
      });

  server.RegisterResourceTemplate(
      "editor-setting", "godot://editor/settings/{key}",
      mcp::ResourceOptions{}
          .Description("Get an editor setting value by key")
          .MimeType("application/json"),
      [&queue](const std::string &uri,
               const std::map<std::string, std::string> &vars)
          -> mcp::ReadResourceResult {
        std::string key = vars.at("key");
        std::string json_str;
         queue.execute_sync([&json_str, &key]() {
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(json_str, "Not in editor mode");
                return;
              }
              auto settings = editor->get_editor_settings();
              if (settings.is_null()) {
                set_error(json_str, "Editor settings not available");
                return;
              }

              godot::String gd_key(key.c_str());
              if (!settings->has_setting(gd_key)) {
                set_error(json_str, "Setting not found: " + key);
                return;
              }

              godot::Variant val = settings->get_setting(gd_key);
              auto jv = VariantJson::serialize(val);
              json_str = jv.Dump(-1);
             });
        return make_json_result(uri, json_str);
      });

  server.RegisterResource(
      "log-recent", "godot://log/recent",
      mcp::ResourceOptions{}
          .Description("Recent log entries from the plugin's log system")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        std::string json_str;
         queue.execute_sync([&json_str]() {
              auto &log = LogSystem::instance();
              auto entries = log.query_recent(50);

              size_t count = entries.size();

              mcp::JsonValue arr(mcp::JsonValue::array_tag);
              for (size_t i = 0; i < count; i++) {
                const auto &entry = entries[i];
                mcp::JsonValue item(mcp::JsonValue::object_tag);
                item["level"] = mcp::JsonValue(level_to_string(entry.level));
                item["category"] =
                    mcp::JsonValue(category_to_string(entry.category));
                item["message"] = mcp::JsonValue(entry.message);

                std::time_t tt =
                    std::chrono::system_clock::to_time_t(entry.timestamp);
                char buf[32] = {0};
                std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ",
                              std::gmtime(&tt));
                item["timestamp"] = mcp::JsonValue(buf);

                arr.PushBack(std::move(item));
              }
              json_str = arr.Dump(-1);
             });
        return make_json_result(uri, json_str);
      });

  LogSystem::instance().log(LogLevel::Info, LogCategory::Resources,
                            "8 resource handlers registered");
}

} // namespace godot_autopilot
