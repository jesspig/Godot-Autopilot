#include "game_bridge.hpp"

#include "core/config.hpp"
#include "core/log_system.hpp"
#include "runtime/gda_protocol.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/expression.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace runtime {
namespace game_bridge {

namespace {

constexpr int64_t kAssertMaxChars = 1024;
constexpr int64_t kSampleFramesMax = 120;
constexpr int64_t kSampleIntervalMax = 60;
constexpr int64_t kSampleSpanMaxFrames = 3600;
constexpr int64_t kEvidenceErrorsMax = 200;
constexpr int64_t kValidateElementsMax = 1000;
constexpr int kValidateMaxDepth = 64;
constexpr size_t kValidateIssuesMax = 200;

double json_number(const JV &value) {
  return value.IsInt() ? static_cast<double>(value.GetInt())
                       : value.GetDouble();
}

JV evaluate_assert_expression(const std::string &assert_expr,
                              const godot::Variant &value, bool &out_pass) {
  godot::Ref<godot::Expression> expr;
  expr.instantiate();
  if (expr.is_null())
    return error_result("eval_assert: failed to create Expression");
  godot::PackedStringArray input_names;
  input_names.append("value");
  godot::Error parse_err =
      expr->parse(godot::String::utf8(assert_expr.c_str()), input_names);
  if (parse_err != godot::OK) {
    JV body = error_result("eval_assert assert expression failed to parse: " +
                           util::to_std(expr->get_error_text()));
    JV details(JV::object_tag);
    details["code"] = JV("assert_parse_error");
    details["expression"] = JV(assert_expr);
    body["structured_error"] = std::move(details);
    return body;
  }
  godot::Array inputs;
  inputs.push_back(value);
  godot::Variant outcome = expr->execute(inputs, nullptr, false);
  if (expr->has_execute_failed()) {
    JV body = error_result("eval_assert assert expression failed to evaluate: " +
                           util::to_std(expr->get_error_text()));
    JV details(JV::object_tag);
    details["code"] = JV("assert_eval_error");
    details["expression"] = JV(assert_expr);
    body["structured_error"] = std::move(details);
    return body;
  }
  out_pass = outcome.booleanize();
  return JV();
}

bool sample_find_property(godot::Node *node, const std::string &prop_name) {
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (dict.has("name") &&
        util::to_std(dict["name"].operator godot::String()) == prop_name) {
      return true;
    }
  }
  return false;
}

void release_armed_eval_awaiter(int64_t request_id) {
  auto it = g_cancel_handlers.find(request_id);
  if (it == g_cancel_handlers.end())
    return;
  std::function<void()> handler = std::move(it->second);
  g_cancel_handlers.erase(it);
  handler();
}

class GameBridgeSampleAwaiter : public godot::Node {
  GDCLASS(GameBridgeSampleAwaiter, godot::Node)

  int64_t request_id_ = 0;
  uint64_t target_instance_id_ = 0;
  godot::StringName prop_;
  std::string node_path_;
  std::string prop_name_;
  int64_t frames_ = 0;
  int64_t interval_ = 0;
  int64_t timeout_ms_ = 5000;
  uint64_t errors_since_seq_ = 0;
  int64_t ticks_ = 0;
  uint64_t start_ticks_ms_ = 0;
  bool done_ = false;
  JV samples_;

protected:
  static void _bind_methods() {}

public:
  void setup(int64_t request_id, uint64_t target_instance_id,
             const std::string &node_path, const std::string &prop_name,
             int64_t frames, int64_t interval, int64_t timeout_ms,
             uint64_t errors_since_seq) {
    request_id_ = request_id;
    target_instance_id_ = target_instance_id;
    node_path_ = node_path;
    prop_name_ = prop_name;
    prop_ = godot::StringName(
        godot::String::utf8(prop_name.c_str()));
    frames_ = frames;
    interval_ = interval;
    timeout_ms_ = timeout_ms;
    errors_since_seq_ = errors_since_seq;
    samples_ = JV(JV::array_tag);
    set_process_mode(godot::Node::PROCESS_MODE_ALWAYS);
    set_process(true);
    start_ticks_ms_ = godot::Time::get_singleton()
                          ? godot::Time::get_singleton()->get_ticks_msec()
                          : 0;
    if (godot::SceneTree *tree = get_scene_tree())
      tree->get_root()->add_child(this);
    else
      done_ = true;
    register_cancel_handler(request_id_, [this] { cancel(); });
  }

