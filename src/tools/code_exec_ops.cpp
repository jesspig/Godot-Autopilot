#include "code_exec_ops.hpp"
#include "core/log_system.hpp"
#include "core/resource_registry.hpp"
#include "tools/debugger_ops.hpp"
#include "tools/dispatch.hpp"
#include "util/error_util.hpp"
#include "util/gdscript_wrap.hpp"
#include "util/variant_json.hpp"
#include <cctype>
#include <chrono>
#include <functional>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <sstream>
#include <string>

using namespace godot_autopilot;

namespace {

constexpr int WRAP_HEADER_LINES_SINGLE = 5;
constexpr int WRAP_HEADER_LINES_MULTI = 5;

std::string map_line_numbers(const std::string &err_text, int offset,
                             const std::string &tag) {
  std::string mapped;
  mapped.reserve(err_text.size());
  const std::string marker = "gdscript://";
  size_t pos = 0;
  while (true) {
    size_t mark = err_text.find(marker, pos);
    if (mark == std::string::npos) {
      mapped.append(err_text, pos, std::string::npos);
      break;
    }
    mapped.append(err_text, pos, mark - pos);
    size_t name_start = mark + marker.size();
    size_t name_end = err_text.find('.', name_start);
    bool is_gd_colon = name_end != std::string::npos &&
                       err_text.compare(name_end, 4, ".gd:") == 0;
    bool name_matches =
        tag.empty() || err_text.compare(name_start, tag.size(), tag) == 0;
    if (!is_gd_colon || !name_matches) {
      mapped.append(marker);
      pos = name_start;
      continue;
    }
    size_t num_start = name_end + 4;
    size_t num_end = num_start;
    while (num_end < err_text.size() &&
           std::isdigit(static_cast<unsigned char>(err_text[num_end]))) {
      ++num_end;
    }
    if (num_end == num_start) {
      mapped.append(marker);
      pos = name_start;
      continue;
    }
    std::string name = err_text.substr(name_start, name_end - name_start);
    int original = std::stoi(err_text.substr(num_start, num_end - num_start));
    std::string replacement;
    if (original - offset > 0) {
      replacement = "gdscript://" + name +
                    ".gd:" + std::to_string(original - offset) +
                    " (mapped to user source line " +
                    std::to_string(original - offset) + ")";
    } else {
      replacement = "gdscript://" + name + ".gd:" + std::to_string(original);
    }
    mapped += replacement;
    pos = num_end;
  }
  return mapped;
}

struct TempNodeGuard {
  godot::Node *&node;

  explicit TempNodeGuard(godot::Node *&n) : node(n) {}

  ~TempNodeGuard() { cleanup(); }

  void cleanup() {
    if (node == nullptr)
      return;
    if (node->get_parent()) {
      node->get_parent()->remove_child(node);
    }
    memdelete(node);
    node = nullptr;
  }
};

mcp::JsonValue extract_structured_error(const std::string &err_text) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);

  std::string line = err_text.substr(0, err_text.find('\n'));
  if (line.empty())
    return out;

  size_t sep = line.find(" - ");
  std::string head = (sep == std::string::npos) ? line : line.substr(0, sep);
  std::string message = (sep == std::string::npos) ? "" : line.substr(sep + 3);

  for (size_t i = head.size(); i > 0; --i) {
    size_t colon = i - 1;
    if (head[colon] != ':' || colon == 0)
      continue;
    char before = head[colon - 1];
    if (std::isdigit(static_cast<unsigned char>(before)))
      continue;
    if (i >= head.size() || !std::isdigit(static_cast<unsigned char>(head[i])))
      continue;
    size_t num_end = i;
    while (num_end < head.size() &&
           std::isdigit(static_cast<unsigned char>(head[num_end]))) {
      ++num_end;
    }
    size_t file_start = colon;
    while (file_start > 0 && head[file_start - 1] != ' ' &&
           head[file_start - 1] != '\t' && head[file_start - 1] != '[' &&
           head[file_start - 1] != ']') {
      --file_start;
    }
    out["file"] = mcp::JsonValue(head.substr(file_start, colon - file_start));
    out["line"] = mcp::JsonValue(
        static_cast<int64_t>(std::stoi(head.substr(i, num_end - i))));
    break;
  }

  if (!message.empty()) {
    out["message"] = mcp::JsonValue(message);
  }
  return out;
}

