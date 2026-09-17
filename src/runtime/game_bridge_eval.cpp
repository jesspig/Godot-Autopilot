#include "game_bridge.hpp"

#include "core/config.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/type_hint.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
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
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

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

bool call_arg_number(const JV &obj, const char *key, double &out) {
  auto *v = obj.Find(key);
  if (!v || !v->IsNumber())
    return false;
  out = v->IsInt() ? static_cast<double>(v->GetInt()) : v->GetDouble();
  return true;
}

// JSON objects such as {"x":440,"y":150} carry no type tag, so a bare
// deserialize() yields Dictionary and engine-side callv then fails with
// CALL_ERROR_INVALID_ARGUMENT. Convert shapes that match a built-in value
// type. Float variants are used ("x"+"y" becomes Vector2); integer variants
// (Vector2i/Vector3i/...) are resolved through method-signature hints in
// method_arg_hints(). Marker objects of the object-ref path
// (__node_ref__/object_id/object_id_str/class) are left untouched.
// Priority: Color > Vector4 > Vector3 > Vector2. Extra keys are ignored.
bool try_heuristic_builtin_arg(const JV &arg, godot::Variant &out) {
  if (!arg.IsObject())
    return false;
  if (arg.Find("__node_ref__") || arg.Find("object_id") ||
      arg.Find("object_id_str") || arg.Find("class")) {
    return false;
  }
  double r = 0.0, g = 0.0, b = 0.0, a = 1.0;
  if (call_arg_number(arg, "r", r) && call_arg_number(arg, "g", g) &&
      call_arg_number(arg, "b", b)) {
    if (auto *pa = arg.Find("a")) {
      if (!pa->IsNumber())
        return false;
      a = pa->IsInt() ? static_cast<double>(pa->GetInt()) : pa->GetDouble();
    }
    out = godot::Variant(
        godot::Color(static_cast<float>(r), static_cast<float>(g),
                     static_cast<float>(b), static_cast<float>(a)));
    return true;
  }
  double x = 0.0, y = 0.0, z = 0.0, w = 0.0;
  bool has_x = call_arg_number(arg, "x", x);
  bool has_y = call_arg_number(arg, "y", y);
  bool has_z = call_arg_number(arg, "z", z);
  bool has_w = call_arg_number(arg, "w", w);
  if (has_x && has_y && has_z && has_w) {
    out = godot::Variant(godot::Vector4(x, y, z, w));
    return true;
  }
  if (has_x && has_y && has_z) {
    out = godot::Variant(godot::Vector3(x, y, z));
    return true;
  }
  if (has_x && has_y) {
    out = godot::Variant(godot::Vector2(x, y));
    return true;
  }
  return false;
}

// Resolve per-argument type hints from the node's method list, reusing the
// same typed path as set_property (util::infer_type_hint plus
// deserialize(arg, hint)). Untyped (NIL) parameters yield an empty hint so the
// heuristic/inferred path applies. Returns one entry per requested argument.
std::vector<std::string> method_arg_hints(godot::Node *node,
                                          const std::string &method_name,
                                          size_t argc) {
  std::vector<std::string> hints(argc);
  if (argc == 0 || !node)
    return hints;
  godot::TypedArray<godot::Dictionary> methods = node->get_method_list();
  for (int64_t i = 0; i < methods.size(); i++) {
    godot::Dictionary info = methods[i];
    if (!info.has("name"))
      continue;
    if (util::to_std(info["name"].operator godot::String()) != method_name)
      continue;
    if (!info.has("args"))
      return hints;
    godot::Array margs = info["args"];
    for (size_t k = 0; k < argc && k < static_cast<size_t>(margs.size());
         k++) {
      godot::Dictionary arg_info = margs[static_cast<int64_t>(k)];
      if (arg_info.has("type") &&
          static_cast<int>(arg_info["type"]) ==
              static_cast<int>(godot::Variant::NIL)) {
        continue;
      }
      std::string hint;
      if (arg_info.has("class_name")) {
        hint = util::to_std(arg_info["class_name"].operator godot::String());
      }
      hints[k] = util::infer_type_hint(arg_info, hint);
    }
    return hints;
  }
  return hints;
}

