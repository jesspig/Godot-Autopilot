#include "resources/resource_handlers.hpp"
#include "core/api_endpoints.hpp"
#include "core/config.hpp"
#include "core/export_guard.hpp"
#include "core/log_system.hpp"
#include "core/monitor.hpp"
#include "resources/resource_error_mapping.hpp"
#include "resources/skill_resources.hpp"
#include "tools/dispatch.hpp"
#include "tools/tool_invoke.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <mcp/McpError.hpp>
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
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {

namespace {

mcp::ReadResourceResult make_text_result(const std::string &uri,
                                         const std::string &mime_type,
                                         const std::string &text) {
  mcp::TextResourceContents trc;
  trc.uri = uri;
  trc.text = text;
  trc.mime_type = mime_type;
  mcp::ReadResourceResult rr;
  rr.contents = {mcp::ResourceContents{trc}};
  return rr;
}

mcp::ReadResourceResult make_json_result(const std::string &uri,
                                                const std::string &json) {
  return make_text_result(uri, "application/json", json);
}

struct ResourceTaskState {
  std::string json_str;
  std::string error_code;
  std::string error_text;
};

void set_error(ResourceTaskState &state, const std::string &msg) {
  mcp::JsonValue err(mcp::JsonValue::object_tag);
  err["error"] = mcp::JsonValue(msg);
  state.json_str = err.Dump(-1);
  state.error_text = msg;
}

struct ResourceTaskResult {
  std::string json_str;
};

template <typename Fn>
ResourceTaskResult run_resource_task(
    CommandQueue &queue, const std::string &uri,
    const std::shared_ptr<ResourceTaskState> &state, Fn task) {
  dispatch::BoundedWaitOutcome outcome = dispatch::run_bounded_on_main_thread(
      queue, uri, state, std::move(task), GDA_DISPATCH_WAIT_DEFAULT_MS);
  if (outcome.timed_out) {
    monitor::error_event("main_thread_timeout", "resource", uri);
    throw mcp::McpError(
        mcp::McpErrorCode::RequestTimeout,
        dispatch::main_thread_timeout_message(uri, outcome.waited_ms,
                                              outcome.cancelled),
        dispatch::main_thread_timeout_result(uri, outcome.waited_ms,
                                             outcome.cancelled));
  }
  if (!outcome.ok) {
    monitor::error_event("resource_read_failed", "resource", uri);
    throw mcp::McpError(mcp::McpErrorCode::InternalError, outcome.error);
  }
  if (!state->error_code.empty()) {
    monitor::error_event(state->error_code, "resource", uri);
    mcp::JsonValue data(mcp::JsonValue::object_tag);
    data["uri"] = mcp::JsonValue(uri);
    throw mcp::McpError(resource_error_code(state->error_code),
                        state->error_text, data);
  }
  ResourceTaskResult result;
  result.json_str = state->json_str;
  return result;
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

const std::vector<skill_gen::SkillSpec> &embedded_skills() {
  static const std::vector<skill_gen::SkillSpec> skills =
      skill_gen::all_skills();
  return skills;
}

int register_skill_resources(mcp::McpServer &server) {
  auto handler = [](const std::string &uri) -> mcp::ReadResourceResult {
    if (godot_autopilot::ExportGuard::is_exporting()) {
      throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
    }
    const skill_resources::ContentResult content =
        skill_resources::resolve_content(embedded_skills(), uri);
    if (!content.error.empty()) {
      monitor::error_event("resource_not_found", "resource", uri);
      mcp::JsonValue data(mcp::JsonValue::object_tag);
      data["uri"] = mcp::JsonValue(uri);
      throw mcp::McpError(mcp::McpErrorCode::InvalidParams, content.error, data);
    }
    monitor::data_flow(
        "resource_read", static_cast<int64_t>(content.text.size()),
        monitor::build_attrs({{"uri", uri},
                              {"request_id", monitor::current_request_id()}}));
    return make_text_result(uri, content.mime_type, content.text);
  };

  int registered = 0;
  server.RegisterResource(
      "skills", "godot://skills",
      mcp::ResourceOptions{}
          .Description("Skill catalog: every godot-autopilot skill book with "
                       "its description and file list")
          .MimeType("application/json"),
      handler);
  ++registered;

  for (const skill_gen::SkillSpec &spec : embedded_skills()) {
    const std::string skill_uri = "godot://skills/" + spec.name;
    server.RegisterResource("skill-" + spec.name, skill_uri,
                            mcp::ResourceOptions{}
                                .Description(spec.description)
                                .MimeType(skill_resources::mime_type_for_path(
                                    "SKILL.md")),
                            handler);
    ++registered;

    for (const skill_gen::SkillFile &file : spec.files) {
      server.RegisterResource(
          "skill-" + spec.name + "-" + file.relative_path,
          skill_uri + "/" + file.relative_path,
          mcp::ResourceOptions{}
              .Description(spec.description)
              .MimeType(skill_resources::mime_type_for_path(
                  file.relative_path)),
          handler);
      ++registered;
    }
  }
  return registered;
}

} // namespace

void register_all_resources(mcp::McpServer &server, CommandQueue &queue) {
  server.RegisterResource(
      "engine-version", "godot://engine/version",
      mcp::ResourceOptions{}
          .Description("Godot engine version information")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const tools::TraceContext ctx = tools::capture_trace_context();
        auto state = std::make_shared<ResourceTaskState>();
        ResourceTaskResult result = run_resource_task(
            queue, uri, state,
            [state, ctx](ResourceTaskState &st) {
              tools::ScopedTraceContext restore(ctx);
              auto *engine = godot::Engine::get_singleton();
              if (!engine) {
                set_error(st,"Engine singleton not available");
                st.error_code = "resource_read_failed";
                return;
              }
              godot::Dictionary v = engine->get_version_info();
              auto jv = VariantJson::serialize(v);
              st.json_str = jv.Dump(-1);
            });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(result.json_str.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, result.json_str);
      });

  server.RegisterResource(
      "endpoints", "godot://endpoints",
      mcp::ResourceOptions{}
          .Description("Endpoint table: instance identity, version, process "
                       "id, project path and the MCP/REST endpoints")
          .MimeType("application/json"),
      [](const std::string &uri) -> mcp::ReadResourceResult {
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const std::string payload = api_endpoints::endpoint_document_json();
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(payload.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, payload);
      });

  server.RegisterResource(
      "scene-tree", "godot://scene/tree",
      mcp::ResourceOptions{}
          .Description("Current edited scene node tree")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const tools::TraceContext ctx = tools::capture_trace_context();
        auto state = std::make_shared<ResourceTaskState>();
        ResourceTaskResult result = run_resource_task(
            queue, uri, state,
            [state, ctx](ResourceTaskState &st) {
              tools::ScopedTraceContext restore(ctx);
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(st,"Not in editor mode");
                st.error_code = "resource_read_failed";
                return;
              }
              auto *root = editor->get_edited_scene_root();
              if (!root) {
                set_error(st,"No scene open");
                st.error_code = "resource_not_found";
                return;
              }
              std::string root_prefix = util::to_std(root->get_path());
              mcp::JsonValue node_json(mcp::JsonValue::object_tag);
              node_to_json(root, root_prefix, node_json);
              st.json_str = node_json.Dump(-1);
            });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(result.json_str.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, result.json_str);
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
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const tools::TraceContext ctx = tools::capture_trace_context();
        auto state = std::make_shared<ResourceTaskState>();
        ResourceTaskResult result = run_resource_task(
            queue, uri, state,
            [state, path, ctx](ResourceTaskState &st) {
              tools::ScopedTraceContext restore(ctx);
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(st,"Not in editor mode");
                st.error_code = "resource_read_failed";
                return;
              }
              auto *root = editor->get_edited_scene_root();
              if (!root) {
                set_error(st,"No scene open");
                st.error_code = "resource_not_found";
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
                set_error(st,"Node not found: " + path);
                st.error_code = "resource_not_found";
                return;
              }

              std::string root_prefix = util::to_std(root->get_path());
              auto jv = node_detail_to_json(node, root_prefix);
              st.json_str = jv.Dump(-1);
            });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(result.json_str.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, result.json_str);
      });

  server.RegisterResource(
      "filesystem-tree", "godot://filesystem/tree",
      mcp::ResourceOptions{}
          .Description("Project file system structure")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const tools::TraceContext ctx = tools::capture_trace_context();
        auto state = std::make_shared<ResourceTaskState>();
        ResourceTaskResult result = run_resource_task(
            queue, uri, state,
            [state, ctx](ResourceTaskState &st) {
              tools::ScopedTraceContext restore(ctx);
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(st,"Not in editor mode");
                st.error_code = "resource_read_failed";
                return;
              }
              auto *fs = editor->get_resource_filesystem();
              if (!fs) {
                set_error(st,"File system not available");
                st.error_code = "resource_read_failed";
                return;
              }
              auto *root_dir = fs->get_filesystem();
              if (!root_dir) {
                set_error(st,"File system still scanning");
                st.error_code = "resource_read_failed";
                return;
              }

              mcp::JsonValue dir_json(mcp::JsonValue::object_tag);
              dir_to_json(root_dir, dir_json);
              st.json_str = dir_json.Dump(-1);
            });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(result.json_str.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, result.json_str);
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
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const tools::TraceContext ctx = tools::capture_trace_context();
        auto state = std::make_shared<ResourceTaskState>();
        ResourceTaskResult result = run_resource_task(
            queue, uri, state,
            [state, path, ctx](ResourceTaskState &st) {
              tools::ScopedTraceContext restore(ctx);
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(st,"Not in editor mode");
                st.error_code = "resource_read_failed";
                return;
              }
              auto *fs = editor->get_resource_filesystem();
              if (!fs) {
                set_error(st,"File system not available");
                st.error_code = "resource_read_failed";
                return;
              }

              godot::String gd_path = to_godot_path(path);

              auto *dir = fs->get_filesystem_path(gd_path);
              if (dir) {
                mcp::JsonValue dir_json(mcp::JsonValue::object_tag);
                dir_to_json(dir, dir_json);
                dir_json["path"] = mcp::JsonValue(path);
                st.json_str = dir_json.Dump(-1);
                return;
              }

              godot::String file_type = fs->get_file_type(gd_path);
              if (!file_type.is_empty()) {
                mcp::JsonValue file_json(mcp::JsonValue::object_tag);
                auto pos = path.rfind('/');
                std::string fname =
                    (pos != std::string::npos) ? path.substr(pos + 1) : path;
                file_json["name"] = mcp::JsonValue(fname);
                file_json["path"] = mcp::JsonValue(path);
                file_json["type"] = mcp::JsonValue(util::to_std(file_type));
                st.json_str = file_json.Dump(-1);
                return;
              }

              set_error(st,"Path not found in file system: " + path);
              st.error_code = "resource_not_found";
            });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(result.json_str.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, result.json_str);
      });

  server.RegisterResource(
      "editor-selection", "godot://editor/selection",
      mcp::ResourceOptions{}
          .Description("Currently selected nodes in the editor")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const tools::TraceContext ctx = tools::capture_trace_context();
        auto state = std::make_shared<ResourceTaskState>();
        ResourceTaskResult result = run_resource_task(
            queue, uri, state,
            [state, ctx](ResourceTaskState &st) {
              tools::ScopedTraceContext restore(ctx);
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(st,"Not in editor mode");
                st.error_code = "resource_read_failed";
                return;
              }
              auto *sel = editor->get_selection();
              if (!sel) {
                st.json_str = "[]";
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
              st.json_str = arr.Dump(-1);
            });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(result.json_str.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, result.json_str);
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
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const tools::TraceContext ctx = tools::capture_trace_context();
        auto state = std::make_shared<ResourceTaskState>();
        ResourceTaskResult result = run_resource_task(
            queue, uri, state,
            [state, key, ctx](ResourceTaskState &st) {
              tools::ScopedTraceContext restore(ctx);
              auto *editor = godot::EditorInterface::get_singleton();
              if (!editor) {
                set_error(st,"Not in editor mode");
                st.error_code = "resource_read_failed";
                return;
              }
              auto settings = editor->get_editor_settings();
              if (settings.is_null()) {
                set_error(st,"Editor settings not available");
                st.error_code = "resource_read_failed";
                return;
              }

              godot::String gd_key(key.c_str());
              if (!settings->has_setting(gd_key)) {
                set_error(st,"Setting not found: " + key);
                st.error_code = "resource_not_found";
                return;
              }

              godot::Variant val = settings->get_setting(gd_key);
              auto jv = VariantJson::serialize(val);
              st.json_str = jv.Dump(-1);
            });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(result.json_str.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, result.json_str);
      });

  server.RegisterResource(
      "log-recent", "godot://log/recent",
      mcp::ResourceOptions{}
          .Description("Recent log entries from the plugin's log system")
          .MimeType("application/json"),
      [&queue](const std::string &uri) -> mcp::ReadResourceResult {
        if (godot_autopilot::ExportGuard::is_exporting()) {
          throw mcp::McpError(mcp::McpErrorCode::HandlerError, dispatch::export_blocked_error_text());
        }
        const tools::TraceContext ctx = tools::capture_trace_context();
        auto state = std::make_shared<ResourceTaskState>();
        ResourceTaskResult result = run_resource_task(
            queue, uri, state,
            [state, ctx](ResourceTaskState &st) {
              tools::ScopedTraceContext restore(ctx);
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
              st.json_str = arr.Dump(-1);
            });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(result.json_str.size()),
            monitor::build_attrs(
                {{"uri", uri},
                 {"request_id", monitor::current_request_id()}}));
        return make_json_result(uri, result.json_str);
      });

  const int skill_resource_count = register_skill_resources(server);

  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Resources,
      "8 resource handlers and " + std::to_string(skill_resource_count) +
          " skill resources registered");
}

} // namespace godot_autopilot