struct ExecContext {
  std::string source_code;
  std::string func_name;
  int timeout_ms = 5000;
  bool auto_owner = true;
  std::chrono::steady_clock::time_point start_time;
  bool has_func_def = false;
  std::string wrapped;
  godot::Ref<godot::GDScript> script;
  godot::Node *scene_root_for_leak = nullptr;
  int child_count_before = 0;
  godot::Variant result;
  std::string new_error_text;
  std::string new_output_text;
  bool temp_added = false;
  int auto_owner_set = 0;
};

bool check_source_safety(ExecContext &ctx, std::string &error_out) {
  ctx.has_func_def =
      gdscript_wrap::has_top_level_func_def(ctx.source_code);

  {
    std::istringstream stream(ctx.source_code);
    std::string line;
    while (std::getline(stream, line)) {
      size_t pos = line.find_first_not_of(" \t");
      if (pos == std::string::npos || line[pos] == '#')
        continue;
      if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/')
        continue;
      if (line.find("close_scene(") != std::string::npos ||
          line.find("close_scene (") != std::string::npos) {
        error_out =
            "calling EditorInterface.close_scene() from code_execute is unsafe "
            "(it destroys the executing node and crashes the editor) — use the "
            "close_editor_scene MCP tool instead";
        return false;
      }
    }
  }
  return true;
}