const char *eval_call_error_kind(GDExtensionCallErrorType type) {
  switch (type) {
  case GDEXTENSION_CALL_OK:
    return "CALL_OK";
  case GDEXTENSION_CALL_ERROR_INVALID_METHOD:
    return "INVALID_METHOD";
  case GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT:
    return "INVALID_ARGUMENT";
  case GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS:
    return "TOO_MANY_ARGUMENTS";
  case GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS:
    return "TOO_FEW_ARGUMENTS";
  case GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL:
    return "INSTANCE_IS_NULL";
  case GDEXTENSION_CALL_ERROR_METHOD_NOT_CONST:
    return "METHOD_NOT_CONST";
  default:
    return "UNKNOWN";
  }
}

std::string eval_call_error_expected_text(const GDExtensionCallError &call_err) {
  if (call_err.error == GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT) {
    return util::to_std(godot::Variant::get_type_name(
        static_cast<godot::Variant::Type>(call_err.expected)));
  }
  return std::to_string(call_err.expected);
}

JV eval_call_error_result(const std::string &method_name,
                          const std::string &node_path,
                          const GDExtensionCallError &call_err, size_t argc) {
  std::string kind = eval_call_error_kind(call_err.error);
  std::string message;
  if (call_err.error == GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT ||
      call_err.error == GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS ||
      call_err.error == GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS) {
    message = "call_method '" + method_name + "' on " + node_path + " failed (" +
              kind + " at argument " + std::to_string(call_err.argument) +
              ", expected " + eval_call_error_expected_text(call_err) +
              "; got " + std::to_string(argc) + " argument(s))";
  } else {
    message = "call_method '" + method_name + "' on " + node_path + " failed (" +
              kind + ")";
  }
  JV err = error_result(message);
  err["call_error"] = JV(kind);
  err["argument"] = JV(static_cast<int>(call_err.argument));
  err["expected"] = JV(eval_call_error_expected_text(call_err));
  return err;
}

// MethodBind hash for classdb_get_method_bind comes from the live method list
// ("id" carries the bind hash, same value the generated bindings bake in).
int64_t eval_native_method_hash(godot::Node *node,
                                const std::string &method_name) {
  godot::TypedArray<godot::Dictionary> methods = node->get_method_list();
  for (int64_t i = 0; i < methods.size(); i++) {
    godot::Dictionary info = methods[i];
    if (!info.has("name") || !info.has("id")) {
      continue;
    }
    if (util::to_std(info["name"].operator godot::String()) != method_name) {
      continue;
    }
    return static_cast<int64_t>(info["id"]);
  }
  return -1;
}

GDExtensionMethodBindPtr
eval_native_method_bind(godot::Node *node,
                        const godot::StringName &method_sn,
                        int64_t method_hash) {
  if (node == nullptr || method_hash < 0) {
    return nullptr;
  }
  godot::StringName cls(node->get_class());
  for (int depth = 0; depth < 64; depth++) {
    GDExtensionMethodBindPtr bind =
        ::godot::gdextension_interface::classdb_get_method_bind(
            cls._native_ptr(), method_sn._native_ptr(), method_hash);
    if (bind != nullptr) {
      return bind;
    }
    godot::ClassDBSingleton *cdb = godot::ClassDBSingleton::get_singleton();
    if (cdb == nullptr) {
      return nullptr;
    }
    godot::StringName parent = cdb->get_parent_class(cls);
    if (parent.length() == 0 || parent == cls) {
      return nullptr;
    }
    cls = parent;
  }
  return nullptr;
}

