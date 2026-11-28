#include "tools/domains/tween/tween_ops.hpp"
#include "core/config.hpp"
#include "core/observability/log_system.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/tool_base.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/tweener.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include "util/json_value.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace godot_autopilot {
namespace tween_ops {

using JV = godot_autopilot::util::JsonValue;

namespace {

class TweenStore {
public:
  int64_t store(const godot::Ref<godot::Tween> &tween) {
    int64_t id = static_cast<int64_t>(tween->get_instance_id());
    map[id] = tween;
    return id;
  }

  godot::Ref<godot::Tween> get(int64_t id) const {
    auto it = map.find(id);
    if (it != map.end())
      return it->second;
    return godot::Ref<godot::Tween>();
  }

private:
  std::unordered_map<int64_t, godot::Ref<godot::Tween>> map;
};

inline TweenStore &tween_store() {
  static TweenStore store;
  return store;
}

int64_t extract_timeout(const JV &args) {
  int64_t timeout = GDA_DEFAULT_TIMEOUT_MS;
  if (auto *tp = args.Find("timeout_ms")) {
    if (tp->IsInt() && tp->GetInt() > 0)
      timeout = tp->GetInt();
  }
  if (timeout > GDA_MAX_GAME_OP_TIMEOUT_MS)
    timeout = GDA_MAX_GAME_OP_TIMEOUT_MS;
  return timeout;
}

bool extract_wait_ready(const JV &args, bool *out_wait_ready) {
  *out_wait_ready = true;
  auto *wait_p = args.Find("wait_ready");
  if (!wait_p)
    return true;
  if (!wait_p->IsBool())
    return false;
  *out_wait_ready = wait_p->GetBool();
  return true;
}

void copy_optional(const JV &from, JV &to, const char *key) {
  if (auto *v = from.Find(key))
    to[key] = *v;
}

bool has_only_fields(const JV &value, const char *const *allowed,
                     size_t count, std::string &unknown) {
  if (!value.IsObject())
    return false;
  for (const auto &entry : value.GetObject()) {
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
      if (entry.first == allowed[i]) {
        found = true;
        break;
      }
    }
    if (!found) {
      unknown = entry.first;
      return false;
    }
  }
  return true;
}

std::string game_op_timeout_error() {
  return "timeout_ms must be an integer between 1 and " +
         std::to_string(GDA_MAX_GAME_OP_TIMEOUT_MS);
}

bool check_timeout_param(const JV &args, JV &error) {
  if (auto *timeout = args.Find("timeout_ms")) {
    if (!timeout->IsInt() || timeout->GetInt() <= 0 ||
        timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS) {
      error = util::error_json(game_op_timeout_error());
      return false;
    }
  }
  return true;
}

bool check_wait_ready_param(const JV &args, bool *out_wait_ready, JV &error) {
  if (!extract_wait_ready(args, out_wait_ready)) {
    error = util::error_json("wait_ready must be a boolean");
    return false;
  }
  return true;
}

bool check_tween_shape_params(const JV &args, JV &error) {
  if (auto *trans = args.Find("trans")) {
    if (!trans->IsInt() || trans->GetInt() < 0 || trans->GetInt() > 10) {
      error = util::error_json("trans must be an integer between 0 and 10 "
                               "(0=linear, 1=sine, 2=quint, 3=quart, 4=quad, "
                               "5=expo, 6=elastic, 7=cubic, 8=circ, 9=bounce, "
                               "10=back)");
      return false;
    }
  }
  if (auto *ease = args.Find("ease")) {
    if (!ease->IsInt() || ease->GetInt() < 0 || ease->GetInt() > 3) {
      error = util::error_json("ease must be an integer between 0 and 3 "
                               "(0=in, 1=out, 2=in_out, 3=out_in)");
      return false;
    }
  }
  if (auto *loops = args.Find("loops")) {
    if (!loops->IsInt() || loops->GetInt() < 0) {
      error = util::error_json("loops must be a non-negative integer "
                               "(0 means infinite loop)");
      return false;
    }
  }
  return true;
}

bool check_positive_duration(const JV &args, JV &error) {
  const JV *duration = args.Find("duration");
  if (!duration || !duration->IsNumber() ||
      util::json_number(duration, 0.0) <= 0.0) {
    error = util::error_json("missing required parameter: duration "
                             "(number of seconds greater than 0)");
    return false;
  }
  return true;
}

bool read_parallel(const JV &args, bool &out, JV &error) {
  out = false;
  const JV *parallel = args.Find("parallel");
  if (!parallel) {
    return true;
  }
  if (!parallel->IsBool()) {
    error = util::error_json("parallel must be a boolean");
    return false;
  }
  out = parallel->GetBool();
  return true;
}

void copy_parallel(const JV &args, JV &params) {
  if (const JV *parallel = args.Find("parallel")) {
    params["parallel"] = *parallel;
  }
}

bool read_named_method(const JV &args, const char *key, godot::Node *node,
                       std::string &out, JV &error) {
  const JV *value = args.Find(key);
  if (!value || !value->IsString() || value->GetString().empty()) {
    error = util::error_json(std::string("missing required parameter: ") +
                             key + " (non-empty method name)");
    return false;
  }
  out = value->GetString();
  if (!node->has_method(godot::StringName(out.c_str()))) {
    error = util::error_json("method not found on the target node: " + out);
    return false;
  }
  return true;
}

godot::Callable callable_on(godot::Node *node, const std::string &method) {
  return godot::Callable(node, godot::StringName(method.c_str()));
}

} // namespace

