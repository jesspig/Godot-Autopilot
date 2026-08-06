
//

//   1. 崩溃检测（核心）：348 个领域工具全部经 call_tool 代理，handler
//   在引擎主线程执行。
//      handler 的任何 godot API 误用/空指针/类型转换问题都会杀死编辑器进程 →
//      editor_alive() 立即检测到 → FAIL
//      并携带工具名与编辑器日志。编辑器进程死亡后
//      后续请求全部失败，循环在崩溃点即中断，崩溃工具名即根因。
//   2. 参数契约：schema 必填参数（get_tool_detail 的 input_schema.required）vs
//   空参数
//      调用的实际报错行为一致性；同时端到端验证 get_tool_detail
//      对全部工具的覆盖 （catalog 完整性）。已知契约缺口（不
//      FAIL，仅统计/打印警告清单）： schema 声明必填但 handler
//      不校验的工具——当前实现允许带默认值的必填参数
//      或静默降级（scene_node_create 的 name/type 有默认值 NewNode/Node；
//      resource_get_extensions 缺 type 时以空串查询返回全类型扩展列表；
//      resource_reimport 空参时 count=0 静默成功）。此类缺口只记录在
//      required_ignored 统计与 stdout 清单，测试失败仅由崩溃或非 JSON
//      响应触发。
//   3. 启发式参数冒烟：按 schema properties 生成合法类型参数调用 → 不崩溃 +
//   结构化
//      JSON 响应（result 或 error 均接受——headless 编辑器无物理/无场景/无窗口，
//      大量工具在合法输入下也返回环境性 error，重点是进程存活）。
//

//   本套件对 348 个工具全量遍历，空参契约对"全部"工具（含 SCHEMA_NONE）调用，
//   冒烟对"有 schema properties"的工具调用——其中一部分工具会写盘或干扰用户
//   桌面环境。曾因此污染测试项目 Example/ 并破坏用户使用体验：
//     - editor_set_main_scene 冒烟以 {"path":"test"} 调用 →
//       ProjectSettings::set_setting + save 把 application/run/main_scene 写成
//       "test" 写入 Example/project.godot → 用户 GUI
//       打开项目弹"主场景缺失"对话框
//     - editor_save_scene（空 schema、空参契约调用）→ 场景无文件路径时
//       save_scene_as("res://<root名>.tscn") 生成 Example/NewNode.tscn
//     - os_alert 冒烟以 {"text":"test","title":"test"} 调用 →
//     弹出系统模态对话框
//     - display_clipboard_set 冒烟 → 覆盖系统剪贴板
//     - display_tts_speak 冒烟 → 系统朗读 "test"
//   修复：两类工具集中列入排除清单，空参契约与冒烟两个遍历循环对清单工具跳过
//   （不调用），计入 stats.excluded 统计并在 stdout 列出工具名与原因。
//   清单分两类：
//   (1) 持久磁盘副作用（写 project.godot / editor_settings / .tscn / 文件）：
//       判定依据 src/ 实现中确认的写盘路径（ProjectSettings::set_setting +
//       save、 EditorSettings::set_setting、editor->save_scene*、FileAccess
//       WRITE、 ResourceSaver::save）。code_execute
//       经核实安全不入清单：冒烟参数
//       {"source_code":"test"} 包装后编译失败（裸标识符非合法 GDScript 语句，
//       code_exec_ops.cpp:460-483）→ 返回 error，不执行、不写文件。
//   (2) 用户可见副作用（弹窗/进程/音频/剪贴板/鼠标/窗口）：
//       任意合法输入下会干扰用户桌面环境——弹窗抢焦点、启动外部进程、播放
//       语音、修改剪贴板、移动/锁定鼠标、改编辑器窗口属性。判定依据 src/
//       实现中确认的 OS/DisplayServer 调用。只读工具（mouse_get_position、
//       window_get_*、clipboard_get、tts_get_voices、screen_*_get、os_get_*、
//       screen_capture）不入清单。
//

//   - 全部 EXPECT/ASSERT 失败由 gtest 报告，失败消息均含工具名上下文
//   - 汇总统计经 RecordProperty 写入 gtest XML（key:
//   empty_args_*/smoke_*）并打印 stdout
//     （总数 / result 数 / error 数 / error 含 "missing required" 数 /
//     必填未报错清单 / 非 JSON 响应清单 / 跳过数）
//