void eval_call_script_method(
    godot::Node *node, const godot::StringName &method_sn,
    const std::vector<const godot::Variant *> &arg_ptrs, godot::Variant &r_ret,
    GDExtensionCallError &r_error) {
  const GDExtensionConstVariantPtr *args =
      arg_ptrs.empty() ? nullptr
                       : reinterpret_cast<const GDExtensionConstVariantPtr *>(
                             arg_ptrs.data());
  ::godot::gdextension_interface::object_call_script_method(
      node->_owner, method_sn._native_ptr(), args,
      static_cast<GDExtensionInt>(arg_ptrs.size()), r_ret._native_ptr(),
      &r_error);
}

void eval_call_native_method(
    godot::Node *node, GDExtensionMethodBindPtr bind,
    const std::vector<const godot::Variant *> &arg_ptrs, godot::Variant &r_ret,
    GDExtensionCallError &r_error) {
  const GDExtensionConstVariantPtr *args =
      arg_ptrs.empty() ? nullptr
                       : reinterpret_cast<const GDExtensionConstVariantPtr *>(
                             arg_ptrs.data());
  ::godot::gdextension_interface::object_method_bind_call(
      bind, node->_owner, args,
      static_cast<GDExtensionInt>(arg_ptrs.size()), r_ret._native_ptr(),
      &r_error);
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
  std::string method_name = method_p->GetString();
  const JV *args_p = params.Find("args");
  size_t argc = (args_p && args_p->IsArray()) ? args_p->Size() : 0;
  std::vector<std::string> arg_hints =
      method_arg_hints(node, method_name, argc);
  std::vector<godot::Variant> arg_values;
  if (args_p && args_p->IsArray()) {
    size_t index = 0;
    for (const auto &arg : args_p->GetArray()) {
      std::string hint =
          index < arg_hints.size() ? arg_hints[index] : std::string();
      godot::Variant value;
      if (!hint.empty()) {
        value = VariantJson::deserialize(arg, hint);
      } else if (arg.IsObject()) {
        godot::Variant heuristic;
        value = try_heuristic_builtin_arg(arg, heuristic)
                    ? heuristic
                    : VariantJson::deserialize(arg);
      } else {
        value = VariantJson::deserialize(arg);
      }
      arg_values.push_back(value);
      index++;
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

  // arg_values owns the converted arguments; arg_ptrs only borrows them and
  // both outlive the synchronous interface calls below.
  std::vector<const godot::Variant *> arg_ptrs;
  arg_ptrs.reserve(arg_values.size());
  for (const auto &v : arg_values) {
    arg_ptrs.push_back(&v);
  }

  // Direct GDExtension call with synchronous CallError, mirroring engine
  // Object::callp: script method first, native MethodBind on INVALID_METHOD
  // fallback. Success (including void/null) is decided by r_error alone.
  godot::Variant result;
  GDExtensionCallError call_err{};
  call_err.error = GDEXTENSION_CALL_OK;
  call_err.argument = 0;
  call_err.expected = 0;
  bool done = false;
  if (::godot::gdextension_interface::object_has_script_method(
          node->_owner, method_sn._native_ptr()) != 0) {
    eval_call_script_method(node, method_sn, arg_ptrs, result, call_err);
    done = call_err.error != GDEXTENSION_CALL_ERROR_INVALID_METHOD;
  }
  if (!done) {
    call_err.error = GDEXTENSION_CALL_OK;
    call_err.argument = 0;
    call_err.expected = 0;
    GDExtensionMethodBindPtr bind = eval_native_method_bind(
        node, method_sn, eval_native_method_hash(node, method_name));
    if (bind == nullptr) {
      call_err.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
    } else {
      eval_call_native_method(node, bind, arg_ptrs, result, call_err);
    }
  }

  if (call_err.error != GDEXTENSION_CALL_OK) {
    JV err =
        eval_call_error_result(method_name, path_p->GetString(), call_err, argc);
    append_eval_runtime_errors(err, call_seq_before);
    return err;
  }

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

  JV body = ok_result(VariantJson::serialize(result));
  append_eval_runtime_errors(body, call_seq_before);
  return body;
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