bool build_wrapped_source(ExecContext &ctx, std::string &error_out) {
  if (ctx.has_func_def) {

    std::string cleaned = gdscript_wrap::strip_extends_lines(ctx.source_code);
    ctx.wrapped = "@tool\nextends Node\n\nvar SceneRoot := "
                  "EditorInterface.get_edited_scene_root()\n\n" +
                  cleaned + "\n";

    if (!gdscript_wrap::defines_function_named(cleaned, ctx.func_name)) {
      ctx.wrapped += "func " + ctx.func_name + "():\n    pass\n";
    }
  } else {
    std::string cleaned = gdscript_wrap::strip_extends_lines(ctx.source_code);

    {
      std::istringstream stream(cleaned);
      std::string line;
      while (std::getline(stream, line)) {
        size_t pos = line.find_first_not_of(" \t");
        if (pos == std::string::npos || line[pos] == '#')
          continue;
        if (pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/')
          continue;
        if (line.compare(pos, 5, "func ") == 0 ||
            line.compare(pos, 5, "func\t") == 0 ||
            (line.compare(pos, 4, "func") == 0 && pos + 4 < line.size() &&
             line[pos + 4] == '(')) {
          mcp::JsonValue detail = godot_autopilot::util::error_detail(
              "func definition detected in single-function mode", "source_code",
              "no func definitions while in single-function mode",
              "top-level func definitions require multi-function mode; use "
              "editor script_create or wrap in a lambda");
          error_out = detail.Find("error")->GetString();
          return false;
        }
      }
    }

    gdscript_wrap::IndentStyle indent =
        gdscript_wrap::scan_indent_style(cleaned);

    if (indent.uses_tabs && indent.uses_spaces) {
      mcp::JsonValue detail = godot_autopilot::util::error_detail(
          "mixed tab/space indentation detected in source", "source_code",
          "consistent indentation",
          "reindent source with only tabs or only spaces; note the wrapper "
          "requires the same indentation style throughout");
      error_out = detail.Find("error")->GetString();
      return false;
    }

    std::string prefix = gdscript_wrap::indent_prefix(indent);

    ctx.wrapped = "@tool\nextends Node\n\nfunc " + ctx.func_name + "():\n";

    ctx.wrapped +=
        prefix + "var SceneRoot := EditorInterface.get_edited_scene_root()\n";
    ctx.wrapped += gdscript_wrap::reindent_lines(cleaned, prefix);
    ctx.wrapped += "\n";
  }
  return true;
}

bool compile_and_map_errors(ExecContext &ctx, std::string &error_out) {
  ctx.script.instantiate();
  if (ctx.script.is_null()) {
    error_out = "failed to create GDScript instance";
    return false;
  }

  ctx.script->set_source_code(godot::String(ctx.wrapped.c_str()));

  size_t compile_log_before = godot_autopilot::debugger_ops::capture_log_count();
  godot::Error parse_err = ctx.script->reload();
  if (parse_err != godot::OK) {
    int code = static_cast<int>(parse_err);
    std::string err_name = "ERR_UNKNOWN";
    if (code == 43)
      err_name = "ERR_PARSE_ERROR";
    int map_offset =
        ctx.has_func_def ? WRAP_HEADER_LINES_MULTI : WRAP_HEADER_LINES_SINGLE;
    std::string message = gdscript_wrap::compose_compile_failure_message(
        "GDScript compilation failed: " + err_name +
            " (code " + std::to_string(code) + ")",
        godot_autopilot::debugger_ops::capture_new_error_text(
            compile_log_before),
        ctx.wrapped,
        [&](const std::string &captured) {
          return gdscript_wrap::truncate_capture_text(
                     map_line_numbers(captured, map_offset, "")) +
                 "\nerror lines above were mapped from the generated wrapper "
                 "script";
        });
    error_out = message;
    return false;
  }
  return true;
}

bool run_with_timeout(ExecContext &ctx, std::string &error_out) {
  size_t log_before = godot_autopilot::debugger_ops::capture_log_count();

  ctx.child_count_before = 0;
  ctx.scene_root_for_leak = nullptr;
  {
    auto *editor_for_leak = godot::EditorInterface::get_singleton();
    if (editor_for_leak) {
      ctx.scene_root_for_leak = editor_for_leak->get_edited_scene_root();
    }
  }
  if (ctx.scene_root_for_leak) {
    ctx.child_count_before = ctx.scene_root_for_leak->get_child_count();
  }

  {
    godot::Node *temp_node = nullptr;
    TempNodeGuard temp_guard(temp_node);

    temp_node = memnew(godot::Node);
    temp_node->set_script(godot::Variant(ctx.script));

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
      ctx.temp_added = true;
    }

    godot::StringName fn_name(ctx.func_name.c_str());
    if (!temp_node->has_method(fn_name)) {
      error_out =
          "function not found in compiled script: " + ctx.func_name;
      return false;
    }

    bool timeout_hit = false;
    auto check_time = [&]() {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - ctx.start_time)
                         .count();
      if (elapsed >= ctx.timeout_ms)
        timeout_hit = true;
    };

    check_time();
    if (timeout_hit) {
      error_out = "Execution timed out after " +
                  std::to_string(ctx.timeout_ms) + " ms";
      return false;
    }

    ctx.result = temp_node->call(fn_name);
    ctx.new_error_text =
        godot_autopilot::debugger_ops::capture_new_error_text(log_before);
    ctx.new_output_text =
        godot_autopilot::debugger_ops::capture_new_output_text(log_before);
  }
  return true;
}

