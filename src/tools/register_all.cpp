#include "register_all.hpp"
#include "core/export_guard.hpp"
#include "core/log_system.hpp"
#include "tools/audio_ops.hpp"
#include "tools/capture_ops.hpp"
#include "tools/code_exec_ops.hpp"
#include "tools/config_ops.hpp"
#include "tools/debug_ops.hpp"
#include "tools/debugger_ops.hpp"
#include "tools/dispatch.hpp"
#include "tools/display_ops.hpp"
#include "tools/doc_ops.hpp"
#include "tools/editor_ops.hpp"
#include "tools/group_ops.hpp"
#include "tools/input_map_ops.hpp"
#include "tools/input_ops.hpp"
#include "tools/log_ops.hpp"
#include "tools/nav_ops.hpp"
#include "tools/os_ops.hpp"
#include "tools/physics_ops.hpp"
#include "tools/property_ops.hpp"
#include "tools/render_ops.hpp"
#include "tools/resource_ops.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/scene_ops.hpp"
#include "tools/scene_tree_ops.hpp"
#include "tools/schema_builder.hpp"
#include "tools/schema_fills.hpp"
#include "tools/script_ops.hpp"
#include "tools/spriteframes_ops.hpp"
#include "tools/text_ops.hpp"
#include "tools/tilemap_ops.hpp"
#include "tools/tileset_ops.hpp"
#include <mcp/Content.hpp>
#include <mcp/JsonValue.hpp>


namespace godot_autopilot {

namespace {
mcp::JsonValue make_schema() {
  mcp::JsonValue s(mcp::JsonValue::object_tag);
  s["type"] = mcp::JsonValue("object");
  mcp::JsonValue props(mcp::JsonValue::object_tag);
  s["properties"] = std::move(props);
  mcp::JsonValue req(mcp::JsonValue::array_tag);
  s["required"] = std::move(req);
  return s;
}

void add_required(mcp::JsonValue &schema, const std::string &name) {
  auto *req = schema.Find("required");
  if (req && req->IsArray()) {
    req->PushBack(mcp::JsonValue(name));
  }
}

std::vector<std::string> split_tags(const std::string &csv) {
  if (csv.empty())
    return {};
  std::vector<std::string> result;
  size_t start = 0, end;
  while ((end = csv.find(',', start)) != std::string::npos) {
    result.push_back(csv.substr(start, end - start));
    start = end + 1;
  }
  result.push_back(csv.substr(start));
  return result;
}

enum SchemaType { SCHEMA_NONE, SCHEMA_BASIC };

mcp::JsonValue build_schema_for_none_by_name(const std::string &name) {
  if (name == "call_scene_tree_group" || name == "get_scene_tree_nodes_in_group" ||
      name == "notify_scene_tree_group" || name == "is_scene_tree_paused" ||
      name == "set_scene_tree_pause") {
    return schema::build_schema({
        {"group", "string", "Scene group name.", false},
    });
  }

  if (name == "create_tilemap" || name == "set_tilemap_cell" ||
      name == "set_tilemap_cells") {
    return schema::build_schema({
        {"node_path", "string", "Path to the TileMap node in the scene.", true},
    });
  }

  mcp::JsonValue s(mcp::JsonValue::object_tag);
  s["type"] = mcp::JsonValue("object");
  s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
  return s;
}

mcp::JsonValue build_schema_for(SchemaType type, const std::string &name) {
  static const std::unordered_map<std::string, mcp::JsonValue> schemas = [] {
        std::unordered_map<std::string, mcp::JsonValue> m;


        fill_schema_scene(m);
        fill_schema_editor_config(m);
        fill_schema_physics(m);
        fill_schema_render_audio(m);
        fill_schema_debug_sys(m);
        fill_schema_content(m);


        return m;
    }();

    auto it = schemas.find(name);
    if (it != schemas.end()) return it->second;

    if (type == SCHEMA_NONE) {
        return build_schema_for_none_by_name(name);
    }

    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
    return s;
}

}

static mcp::JsonValue meta_ping_impl() {
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("pong");
    return r;
}

static mcp::JsonValue meta_search_tools_impl(const mcp::JsonValue& args, Bm25Index& index) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools called");
    if (args.Find("query") == nullptr) {
        mcp::JsonValue err(mcp::JsonValue::object_tag);
        err["error"] = mcp::JsonValue("missing required parameter: query");
        return err;
    }

    Bm25Index::SearchQuery query;
    query.text = args["query"].GetString();

    if (auto* cat = args.Find("category")) {
        if (!cat->IsNull()) {
            query.category = cat->GetString();
        }
    }

