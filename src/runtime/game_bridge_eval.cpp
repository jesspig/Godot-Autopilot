#include "game_bridge.hpp"

#include "core/config.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace runtime {
namespace game_bridge {

namespace {

using JV = mcp::JsonValue;

class GameBridgeEvalAwaiter : public godot::Node {
  GDCLASS(GameBridgeEvalAwaiter, godot::Node)

  int64_t request_id_ = 0;
  int64_t timeout_ms_ = 5000;
  godot::Ref<godot::RefCounted> state_ref_;
  godot::Node *target_ = nullptr;
  godot::Node *parent_ = nullptr;
  bool persist_ = false;
  bool connected_ = false;
  std::string persist_path_;
  godot::Ref<godot::SceneTreeTimer> timer_;
  bool done_ = false;
  uint64_t errors_since_seq_ = 0;

protected:
  static void _bind_methods() {
    godot::ClassDB::bind_method(
        godot::D_METHOD("_on_await_completed", "result"),
        &GameBridgeEvalAwaiter::_on_await_completed);
    godot::ClassDB::bind_method(godot::D_METHOD("_on_await_timeout"),
                                &GameBridgeEvalAwaiter::_on_await_timeout);
  }

public:
  void setup(int64_t request_id, const godot::Ref<godot::RefCounted> &state_ref,
             godot::Node *target, godot::Node *parent, bool persist,
             const std::string &persist_path, int64_t timeout_ms,
             uint64_t errors_since_seq) {
    request_id_ = request_id;
    timeout_ms_ = timeout_ms;
    state_ref_ = state_ref;
    target_ = target;
    parent_ = parent;
    persist_ = persist;
    persist_path_ = persist_path;
    errors_since_seq_ = errors_since_seq;
    godot::SceneTree *tree = get_scene_tree();
    if (!tree) {
      JV body = error_result("no scene tree for eval await");
      send_response(request_id_, std::move(body));
      cleanup();
      return;
    }
    tree->get_root()->add_child(this);
    register_cancel_handler(request_id_, [this] { cancel(); });
    if (state_ref_.is_valid()) {
      godot::Callable completed_cb(this,
                                   godot::StringName("_on_await_completed"));
      godot::Error err = state_ref_->connect("completed", completed_cb);
      if (err == godot::OK)
        connected_ = true;
    }

    timer_ = tree->create_timer(timeout_ms / 1000.0, true, false, true);
    if (timer_.is_valid()) {
      godot::Error err = timer_->connect(
          "timeout",
          godot::Callable(this, godot::StringName("_on_await_timeout")));
      if (err != godot::OK && !connected_) {
        JV body = error_result("failed to arm await timeout");
        send_response(request_id_, std::move(body));
        cleanup();
      }
    }
  }

  void _on_await_completed(const godot::Variant &p_result) {
    if (done_)
      return;
    done_ = true;
    JV body = ok_result(VariantJson::serialize(p_result));
    if (persist_ && !persist_path_.empty()) {
      if (body["result"].IsObject()) {
        body["result"]["node_path"] = JV(persist_path_);
      } else {
        JV inner(JV::object_tag);
        inner["value"] = std::move(body["result"]);
        inner["node_path"] = JV(persist_path_);
        body = ok_result(std::move(inner));
      }
    }
    append_eval_runtime_errors(body, errors_since_seq_);
    send_response(request_id_, std::move(body));
    cleanup();
  }

  void _on_await_timeout() {
    if (done_)
      return;
    done_ = true;
    std::string context;
    if (auto *tree = get_scene_tree()) {
      context = " (game paused: " +
                std::string(tree->is_paused() ? "true" : "false") +
                ", physics_frame: ";
      auto *engine = godot::Engine::get_singleton();
      context += engine ? std::to_string(engine->get_physics_frames())
                        : std::string("?");
      context += ")";
    }
    JV body = error_result("await timed out after " +
                           std::to_string(timeout_ms_) + " ms" + context);
    append_eval_runtime_errors(body, errors_since_seq_);
    send_response(request_id_, std::move(body));
    cleanup();
  }