  void cancel() {
    if (done_)
      return;
    done_ = true;
    cleanup();
  }

  void _process(double delta) override {
    (void)delta;
    if (done_)
      return;
    ticks_++;
    godot::Object *obj =
        godot::ObjectDB::get_instance(target_instance_id_);
    godot::Node *node = godot::Object::cast_to<godot::Node>(obj);
    if (!node) {
      finish(sample_truncated_error(
          "sample target node was freed after " +
          std::to_string(samples_.Size()) + " sample(s)",
          "sample_node_freed"));
      return;
    }
    if ((ticks_ - 1) % (interval_ + 1) == 0) {
      godot::Variant value = node->get(prop_);
      samples_.PushBack(VariantJson::serialize(value));
      if (samples_.Size() >= static_cast<size_t>(frames_)) {
        JV body = ok_result(sample_payload(false));
        finish(std::move(body));
        return;
      }
    }
    const uint64_t now_ms = godot::Time::get_singleton()
                                ? godot::Time::get_singleton()->get_ticks_msec()
                                : 0;
    if (now_ms - start_ticks_ms_ >= static_cast<uint64_t>(timeout_ms_)) {
      finish(sample_truncated_error(
          "sample timed out after " + std::to_string(timeout_ms_) + " ms (" +
              std::to_string(samples_.Size()) + "/" +
              std::to_string(frames_) + " sample(s) collected)",
          "sample_timeout"));
    }
  }

private:
  JV sample_payload(bool truncated) {
    JV r(JV::object_tag);
    r["node_path"] = JV(node_path_);
    r["property"] = JV(prop_name_);
    r["samples"] = samples_;
    r["count"] = JV(static_cast<int64_t>(samples_.Size()));
    r["frames"] = JV(frames_);
    r["interval_frames"] = JV(interval_);
    r["ticks_elapsed"] = JV(ticks_);
    if (truncated)
      r["truncated"] = JV(true);
    return r;
  }

  JV sample_truncated_error(const std::string &message, const char *code) {
    JV body = error_result(message);
    JV details(JV::object_tag);
    details["code"] = JV(code);
    details["samples_collected"] = samples_;
    details["count"] = JV(static_cast<int64_t>(samples_.Size()));
    details["frames"] = JV(frames_);
    body["structured_error"] = std::move(details);
    return body;
  }

  void finish(JV body) {
    done_ = true;
    append_eval_runtime_errors(body, errors_since_seq_);
    cleanup();
    send_response(request_id_, std::move(body));
  }