    if (auto* tags_val = args.Find("tags")) {
        if (!tags_val->IsNull() && tags_val->IsArray()) {
            std::vector<std::string> tags;
            for (auto& t : tags_val->GetArray()) {
                tags.push_back(t.GetString());
            }
            query.tags = std::move(tags);
        }
    }

    auto results = index.search(query);

    mcp::JsonValue j(mcp::JsonValue::object_tag);
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    for (auto& r : results) {
        mcp::JsonValue item(mcp::JsonValue::object_tag);
        item["name"] = mcp::JsonValue(r.name);
        item["score"] = mcp::JsonValue(r.score);
        arr.PushBack(std::move(item));
    }
    j["results"] = std::move(arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools completed");
    return j;
}

static mcp::JsonValue meta_list_categories_impl(ToolCatalog& catalog) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "list_categories called");
    auto cats = catalog.get_categories();
    mcp::JsonValue j(mcp::JsonValue::object_tag);
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    for (auto& c : cats) {
        mcp::JsonValue item(mcp::JsonValue::object_tag);
        item["id"] = mcp::JsonValue(c);
        item["name"] = mcp::JsonValue(c);
        int count = 0;
        for (auto* t : catalog.get_all_tools()) {
            if (t->category == c) {
                ++count;
            }
        }
        item["tool_count"] = mcp::JsonValue(static_cast<int64_t>(count));
        arr.PushBack(std::move(item));
    }
    j["categories"] = std::move(arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "list_categories completed");
    return j;
}

static mcp::JsonValue meta_get_tool_detail_impl(const mcp::JsonValue& args, ToolCatalog& catalog) {
    if (args.Find("name") == nullptr) {
        mcp::JsonValue err(mcp::JsonValue::object_tag);
        err["error"] = mcp::JsonValue("missing required parameter: name");
        return err;
    }

    std::string name = args["name"].GetString();
    auto* info = catalog.get_tool(name);
    if (!info) {
        mcp::JsonValue err(mcp::JsonValue::object_tag);
        err["error"] = mcp::JsonValue("tool not found: " + name);
        return err;
    }

    mcp::JsonValue j(mcp::JsonValue::object_tag);
    mcp::JsonValue t(mcp::JsonValue::object_tag);
    t["name"] = mcp::JsonValue(info->name);
    t["description"] = mcp::JsonValue(info->description);
    t["category"] = mcp::JsonValue(info->category);
    mcp::JsonValue tags(mcp::JsonValue::array_tag);
    for (auto& tg : info->tags) {
        tags.PushBack(mcp::JsonValue(tg));
    }
    t["tags"] = std::move(tags);
    t["input_schema"] = info->input_schema;
    j["tool"] = std::move(t);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "get_tool_detail completed");
    return j;
}

static mcp::JsonValue meta_call_tool_impl(mcp::JsonValue args) {
    if (args.Find("name") == nullptr) {
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "call_tool failed: missing name");
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: name");
        return e;
    }

    std::string name = args["name"].GetString();
    mcp::JsonValue tool_args = [&]{
        if (auto* a = args.Find("arguments")) return std::move(*a);
        return mcp::JsonValue(mcp::JsonValue::object_tag);
    }();

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, name + " called via call_tool");
    return dispatch::call_handler(name, tool_args);
}

static mcp::JsonValue meta_call_tool_wait(const std::string& name, mcp::JsonValue result) {
    auto* pending_p = result.Find("__gda_pending");
    if (pending_p && pending_p->IsInt()) {
        int64_t rid = pending_p->GetInt();
        int64_t timeout = 5000;
        if (auto* t = result.Find("timeout_ms")) {
            if (t->IsInt() && t->GetInt() > 0) timeout = t->GetInt();
        }
        mcp::JsonValue final = runtime_ops::wait_pending_response(rid, timeout);
        if (name == "capture_game_viewport" || (final.IsObject() && final.Contains("path"))) {
            final = runtime_ops::finalize_capture_response(final);
        }
        result = std::move(final);
    }
    return result;
}

static mcp::JsonValue meta_batch_execute_impl(const mcp::JsonValue& args) {
    return code_exec_ops::handle_batch_execute(args);
}

static mcp::JsonValue meta_code_execute_impl(const mcp::JsonValue& args) {
    return code_exec_ops::handle_code_execute(args);
}