//   src/tools/tool_defs.def 的 TOOL_ENTRY(id, "name", ...) —— name 为第 2
//   个引号字段。 为何不动态获取（已逐一读实现确认不可行）：
//     - 领域工具不注册到 MCP server（register_all.cpp 仅 7 处 RegisterTool），
//       tools/list 只返回 7 个元工具
//     - search_tools 空 query → Bm25Index::search
//     直接返回空列表（bm25_index.cpp:58）
//     - get_tool_detail 需要已知 name（catalog 无列举接口）
//

//   - 领域工具缺参错误格式 {"error":"missing required parameter: <名>"}，
//     个别带类型后缀（"operations (array)"、"fps (integer)"、"monitor (integer
//     0-58)"）， input_map 系用 "missing required field 'class'"——断言统一匹配
//     "missing required"
//   - call_tool 元工具：name 必填（缺省 MCP 层 is_error），arguments
//   缺省为空对象
//   - call_handler 捕获 handler 异常 → {"error":"internal error ... (args: 截断
//   256B)"}
//     （register_all.cpp:1543-1549）——不构造崩溃输入主动触发该路径
//   - get_tool_detail 返回
//   {"tool":{name,description,category,tags,input_schema}}
//   - schema 结构 {"type":"object","properties":{...},"required":[...]}
//     （schema_builder.cpp build_schema：无必填参数时 required 字段为空数组）
//   - 元工具直连 handler（ping 等）不设 MCP is_error（返回 error JSON 但
//     last_call_was_error()==false）——本套件基于响应文本 Find("error")
//     判定，不受影响

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include "godot_fixture.hpp"
#include "mcp_test_client.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifndef PROJECT_ROOT
#error                                                                         \
    "PROJECT_ROOT 编译宏未定义（tests/CMakeLists.txt 已为 gsd_engine_tests 配置）"
#endif

namespace {

// ── 常量（来源：tool_defs.def 行数 / register_all.cpp meta_tool_names） ──
constexpr size_t kExpectedDomainToolCount = 348;
constexpr size_t kExpectedMetaToolCount = 7;
constexpr const char *kMetaToolNames[kExpectedMetaToolCount] = {
    "ping",      "search_tools",  "list_categories", "get_tool_detail",
    "call_tool", "batch_execute", "code_execute"};

// ── 副作用排除清单 ──

constexpr const char *kPersistentSideEffectTools[] = {
    // ── 持久磁盘副作用（写 project.godot / editor_settings / .tscn / 文件） ──

    "editor_set_main_scene",
    "editor_set_plugin_enabled",
    "project_settings_save",
    "input_map_action_add_event",
    "input_map_persist",

    "editor_settings_set",

    "editor_save_scene",
    "editor_save_all_scenes",
    "editor_save_scene_as",

    "editor_new_text_resource",
    "file_write",
    "script_create",
    "resource_save",

    // ── 用户可见副作用（弹窗/进程/环境变量/音频/剪贴板/鼠标/窗口） ──

    "os_alert",
    "display_dialog_show",

    "os_create_process",
    "os_execute",
    "os_kill",
    "os_shell_open",
    "os_move_to_trash",

    "os_set_environment",

    "display_tts_speak",
    "display_tts_stop",

    "display_clipboard_set",
    "display_mouse_set_mode",
    "display_mouse_warp",

    "display_window_set_title",
    "display_window_set_position",
    "display_window_set_size",
    "display_window_set_mode",
    "display_window_set_flag",
    "display_window_move_to_foreground",
    "display_window_request_attention",
    "display_window_create",
    "display_window_delete",
};

bool is_persistent_side_effect_tool(const std::string &name) {
  for (const char *excluded : kPersistentSideEffectTools) {
    if (name == excluded)
      return true;
  }
  return false;
}

// ── 工具名单：运行时解析 tool_defs.def ──

std::vector<std::string> parse_tool_names_from_def() {
  const std::string path =
      std::string(PROJECT_ROOT) + "/src/tools/tool_defs.def";
  std::ifstream in(path);
  if (!in.is_open()) {
    ADD_FAILURE() << "无法打开 " << path;
    return {};
  }
  std::vector<std::string> names;
  std::string line;
  while (std::getline(in, line)) {
    const size_t p = line.find("TOOL_ENTRY(");
    if (p == std::string::npos)
      continue;
    const size_t comma = line.find(',', p);
    if (comma == std::string::npos)
      continue;
    const size_t q1 = line.find('"', comma);
    if (q1 == std::string::npos)
      continue;
    const size_t q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos)
      continue;
    names.push_back(line.substr(q1 + 1, q2 - q1 - 1));
  }
  return names;
}