void reconcile_leaked_children(ExecContext &ctx) {
  ctx.auto_owner_set = 0;
  if (ctx.scene_root_for_leak) {
    int child_count_after = ctx.scene_root_for_leak->get_child_count();
    if (child_count_after > ctx.child_count_before) {
      if (ctx.auto_owner) {
        std::function<void(godot::Node *)> set_owner_recursive =
            [&](godot::Node *parent) {
              for (int i = 0; i < parent->get_child_count(); ++i) {
                godot::Node *child = parent->get_child(i);
                if (child->get_owner() == nullptr) {
                  child->set_owner(ctx.scene_root_for_leak);
                  ++ctx.auto_owner_set;
                }
                set_owner_recursive(child);
              }
            };
        for (int i = 0; i < ctx.scene_root_for_leak->get_child_count(); ++i) {
          set_owner_recursive(ctx.scene_root_for_leak->get_child(i));
        }
      } else {
        auto children = ctx.scene_root_for_leak->get_children();
        for (int i = children.size() - 1; i >= 0; i--) {
          auto *child = godot::Object::cast_to<godot::Node>(children[i]);
          if (child && child->get_owner() == nullptr) {
            ctx.scene_root_for_leak->remove_child(child);
            memdelete(child);
          }
        }
      }
    }
  }
}

mcp::JsonValue build_exec_result(ExecContext &ctx) {
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - ctx.start_time)
                     .count();

  mcp::JsonValue r(mcp::JsonValue::object_tag);

  if (ctx.result.get_type() == godot::Variant::OBJECT) {
    godot::Object *obj = ctx.result.operator godot::Object *();
    godot::Resource *res_obj = godot::Object::cast_to<godot::Resource>(obj);
    if (res_obj) {
      godot::Ref<godot::Resource> res(res_obj);
      godot_autopilot::resource_registry::register_resource(res, "");
      mcp::JsonValue reg(mcp::JsonValue::object_tag);
      reg["object_id"] =
          mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
      reg["object_id_str"] = mcp::JsonValue(
          std::to_string(static_cast<int64_t>(res->get_instance_id())));
      reg["class"] = mcp::JsonValue(util::to_std(res->get_class()));
      reg["path"] = mcp::JsonValue(util::to_std(res->get_path()));
      r["registered_resource"] = std::move(reg);
    }
  }

  r["result"] = godot_autopilot::VariantJson::serialize(ctx.result);
  r["execution_time_ms"] = mcp::JsonValue(static_cast<int64_t>(elapsed));
  r["auto_owner_set"] =
      mcp::JsonValue(static_cast<int64_t>(ctx.auto_owner_set));
  r["wrapped_source"] = mcp::JsonValue(ctx.wrapped);
  if (!ctx.new_output_text.empty()) {
    r["output"] = mcp::JsonValue(
        gdscript_wrap::truncate_capture_text(ctx.new_output_text));
  }
  if (!ctx.new_error_text.empty()) {
    r["runtime_error"] = mcp::JsonValue(true);
    std::string error_details =
        gdscript_wrap::truncate_capture_text(ctx.new_error_text);
    if (ctx.new_error_text.find("Node not found") != std::string::npos) {
      error_details += gdscript_wrap::NODE_NOT_FOUND_HINT;
    }
    r["error_details"] = mcp::JsonValue(error_details);
    mcp::JsonValue structured = extract_structured_error(ctx.new_error_text);
    if (structured.IsObject() && !structured.Empty()) {
      r["structured_error"] = std::move(structured);
    }
  }
  if (!ctx.temp_added) {
    r["note"] = mcp::JsonValue("temporary node was not added to any scene tree "
                               "— get_tree() will be null");
  }
  return r;
}

} // namespace

namespace godot_autopilot {
namespace code_exec_ops {

mcp::JsonValue handle_batch_execute(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "batch_execute called");

  auto *ops = args.Find("operations");
  if (!ops || !ops->IsArray()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("missing required parameter: operations (array)");
    return e;
  }

  bool stop_on_error = true;
  auto *stop = args.Find("stop_on_error");
  if (stop && stop->IsBool())
    stop_on_error = stop->GetBool();

  mcp::JsonValue results(mcp::JsonValue::array_tag);
  int succeeded = 0;
  int failed = 0;
  bool stopped = false;
  size_t stopped_after = 0;

