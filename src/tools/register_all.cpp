#include "register_all.hpp"
#include "core/log_system.hpp"
#include <mcp/Content.hpp>
#include <mcp/JsonValue.hpp>

namespace godot_self_driving {

void register_all_tools(mcp::McpServer& server, CommandQueue& queue, ToolCatalog& catalog, Bm25Index& index, int port) {
    // ── 1. Populate catalog ──
    catalog.populate_default_tools();

    // ── 2. Register meta-tools directly ──
    // ping
    {
        mcp::ToolOptions opts;
        opts.Description("Health check ping");
        server.RegisterTool("ping", opts,
            [&queue](const mcp::RequestContext<mcp::CallToolRequestParams>&) -> mcp::CallToolResult {
                auto body = queue.submit([]() -> std::string {
                    mcp::JsonValue r(mcp::JsonValue::object_tag);
                    r["result"] = mcp::JsonValue("pong");
                    return r.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // system_status
    {
        mcp::ToolOptions opts;
        opts.Description("Get server status info");
        auto start = std::chrono::steady_clock::now();
        server.RegisterTool("system_status", opts,
            [&queue, start, port](const mcp::RequestContext<mcp::CallToolRequestParams>&) -> mcp::CallToolResult {
                auto body = queue.submit([start, port]() -> std::string {
                    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - start).count();
                    mcp::JsonValue status(mcp::JsonValue::object_tag);
                    status["version"] = mcp::JsonValue("0.1.0");
                    status["port"] = mcp::JsonValue(static_cast<int64_t>(port));
                    status["uptime_seconds"] = mcp::JsonValue(static_cast<int64_t>(uptime));
                    status["running"] = mcp::JsonValue(true);
                    mcp::JsonValue r(mcp::JsonValue::object_tag);
                    r["result"] = std::move(status);
                    return r.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // search_tools
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

        mcp::ToolOptions opts;
        opts.Description("Search available tools by query").InputSchema(std::move(s));
        server.RegisterTool("search_tools", opts,
            [&queue, &index](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                mcp::JsonValue args_copy = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args_copy), &index]() -> std::string {
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools called");
                    if (args.Find("query") == nullptr) {
                        mcp::JsonValue err(mcp::JsonValue::object_tag);
                        err["error"] = mcp::JsonValue("missing required parameter: query");
                        return err.Dump();
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
                    return j.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // list_categories
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);

        mcp::ToolOptions opts;
        opts.Description("List all tool categories").InputSchema(std::move(s));
        server.RegisterTool("list_categories", opts,
            [&queue, &catalog](const mcp::RequestContext<mcp::CallToolRequestParams>&) -> mcp::CallToolResult {
                auto body = queue.submit([&catalog]() -> std::string {
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
                    return j.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // get_tool_detail
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

        mcp::ToolOptions opts;
        opts.Description("Get complete schema for one tool").InputSchema(std::move(s));
        server.RegisterTool("get_tool_detail", opts,
            [&queue, &catalog](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                mcp::JsonValue args_copy = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args_copy), &catalog]() -> std::string {
                    if (args.Find("name") == nullptr) {
                        mcp::JsonValue err(mcp::JsonValue::object_tag);
                        err["error"] = mcp::JsonValue("missing required parameter: name");
                        return err.Dump();
                    }

                    std::string name = args["name"].GetString();
                    auto* info = catalog.get_tool(name);
                    if (!info) {
                        mcp::JsonValue err(mcp::JsonValue::object_tag);
                        err["error"] = mcp::JsonValue("tool not found: " + name);
                        return err.Dump();
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
                    return j.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // ── 3. Populate BM25 index ──
    for (auto* tool : catalog.get_all_tools()) {
        index.add_entry(tool->name, tool->description, tool->category, tool->tags);
    }

    auto count = catalog.size();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        std::to_string(count) + " tools registered via catalog");
}

} // namespace godot_self_driving