// ── JSON 辅助 ──

mcp::JsonValue parse_response(const std::string &text) {
  return mcp::JsonValue::Parse(text);
}

bool has_error_field(const mcp::JsonValue &j) {
  return j.IsObject() && j.Find("error") != nullptr;
}

std::vector<std::string> schema_required_params(const mcp::JsonValue &detail) {
  std::vector<std::string> out;
  const auto *tool = detail.Find("tool");
  if (!tool || !tool->IsObject())
    return out;
  const auto *schema = tool->Find("input_schema");
  if (!schema || !schema->IsObject())
    return out;
  const auto *req = schema->Find("required");
  if (!req || !req->IsArray())
    return out;
  for (const auto &v : req->GetArray()) {
    if (v.IsString())
      out.push_back(v.GetString());
  }
  return out;
}

std::vector<std::pair<std::string, std::string>>
schema_property_types(const mcp::JsonValue &detail) {
  std::vector<std::pair<std::string, std::string>> out;
  const auto *tool = detail.Find("tool");
  if (!tool || !tool->IsObject())
    return out;
  const auto *schema = tool->Find("input_schema");
  if (!schema || !schema->IsObject())
    return out;
  const auto *props = schema->Find("properties");
  if (!props || !props->IsObject())
    return out;
  for (const auto &[key, value] : props->GetObject()) {
    std::string type = "string";
    if (value.IsObject()) {
      if (const auto *t = value.Find("type")) {
        if (t->IsString())
          type = t->GetString();
      }
    }
    out.emplace_back(key, type);
  }
  return out;
}

mcp::JsonValue heuristic_value(const std::string &type) {
  if (type == "integer")
    return mcp::JsonValue(static_cast<int64_t>(0));
  if (type == "number")
    return mcp::JsonValue(0.0);
  if (type == "boolean")
    return mcp::JsonValue(false);
  if (type == "array")
    return mcp::JsonValue(mcp::JsonValue::array_tag);
  if (type == "object")
    return mcp::JsonValue(mcp::JsonValue::object_tag);
  return mcp::JsonValue("test");
}

// ── 统计结构 ──
struct ToolStats {
  size_t total = 0;
  size_t results = 0;
  size_t errors = 0;
  size_t missing_req = 0;
  size_t required_ignored = 0;
  size_t skipped = 0;
  size_t excluded = 0;
  std::vector<std::string> required_ignored_names;
  std::vector<std::string> non_object_names;
  std::vector<std::string> excluded_names;
};

void print_stats(const char *label, const ToolStats &s) {
  std::cout << "\n=== " << label << " ===\n"
            << "  调用总数: " << s.total << " | result: " << s.results
            << " | error: " << s.errors
            << " | error 含 'missing required': " << s.missing_req
            << " | schema 必填但未报错: " << s.required_ignored
            << " | 跳过: " << s.skipped << " | 排除(副作用): " << s.excluded
            << "\n";
  for (const auto &n : s.required_ignored_names) {
    std::cout << "  [schema 必填但空参未报错] " << n << "\n";
  }
  for (const auto &n : s.non_object_names) {
    std::cout << "  [响应非 JSON 对象] " << n << "\n";
  }
  for (const auto &n : s.excluded_names) {
    std::cout << "  [排除-副作用，跳过调用] " << n << "\n";
  }
}

mcp::JsonValue call_domain_tool(gsd_test::McpTestClient &client,
                                const std::string &name,
                                const mcp::JsonValue &args,
                                std::string *raw_out = nullptr) {
  mcp::JsonValue envelope(mcp::JsonValue::object_tag);
  envelope["name"] = mcp::JsonValue(name);
  envelope["arguments"] = args;
  const std::string text = client.call_tool("call_tool", envelope.Dump());
  if (raw_out)
    *raw_out = text;
  return parse_response(text);
}

} // namespace

class ToolSchemaContractTest : public gsd_test::GodotEditorFixture {
protected:
  void run_heuristic_smoke(size_t begin, size_t end, const char *label);
};