  void cancel() {
    if (done_)
      return;
    done_ = true;
    cleanup();
  }

private:
  void cleanup() {
    unregister_cancel_handler(request_id_);
    timer_.unref();
    if (connected_ && state_ref_.is_valid()) {
      state_ref_->disconnect(
          "completed",
          godot::Callable(this, godot::StringName("_on_await_completed")));
      connected_ = false;
    }
    state_ref_.unref();
    if (!persist_ && target_ && parent_ && target_->get_parent() == parent_) {
      parent_->remove_child(target_);
      memdelete(target_);
    }
    target_ = nullptr;
    parent_ = nullptr;
    queue_free();
  }
};

JV op_eval_script(const JV &params, int64_t request_id) {
  auto *code_p = params.Find("source_code");
  if (!code_p || !code_p->IsString()) {
    return error_result("eval script requires source_code");
  }
  std::string source = code_p->GetString();

  godot::Ref<godot::GDScript> script;
  script.instantiate();
  if (script.is_null())
    return error_result("failed to create GDScript instance");

  script->set_source_code(godot::String(source.c_str()));
  uint64_t compile_seq_before = current_error_seq();
  godot::Error parse_err = script->reload();
  if (parse_err != godot::OK) {
    std::string message = "GDScript compilation failed (ERR code " +
                          std::to_string(static_cast<int>(parse_err)) + ")";
    EvalErrorDelta compile_delta = eval_error_delta(compile_seq_before);
    if (!compile_delta.text.empty()) {
      message += "\n" + truncate_error_text(compile_delta.text);
    }
    return error_result(message);
  }

  bool persist = false;
  if (auto *persist_p = params.Find("persist")) {
    if (persist_p->IsBool())
      persist = persist_p->GetBool();
  }
  std::string persist_name;
  if (auto *name_p = params.Find("persist_name")) {
    if (name_p->IsString())
      persist_name = name_p->GetString();
  }
  if (persist && persist_name.empty()) {
    persist_name = "eval_" + std::to_string(request_id);
  }
  int64_t await_timeout_ms = 5000;
  if (auto *tp = params.Find("timeout_ms")) {
    if (tp->IsInt() && tp->GetInt() > 0)
      await_timeout_ms = tp->GetInt();
  }

  godot::SceneTree *tree = get_scene_tree();
  if (!tree)
    return error_result("no scene tree");
  godot::Node *root = tree->get_root();
  if (!root)
    return error_result("no root node");

  godot::Node *parent = root;
  if (persist) {
    godot::Node *container =
        root->get_node_or_null(godot::NodePath("/root/__gda_runtime"));
    if (!container) {
      container = memnew(godot::Node);
      container->set_name("__gda_runtime");
      root->add_child(container);
    }
    if (container->get_node_or_null(
            godot::NodePath(godot::String(persist_name.c_str())))) {
      return error_result("persist node already exists: /root/__gda_runtime/" +
                          persist_name);
    }
    parent = container;
  }

  godot::Node *temp_node = memnew(godot::Node);
  if (persist)
    temp_node->set_name(godot::StringName(persist_name.c_str()));
  temp_node->set_script(godot::Variant(script));
  parent->add_child(temp_node);

  godot::StringName run_fn("_run");
  if (!temp_node->has_method(run_fn)) {
    parent->remove_child(temp_node);
    memdelete(temp_node);
    return error_result("function _run not found in compiled script — script "
                        "must extend Node and define func _run()");
  }

  uint64_t run_seq_before = current_error_seq();
  godot::Variant result = temp_node->call(run_fn);

  godot::Object *state_obj = nullptr;
  if (result.get_type() == godot::Variant::OBJECT) {
    state_obj = godot::Object::cast_to<godot::Object>(result);
  }
  bool is_await =
      state_obj && state_obj->get_class() == "GDScriptFunctionState";

  if (is_await) {
    std::string persist_path;
    if (persist)
      persist_path = "/root/__gda_runtime/" + persist_name;
    GameBridgeEvalAwaiter *awaiter = memnew(GameBridgeEvalAwaiter);
    awaiter->setup(request_id, godot::Ref<godot::RefCounted>(result), temp_node,
                   parent, persist, persist_path, await_timeout_ms,
                   run_seq_before);
    return JV();
  }

  if (!persist) {
    parent->remove_child(temp_node);
    memdelete(temp_node);
  }

  JV body = ok_result(VariantJson::serialize(result));
  append_eval_runtime_errors(body, run_seq_before);
  if (persist) {
    if (body["result"].IsObject()) {
      body["result"]["node_path"] = JV("/root/__gda_runtime/" + persist_name);
    } else {
      JV inner(JV::object_tag);
      inner["value"] = std::move(body["result"]);
      inner["node_path"] = JV("/root/__gda_runtime/" + persist_name);
      body = ok_result(std::move(inner));
    }
  }
  return body;
}

JV op_eval_get_property(const JV &params) {
  auto *path_p = params.Find("node_path");
  auto *prop_p = params.Find("property");
  if (!path_p || !path_p->IsString())
    return error_result("eval get_property requires node_path");
  if (!prop_p || !prop_p->IsString())
    return error_result("eval get_property requires property");
  godot::Node *node = resolve_node(path_p->GetString());
  if (!node)
    return error_result("node not found: " + path_p->GetString());
  godot::Variant value =
      node->get(godot::StringName(prop_p->GetString().c_str()));
  return ok_result(VariantJson::serialize(value));
}

godot::Dictionary find_property_info(godot::Node *node,
                                     const std::string &prop_name) {
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (dict.has("name") &&
        util::to_std(dict["name"].operator godot::String()) == prop_name) {
      return dict;
    }
  }
  return godot::Dictionary();
}

JV op_eval_set_property(const JV &params) {
  auto *path_p = params.Find("node_path");
  auto *prop_p = params.Find("property");
  auto *value_p = params.Find("value");
  if (!path_p || !path_p->IsString())
    return error_result("eval set_property requires node_path");
  if (!prop_p || !prop_p->IsString())
    return error_result("eval set_property requires property");
  if (!value_p)
    return error_result("eval set_property requires value");
  godot::Node *node = resolve_node(path_p->GetString());
  if (!node)
    return error_result("node not found: " + path_p->GetString());
  std::string prop_name = prop_p->GetString();

  godot::Dictionary prop_info = find_property_info(node, prop_name);
  if (prop_info.is_empty()) {
    return error_result("property not found: " + prop_name + " on " +
                        path_p->GetString());
  }

  std::string type_hint;
  if (prop_info.has("type")) {
    int type_id = static_cast<int>(prop_info["type"]);
    int hint_val = 0;
    if (prop_info.has("hint")) {
      hint_val = static_cast<int>(prop_info["hint"]);
    }
    bool is_object_type =
        static_cast<godot::Variant::Type>(type_id) == godot::Variant::OBJECT;
    bool is_resource_hint = hint_val == godot::PROPERTY_HINT_RESOURCE_TYPE;
    if ((is_object_type || is_resource_hint) && prop_info.has("hint_string")) {
      std::string hint_str =
          util::to_std(prop_info["hint_string"].operator godot::String());
      if (!hint_str.empty()) {
        type_hint = hint_str;
      }
    }
    if (type_hint.empty()) {
      type_hint = util::to_std(godot::Variant::get_type_name(
          static_cast<godot::Variant::Type>(type_id)));
    }
  }

  godot::StringName prop_name_sn(prop_name.c_str());
  godot::Variant value = VariantJson::deserialize(*value_p, type_hint);
  godot::Variant old_val = node->get(prop_name_sn);
  node->set(prop_name_sn, value);
  godot::Variant new_val = node->get(prop_name_sn);

  std::string readback_detail;
  util::ReadbackStatus readback =
      util::check_readback(value, old_val, new_val, readback_detail, true);
  if (readback == util::ReadbackStatus::REJECTED) {
    return util::error_detail(
        "property rejected: '" + prop_name + "' on " + path_p->GetString(),
        path_p->GetString(), "readback equals set value",
        "property may not exist, be read-only, or require a type hint");
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  if (readback == util::ReadbackStatus::CONVERTED) {
    r["warning"] = JV("set applied; " + readback_detail);
  }
  return r;
}

JV op_eval_call_method(const JV &params, int64_t request_id) {
  auto *path_p = params.Find("node_path");
  auto *method_p = params.Find("method");
  if (!path_p || !path_p->IsString())
    return error_result("eval call_method requires node_path");
  if (!method_p || !method_p->IsString())
    return error_result("eval call_method requires method");
  godot::Node *node = resolve_node(path_p->GetString());
  if (!node)
    return error_result("node not found: " + path_p->GetString());
  godot::Array call_args;
  if (auto *args_p = params.Find("args")) {
    if (args_p->IsArray()) {
      for (const auto &arg : args_p->GetArray()) {
        call_args.push_back(VariantJson::deserialize(arg));
      }
    }
  }
  godot::StringName method_sn(method_p->GetString().c_str());
  if (!node->has_method(method_sn)) {
    return error_result(
        "method not found: '" + method_p->GetString() + "' on " +
        path_p->GetString() +
        " — expected a built-in or script method of that node; use "
        "get_property_list or the node's script to list available methods");
  }
  uint64_t call_seq_before = current_error_seq();
  godot::Variant result = node->callv(method_sn, call_args);

  godot::Object *state_obj = nullptr;
  if (result.get_type() == godot::Variant::OBJECT) {
    state_obj = godot::Object::cast_to<godot::Object>(result);
  }
  bool is_await =
      state_obj && state_obj->get_class() == "GDScriptFunctionState";

  if (is_await) {
    int64_t await_timeout_ms = 5000;
    if (auto *tp = params.Find("timeout_ms")) {
      if (tp->IsInt() && tp->GetInt() > 0)
        await_timeout_ms = tp->GetInt();
    }
    GameBridgeEvalAwaiter *awaiter = memnew(GameBridgeEvalAwaiter);
    awaiter->setup(request_id, godot::Ref<godot::RefCounted>(result),
                   /*target=*/nullptr, /*parent=*/nullptr, /*persist=*/false,
                   /*persist_path=*/"", await_timeout_ms, call_seq_before);
    return JV();
  }

  return ok_result(VariantJson::serialize(result));
}

} // namespace

JV op_eval(const JV &params, int64_t request_id) {
  auto *action_p = params.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_result(
        "eval requires action (script|get_property|set_property|call_method)");
  }
  std::string action = action_p->GetString();
  if (action != "script" && action != "get_property" &&
      action != "set_property" && action != "call_method")
    return error_result("unknown eval action: " + action);
  static constexpr const char *allowed[] = {
      "action", "node_path", "property", "value", "method", "args",
      "source_code", "persist", "persist_name", "timeout_ms"};
  if (!params.IsObject())
    return error_result("eval params must be an object");
  for (const auto &entry : params.GetObject()) {
    bool known = false;
    for (const char *key : allowed)
      if (entry.first == key) {
        known = true;
        break;
      }
    if (!known)
      return error_result("unknown eval parameter: " + entry.first);
  }
  if (auto *path = params.Find("node_path"); path && !path->IsString())
    return error_result("eval node_path must be a string");
  if (auto *property = params.Find("property"); property &&
      (!property->IsString() || property->GetString().empty()))
    return error_result("eval property must be a non-empty string");
  if (auto *method = params.Find("method"); method &&
      (!method->IsString() || method->GetString().empty()))
    return error_result("eval method must be a non-empty string");
  if (auto *source = params.Find("source_code"); source &&
      (!source->IsString() || source->GetString().empty()))
    return error_result("eval source_code must be a non-empty string");
  if (auto *call_args = params.Find("args"); call_args && !call_args->IsArray())
    return error_result("eval args must be an array");
  if (auto *persist = params.Find("persist"); persist && !persist->IsBool())
    return error_result("eval persist must be a boolean");
  if (auto *persist_name = params.Find("persist_name"); persist_name &&
      (!persist_name->IsString() || persist_name->GetString().empty()))
    return error_result("eval persist_name must be a non-empty string");
  if (auto *timeout = params.Find("timeout_ms"); timeout &&
      (!timeout->IsInt() || timeout->GetInt() <= 0 ||
       timeout->GetInt() > GDA_MAX_TIMEOUT_MS))
    return error_result("eval timeout_ms must be an integer between 1 and 30000");
  if (action == "script")
    return op_eval_script(params, request_id);
  if (action == "get_property")
    return op_eval_get_property(params);
  if (action == "set_property")
    return op_eval_set_property(params);
  if (action == "call_method")
    return op_eval_call_method(params, request_id);
  return error_result("unknown eval action: " + action);
}

void register_eval_bridge_classes() {
  godot::ClassDB::register_class<GameBridgeEvalAwaiter>();
}

} // namespace game_bridge
} // namespace runtime
} // namespace godot_autopilot