bool tween_ops_property_is_indexed(const std::string &property) {
  return property.find(':') != std::string::npos;
}

bool tween_ops_property_path_valid(const std::string &property,
                                   std::string &error) {
  if (property.empty() || property.front() == ':' || property.back() == ':' ||
      property.find("::") != std::string::npos ||
      property.find('/') != std::string::npos) {
    error = "invalid property path: " + property;
    return false;
  }
  return true;
}

bool tween_ops_property_type_hint(godot::Node *node,
                                  const std::string &property,
                                  std::string &hint, std::string &error) {
  if (!tween_ops_property_path_valid(property, error))
    return false;
  godot::Variant current;
  if (tween_ops_property_is_indexed(property)) {
    current = node->get_indexed(godot::NodePath(godot::String(property.c_str())));
  } else {
    current = node->get(godot::StringName(property.c_str()));
  }
  if (current.get_type() == godot::Variant::NIL) {
    error = "cannot infer property type from '" + property + "' on node";
    return false;
  }
  hint = util::to_std(godot::Variant::get_type_name(current.get_type()));
  return true;
}

JV handle_create_game_tween(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_game_tween called");
  if (!args.IsObject())
    return util::error_json("create_game_tween parameters must be an object");
  static constexpr const char *allowed[] = {
      "node_path", "property", "target_value", "duration", "trans", "ease",
      "loops", "speed_scale", "parallel", "timeout_ms", "wait_ready"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return util::error_json("unknown parameter for create_game_tween: " +
                            unknown);
  auto *path_p = args.Find("node_path");
  auto *prop_p = args.Find("property");
  auto *target_p = args.Find("target_value");
  auto *duration_p = args.Find("duration");
  if (!path_p || !path_p->IsString() || path_p->GetString().empty())
    return util::error_json("missing required parameter: node_path "
                            "(non-empty node path in the running game)");
  if (!prop_p || !prop_p->IsString() || prop_p->GetString().empty())
    return util::error_json("missing required parameter: property "
                            "(non-empty property name, e.g. position)");
  if (!target_p)
    return util::error_json("missing required parameter: target_value "
                            "(final value the property animates to)");
  if (!duration_p || !duration_p->IsNumber() ||
      util::json_number(duration_p, 0.0) <= 0.0)
    return util::error_json("missing required parameter: duration "
                            "(number of seconds greater than 0)");
  JV error;
  if (!check_tween_shape_params(args, error))
    return error;
  if (auto *speed = args.Find("speed_scale")) {
    if (!speed->IsNumber() || util::json_number(speed, 0.0) <= 0.0)
      return util::error_json("speed_scale must be a number greater than 0");
  }
  bool parallel = false;
  if (!read_parallel(args, parallel, error))
    return error;
  if (!check_timeout_param(args, error))
    return error;
  bool wait_ready = true;
  if (!check_wait_ready_param(args, &wait_ready, error))
    return error;
  JV params(JV::object_tag);
  params["node_path"] = *path_p;
  params["property"] = *prop_p;
  params["target_value"] = *target_p;
  params["duration"] = *duration_p;
  copy_optional(args, params, "trans");
  copy_optional(args, params, "ease");
  copy_optional(args, params, "loops");
  copy_optional(args, params, "speed_scale");
  copy_parallel(args, params);
  return runtime_ops::handle_gda_send("tween_create", params,
                                      extract_timeout(args), false, wait_ready);
}

JV handle_game_tween_callback(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_tween callback called");
  if (!args.IsObject())
    return util::error_json("game_tween action=callback parameters must be an "
                            "object");
  static constexpr const char *allowed[] = {
      "node_path", "callback_method", "callback_args", "parallel", "timeout_ms",
      "wait_ready"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return util::error_json("unknown parameter for game_tween action=callback: " +
                            unknown);
  auto *path_p = args.Find("node_path");
  auto *method_p = args.Find("callback_method");
  if (!path_p || !path_p->IsString() || path_p->GetString().empty())
    return util::error_json("missing required parameter: node_path "
                            "(non-empty node path in the running game)");
  if (!method_p || !method_p->IsString() || method_p->GetString().empty())
    return util::error_json("missing required parameter: callback_method "
                            "(non-empty method name on node_path)");
  if (auto *callback_args = args.Find("callback_args")) {
    if (!callback_args->IsArray())
      return util::error_json("callback_args must be an array");
  }
  JV error;
  bool parallel = false;
  if (!read_parallel(args, parallel, error))
    return error;
  if (!check_timeout_param(args, error))
    return error;
  bool wait_ready = true;
  if (!check_wait_ready_param(args, &wait_ready, error))
    return error;
  JV params(JV::object_tag);
  params["node_path"] = *path_p;
  params["kind"] = JV("callback");
  params["method"] = *method_p;
  copy_optional(args, params, "callback_args");
  copy_parallel(args, params);
  return runtime_ops::handle_gda_send("tween_create", params,
                                      extract_timeout(args), false, wait_ready);
}

JV handle_game_tween_method(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_tween method called");
  if (!args.IsObject())
    return util::error_json("game_tween action=method parameters must be an "
                            "object");
  static constexpr const char *allowed[] = {
      "node_path", "method_name", "from_value", "target_value", "duration",
      "parallel", "timeout_ms", "wait_ready"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return util::error_json("unknown parameter for game_tween action=method: " +
                            unknown);
  auto *path_p = args.Find("node_path");
  auto *method_p = args.Find("method_name");
  if (!path_p || !path_p->IsString() || path_p->GetString().empty())
    return util::error_json("missing required parameter: node_path "
                            "(non-empty node path in the running game)");
  if (!method_p || !method_p->IsString() || method_p->GetString().empty())
    return util::error_json("missing required parameter: method_name "
                            "(non-empty method name on node_path)");
  if (!args.Find("from_value"))
    return util::error_json("missing required parameter: from_value "
                            "(starting value passed to method_name)");
  if (!args.Find("target_value"))
    return util::error_json("missing required parameter: target_value "
                            "(end value passed to method_name)");
  JV error;
  if (!check_positive_duration(args, error))
    return error;
  bool parallel = false;
  if (!read_parallel(args, parallel, error))
    return error;
  if (!check_timeout_param(args, error))
    return error;
  bool wait_ready = true;
  if (!check_wait_ready_param(args, &wait_ready, error))
    return error;
  JV params(JV::object_tag);
  params["node_path"] = *path_p;
  params["kind"] = JV("method");
  params["method"] = *method_p;
  params["from"] = *args.Find("from_value");
  params["to"] = *args.Find("target_value");
  params["duration"] = *args.Find("duration");
  copy_parallel(args, params);
  return runtime_ops::handle_gda_send("tween_create", params,
                                      extract_timeout(args), false, wait_ready);
}

JV handle_game_tween_interval(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_tween interval called");
  if (!args.IsObject())
    return util::error_json("game_tween action=interval parameters must be an "
                            "object");
  static constexpr const char *allowed[] = {
      "duration", "parallel", "timeout_ms", "wait_ready"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return util::error_json(
        "unknown parameter for game_tween action=interval: " + unknown);
  JV error;
  if (!check_positive_duration(args, error))
    return error;
  bool parallel = false;
  if (!read_parallel(args, parallel, error))
    return error;
  if (!check_timeout_param(args, error))
    return error;
  bool wait_ready = true;
  if (!check_wait_ready_param(args, &wait_ready, error))
    return error;
  JV params(JV::object_tag);
  params["kind"] = JV("interval");
  params["duration"] = *args.Find("duration");
  copy_parallel(args, params);
  return runtime_ops::handle_gda_send("tween_create", params,
                                      extract_timeout(args), false, wait_ready);
}

JV handle_control_game_tween(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "control_game_tween called");
  if (!args.IsObject())
    return util::error_json("control_game_tween parameters must be an object");
  static constexpr const char *allowed[] = {"tween_id", "action", "timeout_ms",
                                            "wait_ready"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return util::error_json("unknown parameter for control_game_tween: " +
                            unknown);
  auto *id_p = args.Find("tween_id");
  auto *action_p = args.Find("action");
  if (!id_p || !id_p->IsInt())
    return util::error_json("missing required parameter: tween_id "
                            "(integer handle returned by create_game_tween)");
  if (!action_p || !action_p->IsString())
    return util::error_json("missing required parameter: action "
                            "(play|pause|stop|kill)");
  const std::string action = action_p->GetString();
  if (action != "play" && action != "pause" && action != "stop" &&
      action != "kill")
    return util::error_json("invalid action: expected "
                            "play|pause|stop|kill");
  JV error;
  if (!check_timeout_param(args, error))
    return error;
  bool wait_ready = true;
  if (!check_wait_ready_param(args, &wait_ready, error))
    return error;
  JV params(JV::object_tag);
  params["tween_id"] = *id_p;
  params["action"] = *action_p;
  return runtime_ops::handle_gda_send("tween_control", params,
                                      extract_timeout(args), false, wait_ready);
}

JV handle_get_game_tween_list(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_game_tween_list called");
  if (!args.IsObject())
    return util::error_json(
        "get_game_tween_list parameters must be an object");
  static constexpr const char *allowed[] = {"timeout_ms", "wait_ready"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return util::error_json("unknown parameter for get_game_tween_list: " +
                            unknown);
  JV error;
  if (!check_timeout_param(args, error))
    return error;
  bool wait_ready = true;
  if (!check_wait_ready_param(args, &wait_ready, error))
    return error;
  JV params(JV::object_tag);
  return runtime_ops::handle_gda_send("tween_list", params,
                                      extract_timeout(args), false, wait_ready);
}

JV handle_tween_editor_property(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "tween_editor_property called");
  if (!args.IsObject())
    return util::error_json(
        "tween_editor_property parameters must be an object");
  static constexpr const char *allowed[] = {
      "action", "path", "property", "target_value", "from_value", "duration",
      "method_name", "callback_method", "callback_args", "parallel", "trans",
      "ease", "loops"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return util::error_json("unknown parameter for tween_editor_property: " +
                            unknown);
  std::string action = "property";
  if (auto *action_p = args.Find("action")) {
    if (!action_p->IsString())
      return util::error_json(
          "action must be a string (property|callback|method|interval)");
    action = action_p->GetString();
  }
  if (action != "property" && action != "callback" && action != "method" &&
      action != "interval")
    return util::error_json(
        "invalid action: expected property|callback|method|interval");
  auto *path_p = args.Find("path");
  if (!path_p || !path_p->IsString() || path_p->GetString().empty())
    return util::error_json("missing required parameter: path "
                            "(non-empty node path in the edited scene)");
  JV error;
  if (!check_tween_shape_params(args, error))
    return error;
  bool parallel = false;
  if (!read_parallel(args, parallel, error))
    return error;
  std::string property;
  std::string property_hint;
  std::string validation_error;
  std::string method;
  if (action == "property") {
    const JV *prop_p = args.Find("property");
    if (!prop_p || !prop_p->IsString() || prop_p->GetString().empty())
      return util::error_json("missing required parameter: property "
                              "(non-empty property name, e.g. position)");
    property = prop_p->GetString();
    if (!args.Find("target_value"))
      return util::error_json("missing required parameter: target_value "
                              "(final value the property animates to)");
    if (!check_positive_duration(args, error))
      return error;
    if (!tween_ops_property_path_valid(property, validation_error))
      return util::error_json(validation_error);
  } else if (action == "method") {
    if (!args.Find("from_value"))
      return util::error_json("missing required parameter: from_value "
                              "(starting value passed to method_name)");
    if (!args.Find("target_value"))
      return util::error_json("missing required parameter: target_value "
                              "(end value passed to method_name)");
    if (!check_positive_duration(args, error))
      return error;
  } else if (action == "interval") {
    if (!check_positive_duration(args, error))
      return error;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return util::error_json("EditorInterface not available");
  godot::Node *root = editor->get_edited_scene_root();
  if (!root)
    return util::error_json("no edited scene: open or create a scene first");
  std::string hint;
  godot::Node *node =
      util::resolve_scene_node(path_p->GetString(), root, &hint);
  if (!node)
    return util::error_detail(
        "node not found: " + path_p->GetString(),
        "tween_ops.cpp handle_tween_editor_property", hint,
        "pass a node path inside the edited scene");
  godot::SceneTree *tree = node->get_tree();
  if (!tree)
    return util::error_json("SceneTree not available for node " +
                            path_p->GetString());
  if (action == "callback" || action == "method") {
    const char *method_key =
        action == "callback" ? "callback_method" : "method_name";
    if (!read_named_method(args, method_key, node, method, error))
      return error;
  }
  if (action == "callback") {
    if (auto *callback_args = args.Find("callback_args")) {
      if (!callback_args->IsArray())
        return util::error_json("callback_args must be an array");
    }
  }
  if (action == "property") {
    if (!tween_ops_property_type_hint(node, property, property_hint,
                                      validation_error))
      return util::error_json(validation_error);
  }

  godot::Ref<godot::Tween> tween = tree->create_tween();
  if (!tween.is_valid())
    return util::error_json("SceneTree::create_tween returned null");
  auto fail = [&tween](const std::string &message) {
    if (tween.is_valid())
      tween->kill();
    return util::error_json(message);
  };
  if (auto *trans = args.Find("trans"))
    tween->set_trans(
        static_cast<godot::Tween::TransitionType>(trans->GetInt()));
  if (auto *ease = args.Find("ease"))
    tween->set_ease(static_cast<godot::Tween::EaseType>(ease->GetInt()));
  if (auto *loops = args.Find("loops"))
    tween->set_loops(static_cast<int>(loops->GetInt()));
  tween->set_parallel(parallel);
  JV inner(JV::object_tag);
  inner["action"] = JV(action);
  inner["path"] = JV(path_p->GetString());
  inner["parallel"] = JV(parallel);
  if (action == "property") {
    const double duration = util::json_number(args.Find("duration"), 0.0);
    godot::Variant final_value =
        VariantJson::deserialize(*args.Find("target_value"), property_hint);
    godot::Ref<godot::PropertyTweener> tweener = tween->tween_property(
        node, godot::NodePath(godot::String(property.c_str())), final_value,
        duration);
    if (!tweener.is_valid())
      return fail("tween_property rejected property '" + property +
                  "' on node " + path_p->GetString());
    inner["property"] = JV(property);
  } else if (action == "callback") {
    godot::Callable callable = callable_on(node, method);
    if (auto *callback_args = args.Find("callback_args")) {
      godot::Array bound;
      for (const auto &item : callback_args->GetArray())
        bound.push_back(VariantJson::deserialize(item));
      callable = callable.bindv(bound);
    }
    tween->tween_callback(callable);
    inner["callback_method"] = JV(method);
  } else if (action == "method") {
    const double duration = util::json_number(args.Find("duration"), 0.0);
    godot::Variant from = VariantJson::deserialize(*args.Find("from_value"));
    godot::Variant to = VariantJson::deserialize(*args.Find("target_value"));
    tween->tween_method(callable_on(node, method), from, to, duration);
    inner["method_name"] = JV(method);
    inner["duration"] = JV(duration);
  } else {
    const double duration = util::json_number(args.Find("duration"), 0.0);
    tween->tween_interval(duration);
    inner["duration"] = JV(duration);
  }
  const int64_t tween_id = tween_store().store(tween);
  inner["tween_id"] = JV(tween_id);
  inner["running"] = JV(tween->is_running());
  JV r(JV::object_tag);
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, util::edited_scene_info());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "tween_editor_property completed");
  return r;
}

JV handle_game_tween_bundle(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_tween called");
  if (!args.IsObject()) {
    return util::error_json("game_tween parameters must be an object");
  }
  auto *action_p = args.Find("action");
  const std::string action =
      (action_p && action_p->IsString()) ? action_p->GetString() : std::string();
  if (action.empty()) {
    return util::error_json(
        "missing required parameter: action (create|control)");
  }
  JV forwarded(JV::object_tag);
  forwarded = args;
  forwarded.GetObject().erase("action");
  if (action == "create") {
    return handle_create_game_tween(forwarded);
  }
  if (action == "callback") {
    return handle_game_tween_callback(forwarded);
  }
  if (action == "method") {
    return handle_game_tween_method(forwarded);
  }
  if (action == "interval") {
    return handle_game_tween_interval(forwarded);
  }
  if (action == "control") {
    if (auto *tween_action = args.Find("tween_action");
        tween_action != nullptr && tween_action->IsString()) {
      forwarded["action"] = *tween_action;
    }
    forwarded.GetObject().erase("tween_action");
    return handle_control_game_tween(forwarded);
  }
  return util::error_json(
      "invalid action: expected create|control|callback|method|interval");
}

} // namespace tween_ops
} // namespace godot_autopilot
