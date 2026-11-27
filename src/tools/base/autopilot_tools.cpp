#include "tools/base/autopilot_tools.hpp"

#include "core/path_util.hpp"
#include "util/input_guards.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "core/command_queue.hpp"
#include "core/config.hpp"
#include "core/export_guard.hpp"
#include "core/observability/log_system.hpp"
#include "core/observability/monitor.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/dynamic_spec_store.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/tool_base.hpp"
#include "tools/base/tool_invoke.hpp"
#include "tools/base/tool_spec.hpp"
#include "util/dotnet_env.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"

namespace godot_autopilot {

namespace {

struct DynamicTarget;

std::vector<int64_t> &handle_store() {
  static std::vector<int64_t> handles;
  return handles;
}

std::unordered_map<int64_t, std::shared_ptr<DynamicTarget>> &target_store() {
  static std::unordered_map<int64_t, std::shared_ptr<DynamicTarget>> targets;
  return targets;
}

std::atomic<int64_t> &next_handle() {
  static std::atomic<int64_t> counter{1};
  return counter;
}

struct DynamicTarget {
  godot::Callable callable;
  godot::Ref<godot::RefCounted> owner;
  godot::Node *node_owner = nullptr;
  uint64_t node_owner_id = 0;
  std::atomic<bool> active{true};
  std::string name;
};

std::string string_field(const godot::Dictionary &dict, const char *key,
                         const std::string &fallback) {
  godot::Variant value = dict.get(key, godot::Variant());
  if (value.get_type() != godot::Variant::STRING &&
      value.get_type() != godot::Variant::STRING_NAME) {
    return fallback;
  }
  return util::to_std(value.operator godot::String());
}

int64_t reject_registration(const std::string &message) {
  godot::UtilityFunctions::push_error(
      godot::String::utf8(message.c_str()));
  LogSystem::instance().log_detailed(LogLevel::Warning, LogCategory::Tools,
                                     "register_tool rejected: " + message,
                                     "reason=" + message);
  return -1;
}

godot_autopilot::util::JsonValue dynamic_error(const std::string &name, const std::string &message,
                             const std::string &code) {
  godot_autopilot::util::JsonValue result = util::error_json(message);
  result["code"] = godot_autopilot::util::JsonValue(code);
  result["tool"] = godot_autopilot::util::JsonValue(name);
  result["retryable"] = godot_autopilot::util::JsonValue(false);
  return result;
}

godot_autopilot::util::JsonValue invoke_dynamic_target(
    const std::shared_ptr<DynamicTarget> &target, const godot_autopilot::util::JsonValue &args) {
  if (!target->active.load()) {
    return dynamic_error(target->name,
                         "user tool '" + target->name +
                             "' is no longer registered",
                         "tool_unregistered");
  }
  if (!dynamic_specs::enabled().load()) {
    return dynamic_error(target->name, "user tools are disabled",
                         "tools_disabled");
  }
  if (!target->callable.is_valid() || target->callable.get_object() == nullptr) {
    return dynamic_error(target->name,
                         "user tool '" + target->name +
                             "' is no longer valid: its target object was "
                             "freed — re-register it",
                         "invalid_target");
  }
  godot::Object *bound = target->callable.get_object();
  if (godot::ObjectDB::get_instance(bound->get_instance_id()) != bound) {
    return dynamic_error(target->name,
                         "user tool '" + target->name +
                             "' is no longer valid: its target object was "
                             "freed — re-register it",
                         "invalid_target");
  }
  if (target->node_owner_id != 0 &&
      godot::ObjectDB::get_instance(target->node_owner_id) !=
          target->node_owner) {
    return dynamic_error(target->name,
                         "user tool '" + target->name +
                             "' is no longer valid: its target node was "
                             "freed — re-register it",
                         "invalid_target");
  }

  godot::Dictionary args_dict;
  godot::Variant args_variant = VariantJson::deserialize(args, "dictionary");
  if (args_variant.get_type() == godot::Variant::DICTIONARY) {
    args_dict = args_variant.operator godot::Dictionary();
  }
  godot::Array call_args;
  call_args.push_back(args_dict);

  godot::Variant returned = target->callable.callv(call_args);
  if (returned.get_type() == godot::Variant::NIL) {
    return util::error_json("user tool returned null");
  }

  godot_autopilot::util::JsonValue serialized = VariantJson::serialize(returned);
  if (returned.get_type() == godot::Variant::DICTIONARY && serialized.IsObject() &&
      serialized.Find("error") != nullptr) {
    return serialized;
  }
  godot_autopilot::util::JsonValue wrapped(godot_autopilot::util::JsonValue::object_tag);
  wrapped["result"] = std::move(serialized);
  return wrapped;
}

godot_autopilot::util::JsonValue run_dynamic_handler(
    const std::shared_ptr<DynamicTarget> &target, const godot_autopilot::util::JsonValue &args) {
  if (ExportGuard::is_exporting()) {
    return dispatch::export_blocked_json_result();
  }
  if (!runtime_ops::has_editor_queue()) {
    return dispatch::queue_unavailable_result(
        target->name,
        "the plugin has not attached the editor main thread queue yet "
        "(startup) or has already detached it (shutdown)");
  }
  if (!get_editor_queue().is_main_thread()) {
    const dispatch::DepthContext ctx = dispatch::capture_depth_context();
    auto state = std::make_shared<godot_autopilot::util::JsonValue>();
    dispatch::BoundedWaitOutcome outcome =
        dispatch::run_bounded_on_main_thread(
            get_editor_queue(), "user tool dispatch", state,
            [target, args, ctx](godot_autopilot::util::JsonValue &result) {
              dispatch::ScopedDepthContext restore(ctx);
              if (ExportGuard::is_exporting()) {
                result = dispatch::export_blocked_json_result();
                return;
              }
              result = invoke_dynamic_target(target, args);
            },
            GDA_DISPATCH_WAIT_DEFAULT_MS);
    if (!outcome.ok) {
      return util::error_json(outcome.error);
    }
    return *state;
  }
  try {
    return invoke_dynamic_target(target, args);
  } catch (const std::exception &e) {
    return util::error_json(std::string("user tool invocation failed: ") +
                            e.what());
  } catch (...) {
    return util::error_json("user tool invocation failed: unknown exception");
  }
}

constexpr int kRescanMaxFiles = 256;
constexpr size_t kRescanMaxDepth = 8;
constexpr const char *kRescanMethodName = "register_autopilot_tools";

godot::Dictionary rescan_error(const std::string &message) {
  godot::Dictionary out;
  out["error"] = godot::String::utf8(message.c_str());
  return out;
}

bool has_gd_suffix(const std::string &filename) {
  if (filename.size() < 4) {
    return false;
  }
  const std::string lower = util::ascii_lowercase(filename);
  return lower.compare(lower.size() - 3, 3, ".gd") == 0;
}

std::set<std::string> dynamic_tool_name_set() {
  std::set<std::string> names;
  std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
  for (const ToolSpec &spec : dynamic_specs::store()) {
    names.insert(spec.name);
  }
  return names;
}

void collect_rescan_files(const std::string &root,
                          std::vector<std::string> &out) {
  std::vector<std::pair<std::string, size_t>> pending;
  pending.emplace_back(root, 0);
  while (!pending.empty() && static_cast<int>(out.size()) < kRescanMaxFiles) {
    const std::pair<std::string, size_t> top = pending.back();
    pending.pop_back();
    godot::Ref<godot::DirAccess> dir =
        godot::DirAccess::open(godot::String::utf8(top.first.c_str()));
    if (dir.is_null()) {
      continue;
    }
    dir->list_dir_begin();
    godot::String entry = dir->get_next();
    while (entry != godot::String()) {
      if (entry != "." && entry != "..") {
        const std::string name = util::to_std(entry);
        if (!name.empty() && name[0] != '.') {
          const std::string full = join_path(top.first, name);
          if (dir->current_is_dir()) {
            if (top.second < kRescanMaxDepth) {
              pending.emplace_back(full, top.second + 1);
            }
          } else if (has_gd_suffix(name) &&
                     static_cast<int>(out.size()) < kRescanMaxFiles) {
            out.push_back(full);
          }
        }
      }
      entry = dir->get_next();
    }
    dir->list_dir_end();
  }
}

void record_rescan_error(godot::Array &errors, const std::string &file,
                         const std::string &message) {
  godot::Dictionary item;
  item["file"] = godot::String::utf8(file.c_str());
  item["error"] = godot::String::utf8(message.c_str());
  errors.push_back(item);
}

void delete_released_node(uint64_t object_id) {
  auto *object = godot::ObjectDB::get_instance(object_id);
  auto *node = object ? godot::Object::cast_to<godot::Node>(object) : nullptr;
  if (node != nullptr && node->get_parent() == nullptr) {
    memdelete(node);
  }
}

} // namespace

void AutopilotTools::rescan_single_file(const std::string &path,
                                        godot::Array &errors) {
  try {
    auto *loader = godot::ResourceLoader::get_singleton();
    if (loader == nullptr) {
      record_rescan_error(errors, path, "rescan: ResourceLoader not available");
      return;
    }
    const godot::Ref<godot::Resource> script =
        loader->load(godot::String::utf8(path.c_str()), "GDScript");
    if (script.is_null()) {
      record_rescan_error(errors, path, "rescan: failed to load script");
      return;
    }
    godot::Variant instance;
    try {
      instance = script->call(godot::StringName("new"));
    } catch (const std::exception &e) {
      record_rescan_error(errors, path,
                           std::string("rescan: failed to instantiate: ") +
                               e.what());
      return;
    } catch (...) {
      record_rescan_error(errors, path,
                           "rescan: failed to instantiate: unknown exception");
      return;
    }
    if (instance.get_type() == godot::Variant::NIL) {
      record_rescan_error(errors, path,
                           "rescan: failed to instantiate (null instance)");
      return;
    }
    godot::Object *obj = instance;
    if (obj == nullptr) {
      record_rescan_error(errors, path,
                           "rescan: instantiated value is not an Object");
      return;
    }
    const godot::StringName method(kRescanMethodName);
    if (!obj->has_method(method)) {
      record_rescan_error(
          errors, path,
          "rescan: no register_autopilot_tools(api) method; skipped");
    } else {
      try {
        obj->call(method, godot::Variant(this));
      } catch (const std::exception &e) {
        record_rescan_error(errors, path,
                             std::string("rescan: register call failed: ") +
                                 e.what());
      } catch (...) {
        record_rescan_error(errors, path,
                             "rescan: register call failed: unknown exception");
      }
    }
    if (auto *node = godot::Object::cast_to<godot::Node>(obj)) {
      const uint64_t object_id = node->get_instance_id();
      if (node->get_parent() == nullptr && !node_owners_.owns(object_id)) {
        memdelete(node);
      }
    } else if (godot::Object::cast_to<godot::RefCounted>(obj) == nullptr) {
      memdelete(obj);
    }
  } catch (const std::exception &e) {
    record_rescan_error(errors, path, std::string("rescan: ") + e.what());
  } catch (...) {
    record_rescan_error(errors, path, "rescan: unknown exception");
  }
}

AutopilotTools::~AutopilotTools() {
  {
    std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
    for (auto &entry : target_store()) {
      entry.second->active.store(false);
    }
    target_store().clear();
    dynamic_specs::store().clear();
    handle_store().clear();
  }
  refresh_dynamic_tools();
  for (const uint64_t object_id : node_owners_.release_all()) {
    delete_released_node(object_id);
  }
}

int64_t AutopilotTools::register_tool(const godot::Dictionary &definition,
                                      const godot::Callable &callable) {
  const std::string name = string_field(definition, "name", "");
  if (!dynamic_specs::enabled().load()) {
    return reject_registration("register_tool: user tools are disabled");
  }
  if (name.empty()) {
    return reject_registration(
        "register_tool: definition.name must be a non-empty string");
  }
  if (!callable.is_valid() || callable.get_object() == nullptr) {
    return reject_registration(
        "register_tool: callable must be a valid bound method on a live "
        "object");
  }
  {
    std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
    for (const ToolSpec &spec : dynamic_specs::store()) {
      if (spec.name == name) {
        return reject_registration("register_tool: tool '" + name +
                                   "' is already registered");
      }
    }
  }
  if (auto registry = get_active_registry()) {
    if (registry->find_any(name) != nullptr) {
      return reject_registration("register_tool: tool '" + name +
                                 "' collides with a built-in tool");
    }
  }

  ToolSpec spec;
  spec.name = name;
  spec.description = string_field(definition, "description", "");
  spec.category = string_field(definition, "category", "User");

  godot::Variant tags_variant = definition.get("tags", godot::Variant());
  if (tags_variant.get_type() == godot::Variant::ARRAY) {
    const godot::Array tags_array = tags_variant.operator godot::Array();
    for (int64_t i = 0; i < tags_array.size(); ++i) {
      spec.tags.push_back(util::to_std(tags_array[i].operator godot::String()));
    }
  }

  const std::string side_effect_text =
      string_field(definition, "side_effect", "");
  if (!parse_side_effect(side_effect_text, spec.side_effect)) {
    return reject_registration("register_tool: unknown side_effect '" +
                               side_effect_text + "'");
  }

  godot::Variant params_variant = definition.get("params", godot::Variant());
  if (params_variant.get_type() != godot::Variant::NIL &&
      params_variant.get_type() != godot::Variant::ARRAY) {
    return reject_registration("register_tool: params must be an array");
  }
  if (params_variant.get_type() == godot::Variant::ARRAY) {
    const godot::Array params_array = params_variant.operator godot::Array();
    for (int64_t i = 0; i < params_array.size(); ++i) {
      if (params_array[i].get_type() != godot::Variant::DICTIONARY) {
        return reject_registration(
            "register_tool: params[" + std::to_string(i) +
            "] must be a dictionary");
      }
      const godot::Dictionary item =
          params_array[i].operator godot::Dictionary();
      ParamSpec param;
      param.name = string_field(item, "name", "");
      if (param.name.empty()) {
        return reject_registration("register_tool: params[" +
                                   std::to_string(i) +
                                   "].name must be a non-empty string");
      }
      godot::Variant type_variant = item.get("type", godot::Variant());
      if (type_variant.get_type() != godot::Variant::NIL &&
          type_variant.get_type() != godot::Variant::STRING &&
          type_variant.get_type() != godot::Variant::STRING_NAME) {
        return reject_registration("register_tool: params[" +
                                   std::to_string(i) +
                                   "].type must be a string");
      }
      param.type = string_field(item, "type", "string");
      param.description = string_field(item, "description", "");
      godot::Variant required = item.get("required", godot::Variant());
      param.required = required.get_type() == godot::Variant::BOOL &&
                       required.operator bool();
      spec.params.push_back(std::move(param));
    }
  }
  std::string params_error;
  if (!schema::validate_param_defs(spec.params, params_error)) {
    return reject_registration("register_tool: invalid params: " + params_error);
  }

  spec.flags = tool_flags::kDynamic;

  auto target = std::make_shared<DynamicTarget>();
  target->callable = callable;
  target->name = name;
  godot::Object *bound = callable.get_object();
  auto *ref_counted = godot::Object::cast_to<godot::RefCounted>(bound);
  auto *node = godot::Object::cast_to<godot::Node>(bound);
  if (ref_counted == nullptr && node == nullptr) {
    return reject_registration(
        "register_tool: callable target must be a Node or RefCounted object");
  }
  if (ref_counted != nullptr) {
    target->owner = godot::Ref<godot::RefCounted>(ref_counted);
  }
  if (node != nullptr) {
    target->node_owner = node;
    target->node_owner_id = node->get_instance_id();
    if (node->get_parent() == nullptr) {
      node_owners_.retain(target->node_owner_id);
    }
  }
  spec.handler = [target](const godot_autopilot::util::JsonValue &args) {
    return run_dynamic_handler(target, args);
  };

  const int64_t handle = next_handle().fetch_add(1);
  const std::string registration_detail =
      "tool=" + name + " handle=" + std::to_string(handle) +
      " params=" + std::to_string(spec.params.size()) +
      " tags=" + std::to_string(spec.tags.size()) +
      " category=" + spec.category;
  {
    std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
    dynamic_specs::store().push_back(std::move(spec));
    handle_store().push_back(handle);
    target_store()[handle] = target;
  }
  LogSystem::instance().log_detailed(
      LogLevel::Info, LogCategory::Tools,
      "user tool '" + name + "' registered (handle " +
          std::to_string(handle) + ")",
      registration_detail);
  monitor::lifecycle(
      "user_tool_register",
      monitor::build_attrs({{"tool", name}, {"enabled", "true"}}));
  refresh_dynamic_tools();
  return handle;
}

godot::Dictionary AutopilotTools::register_csharp_tool(
    const godot::Dictionary &definition, const godot::Callable &callable) {
  const std::string name = string_field(definition, "name", "");
  if (name.empty()) {
    godot::Dictionary out = rescan_error(
        "register_csharp_tool: definition.name must be a non-empty string");
    out["tool_name"] = godot::String::utf8(name.c_str());
    out["handle"] = static_cast<int64_t>(-1);
    return out;
  }
  bool is_dotnet_editor = false;
  bool assembly_loaded = false;
  dotnet_env::DotnetProbe probe = dotnet_env::probe_current();
  is_dotnet_editor = dotnet_env::is_dotnet_environment(probe);
  assembly_loaded = probe.csharp_script_class;
  if (!is_dotnet_editor) {
    godot::Dictionary out = rescan_error(
        "register_csharp_tool: tool '" + name +
        "' not registered: editor is NOT a .NET build "
        "(CSharpScript class missing and no mono/dotnet feature reported), "
        "so C# callables cannot load in this editor; open the project in the "
        ".NET editor build and retry");
    out["tool_name"] = godot::String::utf8(name.c_str());
    out["handle"] = static_cast<int64_t>(-1);
    return out;
  }
  if (!assembly_loaded) {
    godot::Dictionary out = rescan_error(
        "register_csharp_tool: tool '" + name +
        "' not registered: C# assembly not loaded "
        "(CSharpScript class missing); run build_csharp_assembly with "
        "wait=true and confirm via get_debugger_log before registering");
    out["tool_name"] = godot::String::utf8(name.c_str());
    out["handle"] = static_cast<int64_t>(-1);
    return out;
  }
  {
    const std::set<std::string> existing = dynamic_tool_name_set();
    if (existing.find(name) != existing.end()) {
      godot::Dictionary out = rescan_error(
          "register_csharp_tool: tool '" + name +
          "' is already registered (idempotent reject; unregister first "
          "or reuse handle)");
      out["tool_name"] = godot::String::utf8(name.c_str());
      out["handle"] = static_cast<int64_t>(-1);
      return out;
    }
  }
  if (auto registry = get_active_registry()) {
    if (registry->find_any(name) != nullptr) {
      godot::Dictionary out = rescan_error(
          "register_csharp_tool: tool '" + name +
          "' collides with a built-in tool");
      out["tool_name"] = godot::String::utf8(name.c_str());
      out["handle"] = static_cast<int64_t>(-1);
      return out;
    }
  }
  const int64_t handle = register_tool(definition, callable);
  if (handle <= 0) {
    godot::Dictionary out = rescan_error(
        "register_csharp_tool: registration rejected for tool '" + name +
        "'; see editor log for register_tool reason");
    out["tool_name"] = godot::String::utf8(name.c_str());
    out["handle"] = static_cast<int64_t>(-1);
    return out;
  }
  godot::Dictionary out;
  out["tool_name"] = godot::String::utf8(name.c_str());
  out["handle"] = handle;
  out["call_result"] = get_tool(godot::String::utf8(name.c_str()));
  return out;
}

bool AutopilotTools::unregister_tool(int64_t handle) {
  if (handle <= 0) {
    return false;
  }
  bool removed = false;
  std::string removed_name;
  std::shared_ptr<DynamicTarget> removed_target;
  {
    std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
    std::vector<int64_t> &handles = handle_store();
    auto it = std::find(handles.begin(), handles.end(), handle);
    if (it != handles.end()) {
      const size_t index =
          static_cast<size_t>(std::distance(handles.begin(), it));
      handles.erase(it);
      std::vector<ToolSpec> &specs = dynamic_specs::store();
      removed_name = specs[index].name;
      specs.erase(specs.begin() + static_cast<std::ptrdiff_t>(index));
      auto target_it = target_store().find(handle);
      if (target_it != target_store().end()) {
        removed_target = std::move(target_it->second);
        target_store().erase(target_it);
      }
      removed = true;
    }
  }
  if (removed) {
    if (removed_target) {
      removed_target->active.store(false);
    }
    LogSystem::instance().log_detailed(
        LogLevel::Info, LogCategory::Tools,
        "user tool handle " + std::to_string(handle) + " unregistered",
        "tool=" + removed_name + " handle=" + std::to_string(handle));
    monitor::lifecycle(
        "user_tool_unregister",
        monitor::build_attrs({{"tool", removed_name}, {"enabled", "false"}}));
    refresh_dynamic_tools();
    if (removed_target && removed_target->node_owner_id != 0 &&
        node_owners_.release(removed_target->node_owner_id)) {
      delete_released_node(removed_target->node_owner_id);
    }
  }
  return removed;
}

bool AutopilotTools::has_tool(const godot::String &name) const {
  const std::string target = util::to_std(name);
  std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
  for (const ToolSpec &spec : dynamic_specs::store()) {
    if (spec.name == target) {
      return true;
    }
  }
  return false;
}

godot::Array AutopilotTools::list_tools() const {
  godot::Array out;
  std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
  for (const ToolSpec &spec : dynamic_specs::store()) {
    out.push_back(godot::String::utf8(spec.name.c_str()));
  }
  return out;
}

godot::Dictionary AutopilotTools::get_tool(const godot::String &name) const {
  const std::string target = util::to_std(name);
  std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
  const std::vector<ToolSpec> &specs = dynamic_specs::store();
  const std::vector<int64_t> &handles = handle_store();
  for (size_t i = 0; i < specs.size(); ++i) {
    if (specs[i].name != target) {
      continue;
    }
    const ToolSpec &spec = specs[i];
    godot::Dictionary out;
    out["name"] = godot::String::utf8(spec.name.c_str());
    out["description"] = godot::String::utf8(spec.description.c_str());
    out["category"] = godot::String::utf8(spec.category.c_str());
    godot::Array tags;
    for (const std::string &tag : spec.tags) {
      tags.push_back(godot::String::utf8(tag.c_str()));
    }
    out["tags"] = tags;
    out["side_effect"] = godot::String::utf8(side_effect_name(spec.side_effect));
    godot::Array params;
    for (const ParamSpec &param : spec.params) {
      godot::Dictionary item;
      item["name"] = godot::String::utf8(param.name.c_str());
      item["type"] = godot::String::utf8(param.type.c_str());
      item["description"] = godot::String::utf8(param.description.c_str());
      item["required"] = param.required;
      params.push_back(item);
    }
    out["params"] = params;
    out["handle"] = i < handles.size() ? handles[i] : 0;
    return out;
  }
  return godot::Dictionary();
}

godot::Dictionary AutopilotTools::call_tool(const godot::String &name,
                                            const godot::Dictionary &args) {
  if (ExportGuard::is_exporting()) {
    godot::Dictionary blocked;
    blocked["error"] = godot::String::utf8(
        dispatch::export_blocked_error_text().c_str());
    return blocked;
  }
  const std::string tool_name = util::to_std(name);
  if (!dynamic_specs::enabled().load()) {
    godot::Dictionary disabled;
    disabled["error"] = godot::String::utf8("user tools are disabled");
    disabled["code"] = godot::String::utf8("tools_disabled");
    return disabled;
  }
  godot_autopilot::util::JsonValue result;
  try {
    godot_autopilot::util::JsonValue json_args = VariantJson::serialize(godot::Variant(args));
    result = dispatch::call_handler(
        tool_name, json_args, dispatch::capture_depth_context());
  } catch (const std::exception &e) {
    result = util::error_json("call_tool '" + tool_name +
                              "' failed: " + e.what());
  } catch (...) {
    result =
        util::error_json("call_tool '" + tool_name + "' failed: unknown exception");
  }

  godot::Variant out = VariantJson::deserialize(result, "dictionary");
  if (out.get_type() == godot::Variant::DICTIONARY) {
    return out.operator godot::Dictionary();
  }
  return godot::Dictionary();
}

bool AutopilotTools::is_enabled() const {
  return dynamic_specs::enabled().load();
}

void AutopilotTools::set_enabled(bool enabled) {
  bool changed = false;
  bool expected = !enabled;
  changed = dynamic_specs::enabled().compare_exchange_strong(expected, enabled);
  if (changed) {
    refresh_dynamic_tools();
  }
  LogSystem::instance().log_detailed(
      LogLevel::Info, LogCategory::Tools,
      enabled ? "user tools enabled" : "user tools disabled",
      "enabled=" + std::string(enabled ? "true" : "false"));
  monitor::lifecycle(
      enabled ? "user_tools_enabled" : "user_tools_disabled",
      monitor::build_attrs({{"enabled", enabled ? "true" : "false"}}));
}

godot::Dictionary AutopilotTools::rescan(const godot::String &directory) {
  if (runtime_ops::has_editor_queue() &&
      !get_editor_queue().is_main_thread()) {
    const tools::TraceContext ctx = tools::capture_trace_context();
    const godot::String dir_copy = directory;
    auto state = std::make_shared<godot::Dictionary>();
    dispatch::BoundedWaitOutcome outcome =
        dispatch::run_bounded_on_main_thread(
            get_editor_queue(), "rescan", state,
            [this, dir_copy, ctx](godot::Dictionary &value) {
              tools::ScopedTraceContext restore(ctx);
              value = rescan(dir_copy);
            },
            GDA_DISPATCH_WAIT_DEFAULT_MS);
    if (!outcome.ok) {
      return rescan_error(outcome.error);
    }
    return *state;
  }
  const std::string dir = util::to_std(directory);
  if (dir.compare(0, 6, "res://") != 0 && dir.compare(0, 7, "user://") != 0) {
    return rescan_error("rescan: directory must start with res:// or user://");
  }
  if (godot::DirAccess::open(directory).is_null()) {
    return rescan_error("rescan: failed to open directory: " + dir);
  }
  const std::set<std::string> before = dynamic_tool_name_set();
  std::vector<std::string> files;
  collect_rescan_files(dir, files);
  godot::Array errors;
  for (const std::string &path : files) {
    rescan_single_file(path, errors);
  }
  godot::Array registered;
  {
    std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
    for (const ToolSpec &spec : dynamic_specs::store()) {
      if (before.find(spec.name) == before.end()) {
        registered.push_back(godot::String::utf8(spec.name.c_str()));
      }
    }
  }
  godot::Dictionary out;
  out["scanned"] = static_cast<int64_t>(files.size());
  out["registered"] = registered;
  out["failed"] = static_cast<int64_t>(errors.size());
  out["errors"] = errors;
  return out;
}

void AutopilotTools::_bind_methods() {
  godot::ClassDB::bind_method(
      godot::D_METHOD("register_tool", "definition", "callable"),
      &AutopilotTools::register_tool);
  godot::ClassDB::bind_method(
      godot::D_METHOD("register_csharp_tool", "definition", "callable"),
      &AutopilotTools::register_csharp_tool);
  godot::ClassDB::bind_method(
      godot::D_METHOD("unregister_tool", "handle"),
      &AutopilotTools::unregister_tool);
  godot::ClassDB::bind_method(godot::D_METHOD("has_tool", "name"),
                              &AutopilotTools::has_tool);
  godot::ClassDB::bind_method(godot::D_METHOD("list_tools"),
                              &AutopilotTools::list_tools);
  godot::ClassDB::bind_method(godot::D_METHOD("get_tool", "name"),
                              &AutopilotTools::get_tool);
  godot::ClassDB::bind_method(godot::D_METHOD("call_tool", "name", "args"),
                              &AutopilotTools::call_tool);
  godot::ClassDB::bind_method(
      godot::D_METHOD("rescan", "directory"), &AutopilotTools::rescan,
      DEFVAL(godot::String("res://addons/godot-autopilot-tools")));
  godot::ClassDB::bind_method(godot::D_METHOD("is_enabled"),
                              &AutopilotTools::is_enabled);
  godot::ClassDB::bind_method(godot::D_METHOD("set_enabled", "enabled"),
                              &AutopilotTools::set_enabled);
}

} // namespace godot_autopilot