// ── 主用例 1：全部 348 个领域工具"空参数契约"遍历 ──

TEST_F(ToolSchemaContractTest, EmptyArgsContractAcrossAllDomainTools) {
  const std::vector<std::string> names = parse_tool_names_from_def();
  ASSERT_FALSE(names.empty()) << "无法从 tool_defs.def 解析工具名";
  EXPECT_EQ(names.size(), kExpectedDomainToolCount)
      << "工具数量与 tool_defs.def 不一致（若 def 有增减需同步本基线）";

  gsd_test::McpTestClient client(port());
  ASSERT_TRUE(client.connect());

  ToolStats stats;
  for (const auto &name : names) {

    mcp::JsonValue detail_args(mcp::JsonValue::object_tag);
    detail_args["name"] = mcp::JsonValue(name);
    const std::string detail_text =
        client.call_tool("get_tool_detail", detail_args.Dump());
    ASSERT_TRUE(editor_alive())
        << "get_tool_detail('" << name << "') 后编辑器进程死亡！日志：\n"
        << capture_logs();
    const mcp::JsonValue detail = parse_response(detail_text);
    if (!detail.IsObject()) {
      EXPECT_TRUE(false) << "get_tool_detail('" << name
                         << "') 响应非 JSON 对象: "
                         << detail_text.substr(0, 300);
      continue;
    }
    const auto *tool = detail.Find("tool");
    if (!tool || !tool->IsObject()) {
      EXPECT_TRUE(false) << "get_tool_detail('" << name
                         << "') 缺少 tool 字段: " << detail_text.substr(0, 300);
      continue;
    }
    const auto *tool_name = tool->Find("name");
    EXPECT_TRUE(tool_name != nullptr && tool_name->IsString() &&
                tool_name->GetString() == name)
        << "get_tool_detail('" << name << "') 返回的工具名不匹配";
    const std::vector<std::string> required = schema_required_params(detail);

    //    会写项目配置/文件系统，或干扰用户桌面环境，跳过实际调用；
    //    get_tool_detail（只读）已在上方执行，catalog 覆盖性验证不受影响。
    if (is_persistent_side_effect_tool(name)) {
      stats.excluded_names.push_back(name);
      ++stats.excluded;
      continue;
    }

    std::string raw;
    const mcp::JsonValue resp = call_domain_tool(
        client, name, mcp::JsonValue(mcp::JsonValue::object_tag), &raw);
    ASSERT_TRUE(editor_alive())
        << "工具 '" << name << "' 空参数调用后编辑器进程死亡！日志：\n"
        << capture_logs();
    ++stats.total;
    if (!resp.IsObject()) {
      stats.non_object_names.push_back(name);
      EXPECT_TRUE(false) << "工具 '" << name
                         << "' 空参数响应非 JSON 对象: " << raw.substr(0, 300);
      continue;
    }
    const bool errored = has_error_field(resp);
    if (errored) {
      ++stats.errors;
      const auto *err = resp.Find("error");
      const std::string msg = err && err->IsString() ? err->GetString() : "";
      if (msg.find("missing required") != std::string::npos) {
        ++stats.missing_req;
      }
    } else {
      ++stats.results;
    }

    //    → 已确认的业务缺陷（handler 不校验带默认值的必填参数），列入
    //    required_ignored 统计与 stdout 清单（print_stats）。本用例 FAIL
    //    仅由崩溃（上方 ASSERT）或响应非 JSON 对象（上方 EXPECT）触发。
    if (!required.empty() && !errored) {
      stats.required_ignored_names.push_back(name);
      ++stats.required_ignored;
    }
  }

  print_stats("主用例 1：空参数契约（全部领域工具）", stats);
  RecordProperty("empty_args_total", static_cast<int>(stats.total));
  RecordProperty("empty_args_results", static_cast<int>(stats.results));
  RecordProperty("empty_args_errors", static_cast<int>(stats.errors));
  RecordProperty("empty_args_missing_req", static_cast<int>(stats.missing_req));
  RecordProperty("empty_args_required_ignored",
                 static_cast<int>(stats.required_ignored));
  RecordProperty("empty_args_excluded", static_cast<int>(stats.excluded));
  RecordProperty("empty_args_crashes", 0);
}