void register_all_tools(mcp::McpServer& server, CommandQueue& queue, ToolCatalog& catalog, Bm25Index& index, int port) {

    dispatch::g_handlers["system_status"] = [port, start = std::chrono::steady_clock::now()](const mcp::JsonValue&) -> mcp::JsonValue {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        mcp::JsonValue status(mcp::JsonValue::object_tag);
        status["version"] = mcp::JsonValue("0.1.0");
        status["port"] = mcp::JsonValue(static_cast<int64_t>(port));
        status["uptime_seconds"] = mcp::JsonValue(static_cast<int64_t>(uptime));
        status["running"] = mcp::JsonValue(true);
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = std::move(status);
        return r;
    };

#define TOOL_ENTRY(id, name_str, desc, cat, tags_csv, handler_fn, schema_type) \
  dispatch::g_handlers[name_str] = handler_fn;
#include "tool_defs.def"
#undef TOOL_ENTRY


    catalog.populate_default_tools();



    {
        mcp::ToolOptions opts;
        opts.Description("Health check ping");
        server.RegisterTool("ping", opts,
            [](const mcp::RequestContext<mcp::CallToolRequestParams>&) -> mcp::CallToolResult {
                if (godot_autopilot::ExportGuard::is_exporting()) {
                    return dispatch::export_blocked_result();
                }
                mcp::JsonValue r = meta_ping_impl();
                auto body = r.Dump();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }


    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");

        mcp::JsonValue props(mcp::JsonValue::object_tag);

        mcp::JsonValue q_prop(mcp::JsonValue::object_tag);
        q_prop["type"] = mcp::JsonValue("string");
        q_prop["description"] = mcp::JsonValue("Search query");
        props["query"] = std::move(q_prop);

        mcp::JsonValue c_prop(mcp::JsonValue::object_tag);
        c_prop["type"] = mcp::JsonValue("string");
        c_prop["description"] = mcp::JsonValue("Filter by category");
        props["category"] = std::move(c_prop);

        mcp::JsonValue t_prop(mcp::JsonValue::object_tag);
        t_prop["type"] = mcp::JsonValue("array");
        mcp::JsonValue ti(mcp::JsonValue::object_tag);
        ti["type"] = mcp::JsonValue("string");
        t_prop["items"] = std::move(ti);
        t_prop["description"] = mcp::JsonValue("Filter by tags");
        props["tags"] = std::move(t_prop);

        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("query"));
        s["required"] = std::move(req);

        mcp::ToolOptions s_opts;
        s_opts.Description("Search available tools by query").InputSchema(std::move(s));
        server.RegisterTool("search_tools", s_opts,
            [&queue, &index](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                if (godot_autopilot::ExportGuard::is_exporting()) {
                    return dispatch::export_blocked_result();
                }
                mcp::JsonValue args_copy = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args_copy), &index]() -> std::string {
                    return meta_search_tools_impl(args, index).Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }


    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);

        mcp::ToolOptions l_opts;
        l_opts.Description("List all tool categories").InputSchema(std::move(s));
        server.RegisterTool("list_categories", l_opts,
            [&queue, &catalog](const mcp::RequestContext<mcp::CallToolRequestParams>&) -> mcp::CallToolResult {
                if (godot_autopilot::ExportGuard::is_exporting()) {
                    return dispatch::export_blocked_result();
                }
                auto body = queue.submit([&catalog]() -> std::string {
                    return meta_list_categories_impl(catalog).Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }


    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue n(mcp::JsonValue::object_tag);
        n["type"] = mcp::JsonValue("string");
        n["description"] = mcp::JsonValue("Tool name (e.g. ping, search_tools)");
        props["name"] = std::move(n);
        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(req);

        mcp::ToolOptions g_opts;
        g_opts.Description("Get complete schema for one tool").InputSchema(std::move(s));
        server.RegisterTool("get_tool_detail", g_opts,
            [&queue, &catalog](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                if (godot_autopilot::ExportGuard::is_exporting()) {
                    return dispatch::export_blocked_result();
                }
                mcp::JsonValue args_copy = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args_copy), &catalog]() -> std::string {
                    return meta_get_tool_detail_impl(args, catalog).Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }


    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue name_p(mcp::JsonValue::object_tag);
        name_p["type"] = mcp::JsonValue("string");
        name_p["description"] = mcp::JsonValue("Tool name to execute");
        props["name"] = std::move(name_p);
        mcp::JsonValue a(mcp::JsonValue::object_tag);
        a["type"] = mcp::JsonValue("object");
        a["description"] = mcp::JsonValue("Tool arguments as JSON object");
        props["arguments"] = std::move(a);
        s["properties"] = std::move(props);
        mcp::JsonValue creq(mcp::JsonValue::array_tag);
        creq.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(creq);

        mcp::ToolOptions c_opts;
        c_opts.Description("Execute any tool by name. Use this to call all non-meta tools (scene_*, property_*, signal_*, system_status).").InputSchema(std::move(s));
        server.RegisterTool("call_tool", c_opts,
            [](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                if (godot_autopilot::ExportGuard::is_exporting()) {
                    return dispatch::export_blocked_result();
                }
                mcp::JsonValue args = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                std::string name = args.Find("name") != nullptr ? args["name"].GetString() : "";
                mcp::JsonValue j = meta_call_tool_wait(name, meta_call_tool_impl(std::move(args)));
                std::string body = j.Dump();

                mcp::CallToolResult mcp_result;
                if (j.Find("error") != nullptr) mcp_result.is_error = true;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                if (!name.empty()) {
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, name + " completed");
                }
                return mcp_result;
            });
    }


    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);

        mcp::JsonValue item(mcp::JsonValue::object_tag);
        item["type"] = mcp::JsonValue("object");
        mcp::JsonValue iprops(mcp::JsonValue::object_tag);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        tp["description"] = mcp::JsonValue("Tool name to execute");
        iprops["tool"] = std::move(tp);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("object");
        ap["description"] = mcp::JsonValue("Tool arguments");
        iprops["args"] = std::move(ap);
        item["properties"] = std::move(iprops);
        mcp::JsonValue ireq(mcp::JsonValue::array_tag);
        ireq.PushBack(mcp::JsonValue("tool"));
        item["required"] = std::move(ireq);

        mcp::JsonValue ops_prop(mcp::JsonValue::object_tag);
        ops_prop["type"] = mcp::JsonValue("array");
        ops_prop["items"] = std::move(item);
        ops_prop["description"] = mcp::JsonValue("Ordered list of operations to execute");
        props["operations"] = std::move(ops_prop);

        mcp::JsonValue stop_prop(mcp::JsonValue::object_tag);
        stop_prop["type"] = mcp::JsonValue("boolean");
        stop_prop["description"] = mcp::JsonValue("Stop on first error");
        stop_prop["default"] = mcp::JsonValue(true);
        props["stop_on_error"] = std::move(stop_prop);

        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("operations"));
        s["required"] = std::move(req);

        mcp::ToolOptions b_opts;
        b_opts.Description("Execute multiple tools in batch. Each operation runs in sequence; if stop_on_error is true and any operation fails, remaining operations are skipped.").InputSchema(std::move(s));
        server.RegisterTool("batch_execute", b_opts,
            [&queue](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                if (godot_autopilot::ExportGuard::is_exporting()) {
                    return dispatch::export_blocked_result();
                }
                mcp::JsonValue args = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args)]() -> std::string {
                    return meta_batch_execute_impl(args).Dump();
                }).get();
                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }


    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);

        mcp::JsonValue sc(mcp::JsonValue::object_tag);
        sc["type"] = mcp::JsonValue("string");
        sc["description"] = mcp::JsonValue("GDScript source code");
        props["source_code"] = std::move(sc);

        mcp::JsonValue fn(mcp::JsonValue::object_tag);
        fn["type"] = mcp::JsonValue("string");
        fn["description"] = mcp::JsonValue("Function name to call (default: _run)");
        props["function_name"] = std::move(fn);

        mcp::JsonValue tm(mcp::JsonValue::object_tag);
        tm["type"] = mcp::JsonValue("integer");
        tm["description"] = mcp::JsonValue("Execution timeout in milliseconds (max 30000)");
        tm["default"] = mcp::JsonValue(static_cast<int64_t>(5000));
        tm["maximum"] = mcp::JsonValue(static_cast<int64_t>(30000));
        props["timeout_ms"] = std::move(tm);

        mcp::JsonValue ao(mcp::JsonValue::object_tag);
        ao["type"] = mcp::JsonValue("boolean");
        ao["description"] = mcp::JsonValue("Automatically set owner on nodes created during execution so they are saved with the scene (default true)");
        props["auto_owner"] = std::move(ao);

        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("source_code"));
        s["required"] = std::move(req);

        mcp::ToolOptions c_opts;
        c_opts.Description("Execute arbitrary GDScript code. The source code is wrapped in a script that extends Node, compiled, attached to a temporary node, and executed. Returns the function result serialized as JSON. By default the source is inlined inside the _run() function body: top-level func definitions are not supported — inline all code as expressions/statements, or define named functions and call one via function_name (multi-function mode). Execution environment exposes SceneRoot (the edited scene root node) for node access; see SceneRoot.get_node(\"Child\").").InputSchema(std::move(s));
        server.RegisterTool("code_execute", c_opts,
            [&queue](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                if (godot_autopilot::ExportGuard::is_exporting()) {
                    return dispatch::export_blocked_result();
                }
                mcp::JsonValue args = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args)]() -> std::string {
                    return meta_code_execute_impl(args).Dump();
                }).get();
                mcp::CallToolResult mcp_result;
                mcp::JsonValue j = mcp::JsonValue::Parse(body);
                if (j.Find("error") != nullptr) mcp_result.is_error = true;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

