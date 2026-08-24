#include "register_all.hpp"
#include "core/error_watermark.hpp"
#include "core/export_guard.hpp"
#include "core/log_system.hpp"
#include <version.hpp>
#include "tools/code_exec_ops.hpp"
#include "tools/dispatch.hpp"
#include "tools/fn_tool.hpp"
#include "tools/group_tools.hpp"
#include "tools/capture_tools.hpp"
#include "tools/spriteframes_tools.hpp"
#include "tools/config_tools.hpp"
#include "tools/debug_tools.hpp"
#include "tools/audio_tools.hpp"
#include "tools/render_tools.hpp"
#include "tools/display_tools.hpp"
#include "tools/editor_tools.hpp"
#include "tools/input_tools.hpp"
#include "tools/os_tools.hpp"
#include "tools/physics_tools.hpp"
#include "tools/scene_tools.hpp"
#include "tools/scene_tree_tools.hpp"
#include "tools/resource_tools.hpp"
#include "tools/script_tools.hpp"
#include "tools/nav_tools.hpp"
#include "tools/text_tools.hpp"
#include "tools/tilemap_tools.hpp"
#include "tools/tileset_tools.hpp"
#include "tools/input_map_tools.hpp"
#include "tools/debugger_tools.hpp"
#include "tools/doc_tools.hpp"
#include "tools/game_tools.hpp"
#include "tools/property_tools.hpp"
#include "tools/system_tools.hpp"
#include "tools/animation_tools.hpp"
#include "tools/theme_tools.hpp"
#include "tools/test_tools.hpp"
#include "tools/analyze_tools.hpp"
#include "tools/meta_tools.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/schema_builder.hpp"
#include "tools/schema_fills.hpp"
#include "tools/tool_base.hpp"
#include "tools/tool_registry.hpp"
#include <mcp/Content.hpp>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <utility>


namespace godot_autopilot {

namespace {

mcp::JsonValue build_schema_for(const std::string &name) {
    static const std::unordered_map<std::string, mcp::JsonValue> schemas = [] {
        std::unordered_map<std::string, mcp::JsonValue> m;


        fill_schema_scene(m);
        fill_schema_editor_config(m);
        fill_schema_physics(m);
        fill_schema_render_audio(m);
        fill_schema_debug_sys(m);
        fill_schema_content(m);
        fill_schema_animation(m);
        fill_schema_theme(m);
        fill_schema_analysis(m);


        return m;
    }();

    auto it = schemas.find(name);
    if (it != schemas.end()) return it->second;

    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
    return s;
}

}

mcp::JsonValue tool_input_schema(const std::string& name, bool basic) { (void)basic; return build_schema_for(name); }

static std::unique_ptr<ToolRegistry> g_active_registry = std::make_unique<ToolRegistry>();

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
    const char* se = "";
    if (auto* tool = g_active_registry->find_any(name)) {
        se = ::godot_autopilot::side_effect_name(
            ::godot_autopilot::side_effect_of(*tool));
    }
    t["side_effect"] = mcp::JsonValue(se);
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

void register_all_tools(mcp::McpServer& server, CommandQueue& queue, ToolCatalog& catalog, Bm25Index& index, int port) {

    (void)queue;
    g_active_registry = std::make_unique<ToolRegistry>();
    index.clear();

    g_active_registry->add(make_fn_tool(
        ToolMeta{"system_status", "Get server status info", "System", {"status", "info"}, true},
        [port, start = std::chrono::steady_clock::now()](const mcp::JsonValue&) -> mcp::JsonValue {
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            mcp::JsonValue status(mcp::JsonValue::object_tag);
            status["version"] = mcp::JsonValue(GDA_VERSION);
            status["port"] = mcp::JsonValue(static_cast<int64_t>(port));
            status["uptime_seconds"] = mcp::JsonValue(static_cast<int64_t>(uptime));
            status["running"] = mcp::JsonValue(true);
            mcp::JsonValue r(mcp::JsonValue::object_tag);
            r["result"] = std::move(status);
            return r;
        },
        schema::build_schema({})));

auto add_domain_tools = [](auto& make_fn) {
        for (auto& t : make_fn()) {
            g_active_registry->add(std::move(t));
        }
    };

    add_domain_tools(group_tools::make_tools);
    add_domain_tools(capture_tools::make_tools);
    add_domain_tools(spriteframes_tools::make_tools);
    add_domain_tools(config_tools::make_tools);
    add_domain_tools(debug_tools::make_tools);
    add_domain_tools(audio_tools::make_tools);
    add_domain_tools(render_tools::make_tools);
    add_domain_tools(display_tools::make_tools);
    add_domain_tools(editor_tools::make_tools);
    add_domain_tools(input_tools::make_tools);
    add_domain_tools(os_tools::make_tools);
    add_domain_tools(physics_tools::make_tools);
    add_domain_tools(scene_tools::make_tools);
    add_domain_tools(scene_tree_tools::make_tools);
    add_domain_tools(resource_tools::make_tools);
    add_domain_tools(script_tools::make_tools);
    add_domain_tools(nav_tools::make_tools);
    add_domain_tools(text_tools::make_tools);
    add_domain_tools(tilemap_tools::make_tools);
    add_domain_tools(tileset_tools::make_tools);
    add_domain_tools(input_map_tools::make_tools);
    add_domain_tools(debugger_tools::make_tools);
    add_domain_tools(doc_tools::make_tools);
    add_domain_tools(game_tools::make_tools);
    add_domain_tools(property_tools::make_tools);
    add_domain_tools(system_tools::make_tools);
    add_domain_tools(animation_tools::make_tools);
    add_domain_tools(theme_tools::make_tools);
    add_domain_tools(test_tools::make_tools);
    add_domain_tools(analyze_tools::make_tools);

    g_active_registry->add(std::make_unique<::godot_autopilot::MetaTool>(
        ToolMeta{"ping", "Health check ping", "Meta", {"health", "ping"}, true},
        [](const mcp::JsonValue&) { return meta_ping_impl(); },
        schema::build_schema({})));

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
        g_active_registry->add(std::make_unique<::godot_autopilot::MetaTool>(
            ToolMeta{"search_tools", "Search available tools by query", "Meta", {"search", "discovery"}, true},
            [&index](const mcp::JsonValue& args) { return meta_search_tools_impl(args, index); },
            std::move(s)));
    }