  const auto &arr = ops->GetArray();
  for (size_t i = 0; i < arr.size(); ++i) {
    mcp::JsonValue result_item(mcp::JsonValue::object_tag);
    result_item["index"] = mcp::JsonValue(static_cast<int64_t>(i));

    const auto &op = arr[i];
    if (!op.IsObject()) {
      result_item["status"] = mcp::JsonValue("error");
      result_item["error"] = mcp::JsonValue("operation is not an object");
      results.PushBack(std::move(result_item));
      ++failed;
      if (stop_on_error) {
        stopped = true;
        stopped_after = i + 1;
        break;
      }
      continue;
    }

    auto *tool = op.Find("tool");
    if (!tool || !tool->IsString()) {
      result_item["status"] = mcp::JsonValue("error");
      result_item["error"] = mcp::JsonValue("missing required field: tool");
      results.PushBack(std::move(result_item));
      ++failed;
      if (stop_on_error) {
        stopped = true;
        stopped_after = i + 1;
        break;
      }
      continue;
    }

    std::string tool_name = tool->GetString();
    result_item["tool"] = mcp::JsonValue(tool_name);

    mcp::JsonValue tool_args(mcp::JsonValue::object_tag);
    if (auto *a = op.Find("args")) {
      tool_args = *a;
    }

    mcp::JsonValue handler_result = dispatch::call_handler(tool_name, tool_args);

    if (auto *err = handler_result.Find("error")) {
      result_item["status"] = mcp::JsonValue("error");
      result_item["error"] = *err;
      results.PushBack(std::move(result_item));
      ++failed;
      if (stop_on_error) {
        stopped = true;
        stopped_after = i + 1;
        break;
      }
    } else {
      result_item["status"] = mcp::JsonValue("ok");
      result_item["data"] = std::move(handler_result);
      results.PushBack(std::move(result_item));
      ++succeeded;
    }
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["results"] = std::move(results);
  int64_t executed = succeeded + failed;
  int64_t skipped = static_cast<int64_t>(arr.size()) - executed;
  r["total"] = mcp::JsonValue(executed);
  r["succeeded"] = mcp::JsonValue(static_cast<int64_t>(succeeded));
  r["failed"] = mcp::JsonValue(static_cast<int64_t>(failed));
  r["skipped"] = mcp::JsonValue(skipped);
  if (stopped) {
    r["note"] =
        mcp::JsonValue("stopped at operation " + std::to_string(stopped_after) +
                       "/" + std::to_string(arr.size()) +
                       " (stop_on_error=true); " + std::to_string(skipped) +
                       " remaining operations were not executed. Set "
                       "stop_on_error=false to run all operations.");
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "batch_execute completed");
  return r;
}

mcp::JsonValue handle_code_execute(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "code_execute called");

  ExecContext ctx;
  auto *src = args.Find("source_code");
  if (!src || !src->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: source_code");
    return e;
  }

  ctx.source_code = src->GetString();
  ctx.func_name = "_run";
  if (auto *fn = args.Find("function_name")) {
    if (fn->IsString())
      ctx.func_name = fn->GetString();
  }

  ctx.timeout_ms = 5000;
  if (auto *tm = args.Find("timeout_ms")) {
    if (tm->IsInt())
      ctx.timeout_ms = static_cast<int>(tm->GetInt());
  }

  ctx.auto_owner = true;
  if (auto *ao = args.Find("auto_owner")) {
    if (ao->IsBool())
      ctx.auto_owner = ao->GetBool();
  }

  ctx.start_time = std::chrono::steady_clock::now();

  std::string error_out;
  if (!check_source_safety(ctx, error_out))
    return util::error_json(error_out);
  if (!build_wrapped_source(ctx, error_out))
    return util::error_json(error_out);
  if (!compile_and_map_errors(ctx, error_out))
    return util::error_json(error_out);
  if (!run_with_timeout(ctx, error_out))
    return util::error_json(error_out);
  reconcile_leaked_children(ctx);

  mcp::JsonValue r = build_exec_result(ctx);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "code_execute completed");
  return r;
}

} // namespace code_exec_ops
} // namespace godot_autopilot