  void cleanup() {
    unregister_cancel_handler(request_id_);
    queue_free();
  }
};

JV evidence_status() {
  JV r(JV::object_tag);
  auto *engine = godot::Engine::get_singleton();
  if (engine) {
    godot::Dictionary version_info = engine->get_version_info();
    if (version_info.has("string")) {
      r["version"] = JV(util::to_std(godot::String(version_info["string"])));
    }
    r["fps"] = JV(engine->get_frames_per_second());
    r["physics_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames()));
  }
  r[GDA_FIELD_LAST_ACTIVITY_MS] = JV(static_cast<int64_t>(0));
  r[GDA_FIELD_HEALTHY] = JV(true);
  if (auto *tree = get_scene_tree()) {
    r["paused"] = JV(tree->is_paused());
    r["node_count"] = JV(static_cast<int64_t>(tree->get_node_count()));
    godot::Node *current = tree->get_current_scene();
    if (current) {
      godot::String scene_path = current->get_scene_file_path();
      if (scene_path.is_empty())
        scene_path = godot::String(current->get_name());
      r["scene"] = JV(util::to_std(scene_path));
    } else {
      r["scene"] = JV("");
    }
  }
  return r;
}

struct ValidateElement {
  std::string path;
  std::string type;
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
  int order = 0;
};

void walk_validate_elements(godot::Node *node, int depth, int64_t max_elements,
                            std::vector<ValidateElement> &elements,
                            bool &truncated, int &order) {
  if (truncated || depth > kValidateMaxDepth)
    return;
  if (auto *ctrl = godot::Object::cast_to<godot::Control>(node)) {
    if (static_cast<int64_t>(elements.size()) >= max_elements) {
      truncated = true;
      return;
    }
    ValidateElement element;
    element.path = util::to_std(godot::String(node->get_path()));
    element.type = util::to_std(godot::String(node->get_class()));
    godot::Rect2 rect = ctrl->get_global_rect();
    element.x = rect.position.x;
    element.y = rect.position.y;
    element.w = rect.size.x;
    element.h = rect.size.y;
    element.order = order++;
    elements.push_back(std::move(element));
  }
  const int64_t child_count = node->get_child_count();
  for (int64_t i = 0; i < child_count; i++)
    walk_validate_elements(node->get_child(i), depth + 1, max_elements,
                           elements, truncated, order);
}

bool matches_ignore_path(const std::string &path,
                         const std::vector<std::string> &ignored) {
  for (const std::string &rule : ignored) {
    if (path == rule)
      return true;
    const std::string suffix = "/" + rule;
    if (path.size() >= suffix.size() &&
        path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0)
      return true;
  }
  return false;
}

bool matches_ignore_class(godot::Node *node,
                          const std::vector<std::string> &ignored) {
  const std::string actual = util::to_std(godot::String(node->get_class()));
  for (const std::string &rule : ignored) {
    if (actual == rule)
      return true;
    if (auto *cdbs = godot::ClassDBSingleton::get_singleton()) {
      if (cdbs->is_parent_class(godot::StringName(actual.c_str()),
                                godot::StringName(rule.c_str())))
        return true;
    }
  }
  return false;
}

double intersect_area(double ax, double ay, double aw, double ah, double bx,
                      double by, double bw, double bh) {
  const double x0 = std::max(ax, bx);
  const double y0 = std::max(ay, by);
  const double x1 = std::min(ax + aw, bx + bw);
  const double y1 = std::min(ay + ah, by + bh);
  if (x1 <= x0 || y1 <= y0)
    return 0.0;
  return (x1 - x0) * (y1 - y0);
}

JV validate_element_rect(const ValidateElement &element) {
  JV rect(JV::object_tag);
  JV position(JV::object_tag);
  position["x"] = JV(element.x);
  position["y"] = JV(element.y);
  JV size(JV::object_tag);
  size["x"] = JV(element.w);
  size["y"] = JV(element.h);
  rect["position"] = std::move(position);
  rect["size"] = std::move(size);
  return rect;
}

} // namespace

JV op_eval_with_assert(const JV &params, int64_t request_id) {
  auto *assert_p = params.Find("assert");
  if (!assert_p || !assert_p->IsString() || assert_p->GetString().empty())
    return error_result(
        "eval_assert requires assert (non-empty expression string with the "
        "eval result bound as `value`, e.g. \"value > 0\")");
  const std::string assert_expr = assert_p->GetString();
  if (assert_expr.size() > static_cast<size_t>(kAssertMaxChars))
    return error_result("eval_assert assert exceeds 1024 characters");
  const auto *action_p = params.Find("action");
  const std::string action =
      (action_p && action_p->IsString()) ? action_p->GetString() : std::string();

  if (action == "get_property") {
    const auto *path_p = params.Find("node_path");
    const auto *prop_p = params.Find("property");
    if (!path_p || !path_p->IsString() || path_p->GetString().empty())
      return error_result("eval_assert get_property requires node_path");
    if (!prop_p || !prop_p->IsString() || prop_p->GetString().empty())
      return error_result("eval_assert get_property requires property");
    godot::Node *node = resolve_node(path_p->GetString());
    if (!node)
      return error_result("node not found: " + path_p->GetString());
    if (!sample_find_property(node, prop_p->GetString()))
      return error_result("property not found: " + prop_p->GetString() +
                          " on " + path_p->GetString());
    uint64_t seq_before = current_error_seq();
    godot::Variant value =
        node->get(godot::StringName(godot::String::utf8(
                                       prop_p->GetString().c_str())));
    bool pass = false;
    if (JV assert_error =
            evaluate_assert_expression(assert_expr, value, pass);
        !assert_error.IsNull()) {
      assert_error["actual"] = VariantJson::serialize(value);
      append_eval_runtime_errors(assert_error, seq_before);
      return assert_error;
    }
    JV inner(JV::object_tag);
    inner["actual"] = VariantJson::serialize(value);
    JV verdict(JV::object_tag);
    verdict["pass"] = JV(pass);
    verdict["expression"] = JV(assert_expr);
    inner["assert"] = std::move(verdict);
    JV body = ok_result(std::move(inner));
    append_eval_runtime_errors(body, seq_before);
    return body;
  }

  JV stripped(JV::object_tag);
  if (params.IsObject()) {
    for (const auto &entry : params.GetObject()) {
      if (entry.first == "assert")
        continue;
      stripped[entry.first] = entry.second;
    }
  }
  JV body = op_eval(stripped, request_id);
  if (body.IsNull()) {
    release_armed_eval_awaiter(request_id);
    return error_result(
        "eval_assert requires a synchronous eval result, but the script/method "
        "returned a coroutine (GDScriptFunctionState) — omit assert for "
        "awaitable calls, or observe the coroutine with "
        "start_game_job/get_game_job");
  }
  if (body.Find("error"))
    return body;
  const JV *result_p = body.Find("result");
  const JV result_json = result_p ? *result_p : JV();
  godot::Variant inferred = VariantJson::deserialize(result_json);
  bool pass = false;
  if (JV assert_error =
          evaluate_assert_expression(assert_expr, inferred, pass);
      !assert_error.IsNull()) {
    assert_error["actual"] = result_json;
    return assert_error;
  }
  JV inner(JV::object_tag);
  inner["actual"] = result_json;
  JV verdict(JV::object_tag);
  verdict["pass"] = JV(pass);
  verdict["expression"] = JV(assert_expr);
  inner["assert"] = std::move(verdict);
  return ok_result(std::move(inner));
}

JV op_sample_property(const JV &params, int64_t request_id) {
  const auto *path_p = params.Find("node_path");
  const auto *prop_p = params.Find("property");
  const auto *frames_p = params.Find("frames");
  if (!path_p || !path_p->IsString() || path_p->GetString().empty())
    return error_result("sample requires node_path (non-empty string)");
  if (!prop_p || !prop_p->IsString() || prop_p->GetString().empty())
    return error_result("sample requires property (non-empty string)");
  if (!frames_p || !frames_p->IsInt())
    return error_result("sample requires frames (integer 1-120)");
  const int64_t frames = frames_p->GetInt();
  if (frames < 1 || frames > kSampleFramesMax)
    return error_result("sample frames out of range (1-120): " +
                        std::to_string(frames));
  int64_t interval = 0;
  if (const auto *interval_p = params.Find("interval_frames")) {
    if (!interval_p->IsInt())
      return error_result("sample interval_frames must be an integer");
    interval = interval_p->GetInt();
    if (interval < 0 || interval > kSampleIntervalMax)
      return error_result("sample interval_frames out of range (0-60): " +
                          std::to_string(interval));
  }
  if (frames * (interval + 1) > kSampleSpanMaxFrames)
    return error_result("sample span frames*(interval_frames+1) exceeds 3600");
  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  if (const auto *timeout_p = params.Find("timeout_ms")) {
    if (!timeout_p->IsInt() || timeout_p->GetInt() <= 0)
      return error_result("sample timeout_ms must be a positive integer");
    timeout_ms = timeout_p->GetInt();
    if (timeout_ms > GDA_MAX_TIMEOUT_MS)
      timeout_ms = GDA_MAX_TIMEOUT_MS;
  }

  godot::Node *node = resolve_node(path_p->GetString());
  if (!node)
    return error_result("node not found: " + path_p->GetString());
  if (!sample_find_property(node, prop_p->GetString()))
    return error_result("property not found: " + prop_p->GetString() + " on " +
                        path_p->GetString());
  godot::SceneTree *tree = get_scene_tree();
  if (!tree || !tree->get_root())
    return error_result("no scene tree");

  GameBridgeSampleAwaiter *awaiter = memnew(GameBridgeSampleAwaiter);
  awaiter->setup(request_id, node->get_instance_id(), path_p->GetString(),
                 prop_p->GetString(), frames, interval, timeout_ms,
                 current_error_seq());
  return JV();
}

JV op_collect_evidence(const JV &params, int64_t request_id) {
  bool want_status = true;
  bool want_capture = true;
  bool want_errors = true;
  if (const auto *v = params.Find("include_status")) {
    if (!v->IsBool())
      return error_result("collect_evidence include_status must be a boolean");
    want_status = v->GetBool();
  }
  if (const auto *v = params.Find("include_capture")) {
    if (!v->IsBool())
      return error_result("collect_evidence include_capture must be a boolean");
    want_capture = v->GetBool();
  }
  if (const auto *v = params.Find("include_errors")) {
    if (!v->IsBool())
      return error_result("collect_evidence include_errors must be a boolean");
    want_errors = v->GetBool();
  }
  int64_t err_limit = 20;
  if (const auto *v = params.Find("limit")) {
    if (!v->IsInt())
      return error_result("collect_evidence limit must be an integer");
    err_limit = v->GetInt();
    if (err_limit < 1 || err_limit > kEvidenceErrorsMax)
      return error_result("collect_evidence limit out of range (1-200): " +
                          std::to_string(err_limit));
  }

  JV out(JV::object_tag);
  if (want_status) {
    out["status"] = evidence_status();
  } else {
    JV skipped(JV::object_tag);
    skipped["skipped"] = JV(true);
    out["status"] = std::move(skipped);
  }
  if (want_errors) {
    JV err_params(JV::object_tag);
    err_params["limit"] = JV(err_limit);
    JV errors_body = op_get_errors(err_params);
    if (const JV *r = errors_body.Find("result")) {
      out["errors"] = *r;
    } else {
      out["errors"] = std::move(errors_body);
    }
  } else {
    JV skipped(JV::object_tag);
    skipped["skipped"] = JV(true);
    out["errors"] = std::move(skipped);
  }
  if (want_capture) {
    JV cap_params(JV::object_tag);
    for (const char *key : {"region", "max_dimension", "scale"}) {
      if (const JV *v = params.Find(key))
        cap_params[key] = *v;
    }
    JV capture_body = op_capture(cap_params, request_id);
    if (capture_body.IsNull()) {
      JV skipped(JV::object_tag);
      skipped["skipped"] = JV(true);
      skipped["reason"] = JV("capture deferred (async)");
      out["capture"] = std::move(skipped);
    } else if (const JV *r = capture_body.Find("result")) {
      out["capture"] = *r;
    } else {
      out["capture"] = std::move(capture_body);
    }
  } else {
    JV skipped(JV::object_tag);
    skipped["skipped"] = JV(true);
    out["capture"] = std::move(skipped);
  }
  out["note"] = JV("read-only evidence bundle without verdict; sections fail "
                   "in isolation and mirror the stepwise calls "
                   "(get_game_status/get_debugger_errors/capture_game_viewport)");
  return ok_result(std::move(out));
}

JV op_validate_ui_layout(const JV &params) {
  int64_t max_elements = 100;
  if (const auto *v = params.Find("max_elements")) {
    if (!v->IsInt() || v->GetInt() < 1)
      return error_result(
          "validate_ui_layout max_elements must be a positive integer");
    max_elements = v->GetInt();
    if (max_elements > kValidateElementsMax)
      max_elements = kValidateElementsMax;
  }
  std::vector<std::string> ignore_paths;
  if (const auto *v = params.Find("ignore_paths")) {
    if (!v->IsArray() || v->GetArray().size() > 50)
      return error_result(
          "validate_ui_layout ignore_paths must be an array of at most 50 "
          "strings");
    for (const auto &item : v->GetArray()) {
      if (!item.IsString() || item.GetString().empty())
        return error_result("validate_ui_layout ignore_paths must contain "
                            "non-empty strings");
      ignore_paths.push_back(item.GetString());
    }
  }
  std::vector<std::string> ignore_classes;
  if (const auto *v = params.Find("ignore_classes")) {
    if (!v->IsArray() || v->GetArray().size() > 20)
      return error_result(
          "validate_ui_layout ignore_classes must be an array of at most 20 "
          "strings");
    for (const auto &item : v->GetArray()) {
      if (!item.IsString() || item.GetString().empty())
        return error_result("validate_ui_layout ignore_classes must contain "
                            "non-empty strings");
      ignore_classes.push_back(item.GetString());
    }
  }
  double min_area = 4.0;
  if (const auto *v = params.Find("min_area")) {
    if (!v->IsNumber())
      return error_result("validate_ui_layout min_area must be a number");
    min_area = json_number(*v);
    if (min_area < 0.0)
      return error_result(
          "validate_ui_layout min_area must not be negative");
  }
  double bounds_margin = 2.0;
  if (const auto *v = params.Find("bounds_margin")) {
    if (!v->IsNumber())
      return error_result("validate_ui_layout bounds_margin must be a number");
    bounds_margin = json_number(*v);
    if (bounds_margin < 0.0)
      return error_result(
          "validate_ui_layout bounds_margin must not be negative");
  }
  double occlude_ratio = 0.98;
  if (const auto *v = params.Find("occlude_ratio")) {
    if (!v->IsNumber())
      return error_result("validate_ui_layout occlude_ratio must be a number");
    occlude_ratio = json_number(*v);
    if (occlude_ratio < 0.5 || occlude_ratio > 1.0)
      return error_result(
          "validate_ui_layout occlude_ratio out of range (0.5-1.0)");
  }

  godot::SceneTree *tree = get_scene_tree();
  if (!tree)
    return error_result("no scene tree");
  godot::Node *root = tree->get_root();
  if (!root)
    return error_result("no root node");
  godot::Viewport *viewport =
      godot::Object::cast_to<godot::Viewport>(root);
  if (!viewport)
    return error_result("no root viewport");
  const godot::Rect2 viewport_rect = viewport->get_visible_rect();

  std::vector<ValidateElement> elements;
  bool truncated = false;
  int order = 0;
  walk_validate_elements(root, 0, max_elements, elements, truncated, order);

  struct VisibleItem {
    ValidateElement element;
    godot::Node *node = nullptr;
  };
  std::vector<VisibleItem> visible;
  int64_t skipped_invisible = 0;
  for (const ValidateElement &element : elements) {
    godot::Node *node = root->get_node_or_null(
        godot::NodePath(godot::String::utf8(element.path.c_str())));
    if (!node)
      continue;
    auto *ctrl = godot::Object::cast_to<godot::Control>(node);
    if (!ctrl || !ctrl->is_visible_in_tree()) {
      skipped_invisible++;
      continue;
    }
    if (matches_ignore_path(element.path, ignore_paths)) {
      skipped_invisible++;
      continue;
    }
    if (!ignore_classes.empty() &&
        matches_ignore_class(node, ignore_classes)) {
      skipped_invisible++;
      continue;
    }
    visible.push_back({element, node});
  }

  JV issues(JV::array_tag);
  bool issues_truncated = false;
  auto push_issue = [&](const char *kind, const char *severity,
                        const ValidateElement &element,
                        const std::string &detail, const JV *extra = nullptr) {
    if (issues.Size() >= static_cast<int64_t>(kValidateIssuesMax)) {
      issues_truncated = true;
      return;
    }
    JV item(JV::object_tag);
    item["kind"] = JV(kind);
    item["severity"] = JV(severity);
    item["path"] = JV(element.path);
    item["type"] = JV(element.type);
    item["rect"] = validate_element_rect(element);
    item["detail"] = JV(detail);
    if (extra)
      item["by"] = *extra;
    issues.PushBack(std::move(item));
  };

  for (const VisibleItem &item : visible) {
    const ValidateElement &element = item.element;
    if (element.w <= 0.0 || element.h <= 0.0) {
      push_issue("zero_size", "warning", element,
                 "visible control has zero width or height");
      continue;
    }
    const double area = element.w * element.h;
    if (area < min_area) {
      push_issue("tiny_area", "info", element,
                 "visible area " + std::to_string(area) +
                     " is below min_area " + std::to_string(min_area));
    }
    const double vx0 = viewport_rect.position.x - bounds_margin;
    const double vy0 = viewport_rect.position.y - bounds_margin;
    const double vx1 =
        viewport_rect.position.x + viewport_rect.size.x + bounds_margin;
    const double vy1 =
        viewport_rect.position.y + viewport_rect.size.y + bounds_margin;
    const bool fully_outside = element.x + element.w < vx0 ||
                               element.y + element.h < vy0 ||
                               element.x > vx1 || element.y > vy1;
    if (fully_outside) {
      push_issue("out_of_bounds", "warning", element,
                 "rect lies fully outside the viewport visible rect");
    } else {
      const double inside = intersect_area(
          element.x, element.y, element.w, element.h,
          viewport_rect.position.x, viewport_rect.position.y,
          viewport_rect.size.x, viewport_rect.size.y);
      if (inside < area) {
        push_issue("partially_out_of_bounds", "info", element,
                   "rect extends beyond the viewport visible rect");
      }
    }
  }

  for (size_t i = 0; i < visible.size(); i++) {
    const ValidateElement &lower = visible[i].element;
    if (lower.w <= 0.0 || lower.h <= 0.0)
      continue;
    const double area = lower.w * lower.h;
    for (size_t j = i + 1; j < visible.size(); j++) {
      const ValidateElement &upper = visible[j].element;
      if (upper.w <= 0.0 || upper.h <= 0.0)
        continue;
      const double covered = intersect_area(lower.x, lower.y, lower.w, lower.h,
                                            upper.x, upper.y, upper.w, upper.h);
      if (area > 0.0 && covered / area >= occlude_ratio) {
        JV by(JV::object_tag);
        by["path"] = JV(upper.path);
        by["type"] = JV(upper.type);
        push_issue("possibly_occluded", "warning", lower,
                   "at least " + std::to_string(static_cast<int>(
                                        occlude_ratio * 100.0)) +
                       "% of the area is covered by a later visible control "
                       "(draw-order heuristic, canvas_layer/z_index ignored)",
                   &by);
        break;
      }
    }
  }

  JV r(JV::object_tag);
  r["issues"] = std::move(issues);
  r["checked"] = JV(static_cast<int64_t>(visible.size()));
  r["truncated"] = JV(truncated);
  r["skipped_invisible"] = JV(skipped_invisible);
  if (issues_truncated)
    r["issues_truncated"] = JV(true);
  r["note"] = JV("read-only heuristic scan without verdict; invisible and "
                 "whitelisted controls are skipped, occlusion is a "
                 "document-order approximation");
  return ok_result(std::move(r));
}

void register_verify_bridge_classes() {
  godot::ClassDB::register_class<GameBridgeSampleAwaiter>();
}

} // namespace game_bridge
} // namespace runtime
} // namespace godot_autopilot