// ── 主用例 2：合法参数启发式生成冒烟（拆分两段，崩溃隔离） ──

void ToolSchemaContractTest::run_heuristic_smoke(size_t begin, size_t end,
                                                 const char *label) {
  std::vector<std::string> names = parse_tool_names_from_def();
  ASSERT_FALSE(names.empty()) << "无法从 tool_defs.def 解析工具名";
  std::sort(names.begin(), names.end());
  end = std::min(end, names.size());
  ASSERT_LT(begin, end) << "冒烟段区间为空";

  gsd_test::McpTestClient client(port());
  ASSERT_TRUE(client.connect());

  ToolStats stats;
  for (size_t i = begin; i < end; ++i) {
    const std::string &name = names[i];

    //    会写项目配置/文件系统，或干扰用户桌面环境，直接跳过，不做
    //    detail/调用。
    if (is_persistent_side_effect_tool(name)) {
      stats.excluded_names.push_back(name);
      ++stats.excluded;
      continue;
    }
    mcp::JsonValue detail_args(mcp::JsonValue::object_tag);
    detail_args["name"] = mcp::JsonValue(name);
    const std::string detail_text =
        client.call_tool("get_tool_detail", detail_args.Dump());
    ASSERT_TRUE(editor_alive())
        << "get_tool_detail('" << name << "') 后编辑器进程死亡！日志：\n"
        << capture_logs();
    const mcp::JsonValue detail = parse_response(detail_text);
    if (!detail.IsObject()) {
      stats.non_object_names.push_back(name);
      EXPECT_TRUE(false) << "get_tool_detail('" << name
                         << "') 响应非 JSON 对象: "
                         << detail_text.substr(0, 300);
      continue;
    }
    const auto props = schema_property_types(detail);
    if (props.empty()) {
      ++stats.skipped;
      continue;
    }
    mcp::JsonValue args(mcp::JsonValue::object_tag);
    for (const auto &[key, type] : props) {
      args[key] = heuristic_value(type);
    }
    std::string raw;
    const mcp::JsonValue resp = call_domain_tool(client, name, args, &raw);
    ASSERT_TRUE(editor_alive())
        << "工具 '" << name << "' 启发式参数调用后编辑器进程死亡！日志：\n"
        << capture_logs();
    ++stats.total;
    if (!resp.IsObject()) {
      stats.non_object_names.push_back(name);
      EXPECT_TRUE(false) << "工具 '" << name << "' 启发式参数响应非 JSON 对象: "
                         << raw.substr(0, 300);
      continue;
    }
    if (has_error_field(resp)) {
      ++stats.errors;
    } else {
      ++stats.results;
    }
  }

  print_stats(label, stats);
  RecordProperty("smoke_total", static_cast<int>(stats.total));
  RecordProperty("smoke_skipped", static_cast<int>(stats.skipped));
  RecordProperty("smoke_excluded", static_cast<int>(stats.excluded));
  RecordProperty("smoke_crashes", 0);
}

TEST_F(ToolSchemaContractTest, HeuristicArgsSmokePart1) {
  run_heuristic_smoke(0, kExpectedDomainToolCount / 2,
                      "主用例 2a：启发式参数冒烟（字典序前 174）");
}

TEST_F(ToolSchemaContractTest, HeuristicArgsSmokePart2) {
  run_heuristic_smoke(kExpectedDomainToolCount / 2, kExpectedDomainToolCount,
                      "主用例 2b：启发式参数冒烟（字典序后 174）");
}