#define TOOL_ENTRY(id, name_str, desc, cat, tags_csv, handler_fn, schema_type) \
  catalog.add_tool({name_str, desc, cat, split_tags(tags_csv),                 \
                    build_schema_for(schema_type, name_str)});
#include "tool_defs.def"
#undef TOOL_ENTRY


    dispatch::g_meta_handlers["ping"] = [](const mcp::JsonValue&) -> mcp::JsonValue {
        return meta_ping_impl();
    };
    dispatch::g_meta_handlers["search_tools"] = [&index](const mcp::JsonValue& args) -> mcp::JsonValue {
        return meta_search_tools_impl(args, index);
    };
    dispatch::g_meta_handlers["list_categories"] = [&catalog](const mcp::JsonValue&) -> mcp::JsonValue {
        return meta_list_categories_impl(catalog);
    };
    dispatch::g_meta_handlers["get_tool_detail"] = [&catalog](const mcp::JsonValue& args) -> mcp::JsonValue {
        return meta_get_tool_detail_impl(args, catalog);
    };
    dispatch::g_meta_handlers["call_tool"] = [](const mcp::JsonValue& args) -> mcp::JsonValue {
        std::string name = args.Find("name") != nullptr ? args["name"].GetString() : "";
        mcp::JsonValue j = meta_call_tool_wait(name, meta_call_tool_impl(args));
        if (!name.empty()) {
            LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, name + " completed");
        }
        return j;
    };
    dispatch::g_meta_handlers["batch_execute"] = [](const mcp::JsonValue& args) -> mcp::JsonValue {
        return meta_batch_execute_impl(args);
    };
    dispatch::g_meta_handlers["code_execute"] = [](const mcp::JsonValue& args) -> mcp::JsonValue {
        return meta_code_execute_impl(args);
    };


    if (catalog.get_tool("batch_execute") == nullptr) {
        catalog.add_tool({"batch_execute",
            "Execute multiple tools in batch. Each operation runs in sequence; if stop_on_error is true and any operation fails, remaining operations are skipped.",
            "System", {"batch", "execute", "multi"},
            build_schema_for(SCHEMA_BASIC, "batch_execute")
        });
    }
    if (catalog.get_tool("call_tool") == nullptr) {
        catalog.add_tool({"call_tool",
            "Execute any tool by name. Use this to call all non-meta tools (scene_*, property_*, signal_*, system_status).",
            "System", {"call", "dispatch", "proxy"},
            build_schema_for(SCHEMA_BASIC, "call_tool")
        });
    }
    if (catalog.get_tool("code_execute") == nullptr) {
        catalog.add_tool({"code_execute",
            "Execute arbitrary GDScript code. The source code is wrapped in a script that extends Node, compiled, attached to a temporary node, and executed. Returns the function result serialized as JSON. By default the source is inlined inside the _run() function body: top-level func definitions are not supported — inline all code as expressions/statements, or define named functions and call one via function_name (multi-function mode). Execution environment exposes SceneRoot (the edited scene root node) for node access; see SceneRoot.get_node(\"Child\").",
            "System", {"code", "execute", "script", "gdscript"},
            build_schema_for(SCHEMA_BASIC, "code_execute")
        });
    }


    for (auto* tool : catalog.get_all_tools()) {
        index.add_entry(tool->name, tool->description, tool->category, tool->tags);
    }

    auto count = catalog.size();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        std::to_string(count) + " tools registered via catalog");


    int missing_from_catalog = 0;
    for (auto& [name, _] : dispatch::g_handlers) {
        if (catalog.get_tool(name) == nullptr) {
            LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                "handler missing catalog entry, using Auto: " + name);
            catalog.add_tool({name, name, "Auto", {"auto"}, make_schema()});
            ++missing_from_catalog;
        }
    }
    if (missing_from_catalog > 0) {
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
            std::to_string(missing_from_catalog) + " handlers auto-added to catalog");
    }


    for (auto* ctool : catalog.get_all_tools()) {
        if (dispatch::g_handlers.find(ctool->name) == dispatch::g_handlers.end() && dispatch::meta_tool_names.find(ctool->name) == dispatch::meta_tool_names.end()) {
            LogSystem::instance().log(LogLevel::Error, LogCategory::Tools,
                "tool '" + ctool->name + "' exists in catalog but has no handler in g_handlers");
        }
    }
}

}
