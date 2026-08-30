#include "test_ops.hpp"
#include "core/log_system.hpp"
#include "tools/debugger_ops.hpp"
#include "util/error_util.hpp"
#include "util/gdscript_wrap.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <sstream>
#include <string>
#include <vector>

using namespace godot_autopilot;

namespace {

constexpr int TEST_DEFAULT_TIMEOUT_MS = 10000;
constexpr int TEST_MAX_TIMEOUT_MS = 30000;
constexpr const char *TEST_BODY_FUNC = "__gda_body";
constexpr const char *TEST_ABORT_TOKEN = "gda_test_abort";

int64_t ms_between(std::chrono::steady_clock::time_point a,
                   std::chrono::steady_clock::time_point b) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
}

std::string map_line_numbers(const std::string &err_text, int offset) {
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
    if (!is_gd_colon) {
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
    if (original - offset > 0) {
      mapped += "gdscript://" + name + ".gd:" +
                std::to_string(original - offset) +
                " (mapped to user source line " +
                std::to_string(original - offset) + ")";
    } else {
      mapped +=
          "gdscript://" + name + ".gd:" + std::to_string(original);
    }
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

bool is_comment_or_blank(const std::string &line) {
  size_t pos = line.find_first_not_of(" \t");
  if (pos == std::string::npos || line[pos] == '#')
    return true;
  return pos + 1 < line.size() && line[pos] == '/' && line[pos + 1] == '/';
}

bool top_level_token_is(const std::string &line, const char *word,
                        size_t wlen) {
  size_t pos = line.find_first_not_of(" \t");
  if (pos == std::string::npos || line.compare(pos, wlen, word) != 0)
    return false;
  size_t after = pos + wlen;
  return after >= line.size() || line[after] == ' ' || line[after] == '\t' ||
         line[after] == '(';
}

std::string filter_abort_noise(const std::string &text) {
  if (text.find(TEST_ABORT_TOKEN) == std::string::npos)
    return text;
  std::istringstream stream(text);
  std::string line;
  std::string kept;
  bool first = true;
  while (std::getline(stream, line)) {
    if (line.find(TEST_ABORT_TOKEN) != std::string::npos)
      continue;
    if (!first)
      kept += "\n";
    first = false;
    kept += line;
  }
  return kept;
}

int parse_timeout_ms(const mcp::JsonValue &args) {
  int value = TEST_DEFAULT_TIMEOUT_MS;
  if (auto *tm = args.Find("timeout_ms")) {
    if (tm->IsInt())
      value = static_cast<int>(tm->GetInt());
  }
  if (value <= 0)
    value = TEST_DEFAULT_TIMEOUT_MS;
  return std::min(value, TEST_MAX_TIMEOUT_MS);
}

struct WrappedCase {
  std::string wrapped;
  int header_lines = 0;
};

std::string skeleton_header(const std::string &I) {
  std::string s;
  s += "@tool\n";
  s += "extends Node\n";
  s += "\n";
  s += "var SceneRoot := EditorInterface.get_edited_scene_root()\n";
  s += "\n";
  s += "var __gda_checks: Array = []\n";
  s += "var __gda_aborted := false\n";
  s += "var __gda_sink: Object = null\n";
  s += "\n";
  s += "func __gda_record(ok_value, msg_text) -> void:\n";
  s += I + "if __gda_aborted:\n";
  s += I + I + "return\n";
  s += I + "__gda_checks.append({\"ok\": bool(ok_value), \"message\": "
           "str(msg_text)})\n";
  s += "\n";
  s += "func check(condition, msg = \"\") -> void:\n";
  s += I + "__gda_record(condition, msg)\n";
  s += "\n";
  s += "func check_equal(actual, expected) -> void:\n";
  s += I + "var same_value: bool = actual == expected\n";
  s += I + "if same_value:\n";
  s += I + I + "__gda_record(true, \"check_equal ok\")\n";
  s += I + "else:\n";
  s += I + I + "__gda_record(false, \"check_equal failed: expected \" + "
           "str(expected) + \", got \" + str(actual))\n";
  s += "\n";
  s += "func check_almost_equal(actual, expected, epsilon = 0.001) -> void:\n";
  s += I + "var numeric_pair: bool = (typeof(actual) == TYPE_INT or "
           "typeof(actual) == TYPE_FLOAT) and (typeof(expected) == TYPE_INT "
           "or typeof(expected) == TYPE_FLOAT)\n";
  s += I + "if numeric_pair:\n";
  s += I + I + "var diff: float = absf(float(actual) - float(expected))\n";
  s += I + I + "if diff <= float(epsilon):\n";
  s += I + I + I + "__gda_record(true, \"check_almost_equal ok\")\n";
  s += I + I + "else:\n";
  s += I + I + I + "__gda_record(false, \"check_almost_equal failed: abs "
               "diff \" + str(diff) + \" exceeds epsilon \" + str(epsilon))\n";
  s += I + "else:\n";
  s += I + I + "var same_value: bool = actual == expected\n";
  s += I + I + "if same_value:\n";
  s += I + I + I + "__gda_record(true, \"check_almost_equal ok (non-numeric "
               "arguments fell back to check_equal semantics)\")\n";
  s += I + I + "else:\n";
  s += I + I + I + "__gda_record(false, \"check_almost_equal failed "
               "(non-numeric arguments fell back to check_equal semantics): "
               "expected \" + str(expected) + \", got \" + str(actual))\n";
  s += "\n";
  s += "func fail(msg = \"\") -> void:\n";
  s += I + "__gda_record(false, \"[fail] \" + str(msg))\n";
  s += "\n";
  s += "func fatal(msg = \"\") -> void:\n";
  s += I + "if __gda_aborted:\n";
  s += I + I + "return\n";
  s += I + "__gda_aborted = true\n";
  s += I + "__gda_checks.append({\"ok\": false, \"message\": \"[fatal] \" + "
           "str(msg)})\n";
  s += I + "__gda_sink." + TEST_ABORT_TOKEN + "()\n";
  s += "\n";
  s += "func " + std::string(TEST_BODY_FUNC) + "() -> void:\n";
  return s;
}

bool build_case_wrapped(const std::string &raw_source, WrappedCase &out,
                        std::string &error_out) {
  std::string cleaned = gdscript_wrap::strip_extends_lines(raw_source);

  {
    std::istringstream stream(cleaned);
    std::string line;
    while (std::getline(stream, line)) {
      if (is_comment_or_blank(line))
        continue;
      if (top_level_token_is(line, "func", 4) ||
          top_level_token_is(line, "static", 6) ||
          top_level_token_is(line, "class_name", 10) ||
          top_level_token_is(line, "class", 5)) {
        error_out =
            "test source must be plain sequential statements; top-level "
            "declarations (func/static/class/class_name) are not allowed in "
            "a test body - inline the logic or use code_execute for scripted "
            "helpers";
        return false;
      }
    }
  }

  gdscript_wrap::IndentStyle indent =
      gdscript_wrap::scan_indent_style(cleaned);
  if (indent.uses_tabs && indent.uses_spaces) {
    error_out = "mixed tab/space indentation detected in test source; use "
                "only tabs or only spaces";
    return false;
  }
  std::string I = gdscript_wrap::indent_prefix(indent);

  bool has_content = false;
  {
    std::istringstream stream(cleaned);
    std::string line;
    while (std::getline(stream, line)) {
      if (is_comment_or_blank(line))
        continue;
      has_content = true;
      break;
    }
  }

  std::string skeleton = skeleton_header(I);
  out.header_lines = static_cast<int>(
      std::count(skeleton.begin(), skeleton.end(), '\n'));
  out.wrapped =
      skeleton +
      (has_content ? gdscript_wrap::reindent_lines(cleaned, I) : I + "pass") +
      "\n";
  return true;
}

bool compile_case_script(godot::Ref<godot::GDScript> &script,
                         const WrappedCase &w, std::string &error_out) {
  script.instantiate();
  if (script.is_null()) {
    error_out = "failed to create GDScript instance";
    return false;
  }

  script->set_source_code(godot::String(w.wrapped.c_str()));

  size_t compile_log_before = debugger_ops::capture_log_count();
  godot::Error parse_err = script->reload();
  if (parse_err != godot::OK) {
    int code = static_cast<int>(parse_err);
    std::string err_name = "ERR_UNKNOWN";
    if (code == 43)
      err_name = "ERR_PARSE_ERROR";
    error_out = gdscript_wrap::compose_compile_failure_message(
        "GDScript compilation failed: " + err_name +
            " (code " + std::to_string(code) + ")",
        debugger_ops::capture_new_error_text(compile_log_before), w.wrapped,
        [&](const std::string &captured) {
          return gdscript_wrap::truncate_capture_text(
                     map_line_numbers(captured, w.header_lines)) +
                 "\nerror lines above were mapped to user source line numbers";
        });
    return false;
  }
  return true;
}

struct CaseOutcome {
  mcp::JsonValue checks{mcp::JsonValue::array_tag};
  bool aborted = false;
  std::string runtime_error;
};

bool execute_case_script(const godot::Ref<godot::GDScript> &script,
                         int header_lines, CaseOutcome &out,
                         std::string &error_out) {
  size_t log_before = debugger_ops::capture_log_count();

  godot::Node *scene_root_for_leak = nullptr;
  int child_count_before = 0;
  {
    auto *editor = godot::EditorInterface::get_singleton();
    if (editor)
      scene_root_for_leak = editor->get_edited_scene_root();
  }
  if (scene_root_for_leak)
    child_count_before = scene_root_for_leak->get_child_count();

  {
    godot::Node *temp_node = nullptr;
    TempNodeGuard temp_guard(temp_node);

    temp_node = memnew(godot::Node);
    temp_node->set_script(godot::Variant(script));

    godot::Node *parent_node = nullptr;
    {
      auto *engine = godot::Engine::get_singleton();
      auto *main_loop = engine ? engine->get_main_loop() : nullptr;
      auto *tree = godot::Object::cast_to<godot::SceneTree>(main_loop);
      if (tree)
        parent_node = godot::Object::cast_to<godot::Node>(tree->get_root());
    }
    if (!parent_node)
      parent_node = scene_root_for_leak;
    if (parent_node)
      parent_node->add_child(temp_node);

    godot::StringName fn_name(TEST_BODY_FUNC);
    if (!temp_node->has_method(fn_name)) {
      error_out =
          std::string("function not found in compiled script: ") +
          TEST_BODY_FUNC;
      return false;
    }

    temp_node->call(fn_name);

    godot::Variant checks_v = temp_node->get("__gda_checks");
    godot::Variant aborted_v = temp_node->get("__gda_aborted");
    if (checks_v.get_type() == godot::Variant::ARRAY)
      out.checks = VariantJson::serialize(checks_v);
    out.aborted =
        aborted_v.get_type() == godot::Variant::BOOL && aborted_v.operator bool();

    std::string captured =
        filter_abort_noise(debugger_ops::capture_new_error_text(log_before));
    if (!captured.empty()) {
      out.runtime_error = gdscript_wrap::truncate_capture_text(
          map_line_numbers(captured, header_lines));
    }
  }

  if (scene_root_for_leak &&
      scene_root_for_leak->get_child_count() > child_count_before) {
    auto children = scene_root_for_leak->get_children();
    for (int i = children.size() - 1; i >= 0; --i) {
      auto *child = godot::Object::cast_to<godot::Node>(children[i]);
      if (child && child->get_owner() == nullptr) {
        scene_root_for_leak->remove_child(child);
        memdelete(child);
      }
    }
  }
  return true;
}

struct TestCaseSpec {
  std::string name;
  std::string source;
  bool invalid = false;
  std::string invalid_reason;
};

struct CaseResult {
  std::string name;
  std::string status;
  mcp::JsonValue checks{mcp::JsonValue::array_tag};
  std::string error;
  std::string reason;
  int64_t elapsed_ms = 0;
  bool running_over_budget = false;
};

bool case_has_failed_check(const mcp::JsonValue &checks) {
  if (!checks.IsArray())
    return false;
  for (const auto &c : checks.GetArray()) {
    const auto *ok = c.Find("ok");
    if (ok && ok->IsBool() && !ok->GetBool())
      return true;
  }
  return false;
}

mcp::JsonValue case_to_json(const CaseResult &cr) {
  mcp::JsonValue o(mcp::JsonValue::object_tag);
  o["name"] = mcp::JsonValue(cr.name);
  o["status"] = mcp::JsonValue(cr.status);
  o["checks"] = cr.checks;
  o["elapsed_ms"] = mcp::JsonValue(cr.elapsed_ms);
  if (!cr.error.empty())
    o["error"] = mcp::JsonValue(cr.error);
  if (!cr.reason.empty())
    o["reason"] = mcp::JsonValue(cr.reason);
  if (cr.running_over_budget)
    o["running_over_budget"] = mcp::JsonValue(true);
  return o;
}

mcp::JsonValue run_suite(const std::vector<TestCaseSpec> &specs,
                         int timeout_ms) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "gdscript test suite executing " +
                                std::to_string(specs.size()) + " case(s)");

  auto suite_start = std::chrono::steady_clock::now();
  auto since_start = [&] {
    return ms_between(suite_start, std::chrono::steady_clock::now());
  };

  mcp::JsonValue cases(mcp::JsonValue::array_tag);
  int64_t passed = 0;
  int64_t failed = 0;
  int64_t skipped = 0;
  bool any_over_budget = false;

  for (const auto &spec : specs) {
    CaseResult cr;
    cr.name = spec.name;

    if (spec.invalid) {
      cr.status = "fail";
      cr.error = spec.invalid_reason;
      cases.PushBack(case_to_json(cr));
      ++failed;
      continue;
    }

    if (since_start() >= timeout_ms) {
      cr.status = "skip";
      cr.reason = "timeout budget exhausted";
      cases.PushBack(case_to_json(cr));
      ++skipped;
      continue;
    }

    auto case_start = std::chrono::steady_clock::now();
    auto finish_case = [&](bool is_fail) {
      cr.elapsed_ms =
          ms_between(case_start, std::chrono::steady_clock::now());
      if (since_start() > timeout_ms) {
        cr.running_over_budget = true;
        any_over_budget = true;
      }
      cases.PushBack(case_to_json(cr));
      if (is_fail)
        ++failed;
      else
        ++passed;
    };

    cr.checks = mcp::JsonValue(mcp::JsonValue::array_tag);

    WrappedCase w;
    std::string error_out;
    if (!build_case_wrapped(spec.source, w, error_out)) {
      cr.status = "fail";
      cr.error = error_out;
      finish_case(true);
      continue;
    }

    godot::Ref<godot::GDScript> script;
    if (!compile_case_script(script, w, error_out)) {
      cr.status = "fail";
      cr.error = error_out;
      finish_case(true);
      continue;
    }

    CaseOutcome outcome;
    if (!execute_case_script(script, w.header_lines, outcome, error_out)) {
      cr.status = "fail";
      cr.error = error_out;
      finish_case(true);
      continue;
    }

    cr.checks = outcome.checks;
    if (!outcome.runtime_error.empty()) {
      cr.status = "fail";
      cr.error = outcome.runtime_error;
    } else if (outcome.aborted || case_has_failed_check(outcome.checks)) {
      cr.status = "fail";
    } else {
      cr.status = "pass";
    }
    finish_case(cr.status == "fail");
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  mcp::JsonValue summary(mcp::JsonValue::object_tag);
  summary["total"] = mcp::JsonValue(static_cast<int64_t>(specs.size()));
  summary["passed"] = mcp::JsonValue(passed);
  summary["failed"] = mcp::JsonValue(failed);
  summary["skipped"] = mcp::JsonValue(skipped);
  r["summary"] = std::move(summary);
  r["cases"] = std::move(cases);
  if (any_over_budget) {
    r["note"] = mcp::JsonValue(
        "one or more cases finished past the timeout budget: GDScript "
        "execution is synchronous, so the currently running case cannot be "
        "interrupted; remaining cases were skipped with reason 'timeout "
        "budget exhausted'");
  }
  return r;
}

std::string normalize_res_dir(const std::string &dir) {
  std::string d = dir;
  if (d.rfind("res://", 0) != 0) {
    while (!d.empty() && d.front() == '/')
      d.erase(d.begin());
    d = "res://" + d;
  }
  while (d.size() > 6 && d.back() == '/')
    d.pop_back();
  return d;
}

std::string join_dir_file(const std::string &dir, const std::string &name) {
  return dir + (dir.empty() || dir.back() == '/' ? std::string() : "/") +
         name;
}

bool wildcard_match(const std::string &text, const std::string &pattern) {
  size_t t = 0, p = 0;
  size_t star = std::string::npos, mark = 0;
  while (t < text.size()) {
    if (p < pattern.size() &&
        (pattern[p] == '?' || pattern[p] == text[t])) {
      ++t;
      ++p;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      mark = t;
    } else if (star != std::string::npos) {
      p = star + 1;
      t = ++mark;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*')
    ++p;
  return p == pattern.size();
}

} // namespace

namespace godot_autopilot {
namespace test_ops {

mcp::JsonValue handle_run_gdscript_tests(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "run_gdscript_tests called");

  auto *tests = args.Find("tests");
  if (!tests || !tests->IsArray()) {
    return util::error_json(
        "missing required parameter: tests (array of {\"name\": string, "
        "\"source\": string})");
  }

  int timeout_ms = parse_timeout_ms(args);

  std::vector<TestCaseSpec> specs;
  const auto &arr = tests->GetArray();
  specs.reserve(arr.size());
  for (size_t i = 0; i < arr.size(); ++i) {
    TestCaseSpec spec;
    const auto &item = arr[i];
    const mcp::JsonValue *name_v = nullptr;
    const mcp::JsonValue *src_v = nullptr;
    if (item.IsObject()) {
      name_v = item.Find("name");
      src_v = item.Find("source");
    }
    if (!name_v || !name_v->IsString() || !src_v || !src_v->IsString()) {
      spec.invalid = true;
      spec.invalid_reason =
          "invalid test item at index " + std::to_string(i) +
          ": expected an object with string fields 'name' and 'source'";
      spec.name = (name_v && name_v->IsString())
                      ? name_v->GetString()
                      : "<index " + std::to_string(i) + ">";
    } else {
      spec.name = name_v->GetString();
      spec.source = src_v->GetString();
    }
    specs.push_back(std::move(spec));
  }

  mcp::JsonValue r = run_suite(specs, timeout_ms);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "run_gdscript_tests completed");
  return r;
}

mcp::JsonValue handle_run_gdscript_test_files(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "run_gdscript_test_files called");

  std::string directory = "res://tests";
  if (auto *d = args.Find("directory")) {
    if (d->IsString())
      directory = d->GetString();
  }
  std::string pattern = "*.gda_test.gd";
  if (auto *p = args.Find("pattern")) {
    if (p->IsString())
      pattern = p->GetString();
  }
  int timeout_ms = parse_timeout_ms(args);

  std::string dir_norm = normalize_res_dir(directory);
  if (!godot::DirAccess::dir_exists_absolute(
          godot::String(dir_norm.c_str()))) {
    return util::error_json("directory not found: " + dir_norm);
  }
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir_norm.c_str()));
  if (da.is_null()) {
    return util::error_json("failed to open directory: " + dir_norm);
  }

  std::vector<std::string> filenames;
  da->list_dir_begin();
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != ".." && !da->current_is_dir()) {
      std::string name = util::to_std(entry);
      if (wildcard_match(name, pattern))
        filenames.push_back(name);
    }
    entry = da->get_next();
  }
  da->list_dir_end();
  std::sort(filenames.begin(), filenames.end());

  std::vector<TestCaseSpec> specs;
  std::vector<std::string> files;
  specs.reserve(filenames.size());
  files.reserve(filenames.size());
  for (const auto &fname : filenames) {
    std::string fpath = join_dir_file(dir_norm, fname);
    files.push_back(fpath);
    TestCaseSpec spec;
    spec.name = fpath;
    auto file = godot::FileAccess::open(godot::String(fpath.c_str()),
                                        godot::FileAccess::READ);
    if (file.is_null()) {
      spec.invalid = true;
      spec.invalid_reason = "failed to read file: " + fpath;
    } else {
      spec.source = util::to_std(file->get_as_text());
      file->close();
    }
    specs.push_back(std::move(spec));
  }

  mcp::JsonValue r = run_suite(specs, timeout_ms);
  mcp::JsonValue files_arr(mcp::JsonValue::array_tag);
  for (const auto &f : files)
    files_arr.PushBack(mcp::JsonValue(f));
  r["files"] = std::move(files_arr);
  if (files.empty()) {
    r["note"] = mcp::JsonValue("no files matched pattern '" + pattern +
                               "' in directory '" + dir_norm + "'");
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "run_gdscript_test_files completed");
  return r;
}

} // namespace test_ops
} // namespace godot_autopilot