    g_active_registry->add(std::make_unique<::godot_autopilot::MetaTool>(
        ToolMeta{"list_categories", "List all tool categories", "Meta", {"categories", "discovery"}, true},
        [&catalog](const mcp::JsonValue&) { return meta_list_categories_impl(catalog); },
        schema::build_schema({})));

    g_active_registry->add(std::make_unique<::godot_autopilot::MetaTool>(
        ToolMeta{"get_tool_detail", "Get complete schema for one tool", "Meta", {"detail", "schema"}, true},
        [&catalog](const mcp::JsonValue& args) { return meta_get_tool_detail_impl(args, catalog); },
        schema::build_schema({
            {"name", "string", "Tool name (e.g. ping, search_tools)", true},
        })));

    g_active_registry->add(std::make_unique<::godot_autopilot::MetaTool>(
        ToolMeta{"call_tool",
                 "Execute any tool by name. Use this to call all non-meta tools (scene_*, property_*, signal_*, system_status).",
                 "System", {"call", "dispatch", "proxy"}, true},
        [](const mcp::JsonValue& args) -> mcp::JsonValue {
            std::string name = args.Find("name") != nullptr && args["name"].IsString() ? args["name"].GetString() : std::string();
            mcp::JsonValue j = meta_call_tool_wait(name, meta_call_tool_impl(args));
            if (!name.empty()) {
                LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, name + " completed");
            }
            return j;
        },
        schema::build_schema({
            {"name", "string", "Tool name to execute", true},
            {"arguments", "object", "Tool arguments as JSON object", false},
        })));

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
        g_active_registry->add(std::make_unique<::godot_autopilot::MetaTool>(
            ToolMeta{"batch_execute",
                     "Execute multiple tools in batch. Each operation runs in sequence; if stop_on_error is true and any operation fails, remaining operations are skipped.",
                     "System", {"batch", "execute", "multi"}, true},
            [](const mcp::JsonValue& a) { return code_exec_ops::handle_batch_execute(a); },
            std::move(s)));
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
        g_active_registry->add(std::make_unique<::godot_autopilot::MetaTool>(
            ToolMeta{"code_execute",
                     "Execute arbitrary GDScript code. The source code is wrapped in a script that extends Node, compiled, attached to a temporary node, and executed. Returns the function result serialized as JSON. By default the source is inlined inside the _run() function body: top-level func definitions are not supported — inline all code as expressions/statements, or define named functions and call one via function_name (multi-function mode). Execution environment exposes SceneRoot (the edited scene root node) for node access; see SceneRoot.get_node(\"Child\").",
                     "System", {"code", "execute", "script", "gdscript"}, true},
            [](const mcp::JsonValue& a) { return code_exec_ops::handle_code_execute(a); },
            std::move(s)));
    }

    for (auto* t : g_active_registry->all_any()) {
        catalog.add_tool(make_tool_info(*t));
    }

    for (auto* meta_tool : g_active_registry->all_meta()) {
        const std::string meta_name = meta_tool->meta().name;
        mcp::ToolOptions opts;
        opts.Description(meta_tool->meta().description).InputSchema(meta_tool->input_schema());
        server.RegisterTool(meta_name, opts,
            [meta_name](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                if (godot_autopilot::ExportGuard::is_exporting()) {
                    return dispatch::export_blocked_result();
                }
                mcp::JsonValue args = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                mcp::JsonValue res = g_active_registry->find_meta(meta_name)->execute(args);
                int64_t new_errors = error_watermark::count_response_errors(res);
                if (new_errors > 0) {
                    error_watermark::record_error(new_errors);
                }
                mcp::CallToolResult mcp_result;
                if ((meta_name == "call_tool" || meta_name == "code_execute") && res.Find("error") != nullptr) {
                    mcp_result.is_error = true;
                }
                if (res.IsObject()) {
                    res["new_errors_since_last_call"] = mcp::JsonValue(error_watermark::consume_new_errors());
                }
                mcp_result.content.push_back(mcp::TextContent{"text", res.Dump()});
                return mcp_result;
            });
    }

    for (auto* t : g_active_registry->all_any()) {
        index.add_entry(t->meta().name, t->meta().description, t->meta().category, t->meta().tags);
    }

    for (auto* t : g_active_registry->all()) {
        std::string name = t->meta().name;
        dispatch::g_handlers[name] = [name](const mcp::JsonValue& a) -> mcp::JsonValue {
            return g_active_registry->find(name)->execute(a);
        };
    }
    for (auto* t : g_active_registry->all_meta()) {
        std::string name = t->meta().name;
        dispatch::g_meta_handlers[name] = [name](const mcp::JsonValue& a) -> mcp::JsonValue {
            return g_active_registry->find_meta(name)->execute(a);
        };
    }

    auto count = catalog.size();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        std::to_string(count) + " tools registered via registry");
}

ToolRegistry& get_active_registry() { return *g_active_registry; }

}