// ── 主用例 3：7 个元工具直连自检 + 边界输入 ──
TEST_F(ToolSchemaContractTest, MetaToolsDirectContract) {
  gsd_test::McpTestClient client(port());
  ASSERT_TRUE(client.connect());

  const auto tools = client.list_tools();
  EXPECT_EQ(tools.size(), kExpectedMetaToolCount)
      << "MCP tools/list 应只返回 7 个元工具（领域工具不注册到 server）";
  for (const char *name : kMetaToolNames) {
    bool found = false;
    for (const auto &t : tools) {
      if (t.name == name)
        found = true;
    }
    EXPECT_TRUE(found) << "元工具缺失: " << name;
  }

  {
    const mcp::JsonValue j = parse_response(client.call_tool("ping"));
    ASSERT_TRUE(j.IsObject());
    const auto *r = j.Find("result");
    EXPECT_TRUE(r != nullptr && r->IsString() && r->GetString() == "pong")
        << "ping 响应: " << j.Dump();
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j = parse_response(client.call_tool("search_tools"));
    ASSERT_TRUE(j.IsObject());
    const auto *err = j.Find("error");
    ASSERT_NE(err, nullptr);
    ASSERT_TRUE(err->IsString());
    EXPECT_NE(err->GetString().find("missing required parameter: query"),
              std::string::npos)
        << "search_tools 缺 query 错误消息: " << err->GetString();
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j = parse_response(
        client.call_tool("search_tools", R"({"query":"physics"})"));
    ASSERT_TRUE(j.IsObject());
    const auto *results = j.Find("results");
    ASSERT_TRUE(results != nullptr && results->IsArray())
        << "search_tools 响应: " << j.Dump();
    EXPECT_FALSE(results->GetArray().empty()) << "query=physics 应命中工具";
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j =
        parse_response(client.call_tool("search_tools", R"({"query":""})"));
    ASSERT_TRUE(j.IsObject());
    const auto *results = j.Find("results");
    ASSERT_TRUE(results != nullptr && results->IsArray())
        << "search_tools 响应: " << j.Dump();
    EXPECT_TRUE(results->GetArray().empty()) << "空 query 应返回空列表";
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j =
        parse_response(client.call_tool("list_categories"));
    ASSERT_TRUE(j.IsObject());
    const auto *cats = j.Find("categories");
    ASSERT_TRUE(cats != nullptr && cats->IsArray())
        << "list_categories 响应: " << j.Dump();
    EXPECT_FALSE(cats->GetArray().empty());
    for (const auto &c : cats->GetArray()) {
      EXPECT_TRUE(c.IsObject() && c.Find("id") != nullptr &&
                  c.Find("tool_count") != nullptr)
          << "categories 条目缺少 id/tool_count: " << c.Dump();
    }
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j =
        parse_response(client.call_tool("get_tool_detail"));
    ASSERT_TRUE(j.IsObject());
    const auto *err = j.Find("error");
    ASSERT_NE(err, nullptr);
    ASSERT_TRUE(err->IsString());
    EXPECT_NE(err->GetString().find("missing required parameter: name"),
              std::string::npos)
        << "get_tool_detail 缺 name 错误消息: " << err->GetString();
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j = parse_response(
        client.call_tool("get_tool_detail", R"({"name":"no_such_tool_xyz"})"));
    ASSERT_TRUE(j.IsObject());
    const auto *err = j.Find("error");
    ASSERT_NE(err, nullptr);
    ASSERT_TRUE(err->IsString());
    EXPECT_NE(err->GetString().find("tool not found"), std::string::npos)
        << "get_tool_detail 未知工具错误消息: " << err->GetString();
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j = parse_response(
        client.call_tool("get_tool_detail", R"({"name":"scene_node_create"})"));
    ASSERT_TRUE(j.IsObject());
    const auto *tool = j.Find("tool");
    ASSERT_TRUE(tool != nullptr && tool->IsObject()) << "响应: " << j.Dump();
    const auto *tn = tool->Find("name");
    ASSERT_TRUE(tn != nullptr && tn->IsString());
    EXPECT_EQ(tn->GetString(), "scene_node_create");
    const auto *schema = tool->Find("input_schema");
    ASSERT_TRUE(schema != nullptr && schema->IsObject())
        << "input_schema 缺失: " << tool->Dump();
    EXPECT_TRUE(schema->Find("properties") != nullptr &&
                schema->Find("properties")->IsObject())
        << "input_schema 缺少 properties: " << schema->Dump();
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j =
        parse_response(client.call_tool("call_tool", R"({})"));
    ASSERT_TRUE(j.IsObject());
    const auto *err = j.Find("error");
    ASSERT_NE(err, nullptr);
    ASSERT_TRUE(err->IsString());
    EXPECT_NE(err->GetString().find("missing required parameter: name"),
              std::string::npos)
        << "call_tool 缺 name 错误消息: " << err->GetString();
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j = parse_response(
        client.call_tool("call_tool", R"({"name":"no_such_tool_xyz"})"));
    ASSERT_TRUE(j.IsObject());
    const auto *err = j.Find("error");
    ASSERT_NE(err, nullptr);
    ASSERT_TRUE(err->IsString());
    EXPECT_NE(err->GetString().find("domain tool 'no_such_tool_xyz' not found"),
              std::string::npos)
        << "call_tool 未知工具错误消息: " << err->GetString();
    ASSERT_TRUE(editor_alive());
  }

  {
    const std::string big_name(512, 'x');
    mcp::JsonValue args(mcp::JsonValue::object_tag);
    args["name"] = mcp::JsonValue(big_name);
    const std::string raw = client.call_tool("call_tool", args.Dump());
    ASSERT_TRUE(editor_alive()) << "超长工具名调用后编辑器进程死亡！日志：\n"
                                << capture_logs();
    const mcp::JsonValue j = parse_response(raw);
    ASSERT_TRUE(j.IsObject()) << "响应非 JSON 对象: " << raw.substr(0, 300);
    const auto *err = j.Find("error");
    ASSERT_NE(err, nullptr);
    ASSERT_TRUE(err->IsString());
    EXPECT_NE(err->GetString().find("not found"), std::string::npos)
        << "超长工具名错误消息: " << err->GetString();
  }

  {
    const mcp::JsonValue j =
        parse_response(client.call_tool("batch_execute", R"({})"));
    ASSERT_TRUE(j.IsObject());
    const auto *err = j.Find("error");
    ASSERT_NE(err, nullptr);
    ASSERT_TRUE(err->IsString());
    EXPECT_NE(
        err->GetString().find("missing required parameter: operations (array)"),
        std::string::npos)
        << "batch_execute 缺 operations 错误消息: " << err->GetString();
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j = parse_response(
        client.call_tool("batch_execute", R"({"operations":[]})"));
    ASSERT_TRUE(j.IsObject());
    const auto *results = j.Find("results");
    ASSERT_TRUE(results != nullptr && results->IsArray())
        << "响应: " << j.Dump();
    EXPECT_TRUE(results->GetArray().empty());
    const auto *total = j.Find("total");
    ASSERT_TRUE(total != nullptr && total->IsInt());
    EXPECT_EQ(total->GetInt(), static_cast<int64_t>(0));
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j = parse_response(client.call_tool(
        "batch_execute", R"({"operations":[{"tool":"no_such_tool_xyz"}]})"));
    ASSERT_TRUE(editor_alive()) << "batch_execute 后编辑器进程死亡！日志：\n"
                                << capture_logs();
    ASSERT_TRUE(j.IsObject()) << "响应非 JSON 对象: " << j.Dump();
    const auto *results = j.Find("results");
    ASSERT_TRUE(results != nullptr && results->IsArray())
        << "响应: " << j.Dump();
    ASSERT_FALSE(results->GetArray().empty());
    const auto &item = results->GetArray()[0];
    ASSERT_TRUE(item.IsObject());
    const auto *status = item.Find("status");
    ASSERT_TRUE(status != nullptr && status->IsString());
    EXPECT_EQ(status->GetString(), "error");
    const auto *failed = j.Find("failed");
    ASSERT_TRUE(failed != nullptr && failed->IsInt());
    EXPECT_EQ(failed->GetInt(), static_cast<int64_t>(1));
  }

  {
    const mcp::JsonValue j =
        parse_response(client.call_tool("code_execute", R"({})"));
    ASSERT_TRUE(j.IsObject());
    const auto *err = j.Find("error");
    ASSERT_NE(err, nullptr);
    ASSERT_TRUE(err->IsString());
    EXPECT_NE(err->GetString().find("missing required parameter: source_code"),
              std::string::npos)
        << "code_execute 缺 source_code 错误消息: " << err->GetString();
    ASSERT_TRUE(editor_alive());
  }

  {
    const mcp::JsonValue j = parse_response(
        client.call_tool("call_tool", R"({"name":"system_status"})"));
    ASSERT_TRUE(editor_alive())
        << "system_status 调用后编辑器进程死亡！日志：\n"
        << capture_logs();
    ASSERT_TRUE(j.IsObject()) << "响应非 JSON 对象: " << j.Dump();
    const auto *result = j.Find("result");
    ASSERT_TRUE(result != nullptr && result->IsObject())
        << "响应: " << j.Dump();
    const auto *running = result->Find("running");
    EXPECT_TRUE(running != nullptr && running->IsBool() && running->GetBool())
        << "system_status 应报告 running=true";
  }
}
