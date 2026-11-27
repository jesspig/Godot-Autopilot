#include "tools/base/register_all.hpp"
#include "core/command_queue.hpp"
#include "core/error_watermark.hpp"
#include "core/export_guard.hpp"
#include "core/observability/log_system.hpp"
#include "core/observability/monitor.hpp"
#include "util/input_guards.hpp"
#include <version.hpp>
#include "tools/base/code_exec_ops.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/dynamic_spec_store.hpp"
#include "tools/domains/group/group_tools.hpp"
#include "tools/domains/capture/capture_tools.hpp"
#include "tools/domains/spriteframes/spriteframes_tools.hpp"
#include "tools/domains/config/config_tools.hpp"
#include "tools/domains/csharp_bridge/csharp_bridge_tools.hpp"
#include "tools/domains/debug/debug_tools.hpp"
#include "tools/domains/audio/audio_tools.hpp"
#include "tools/domains/render/render_tools.hpp"
#include "tools/domains/display/display_tools.hpp"
#include "tools/domains/editor/editor_tools.hpp"
#include "tools/domains/input/input_tools.hpp"
#include "tools/domains/image/image_tools.hpp"
#include "tools/domains/mesh/mesh_tools.hpp"
#include "tools/domains/os/os_tools.hpp"
#include "tools/domains/physics/physics_tools.hpp"
#include "tools/domains/scene/scene_tools.hpp"
#include "tools/domains/scene_tree/scene_tree_tools.hpp"
#include "tools/domains/resource/resource_tools.hpp"
#include "tools/domains/script/script_tools.hpp"
#include "tools/domains/nav/nav_tools.hpp"
#include "tools/domains/text/text_tools.hpp"
#include "tools/domains/tilemap/tilemap_tools.hpp"
#include "tools/domains/tileset/tileset_tools.hpp"
#include "tools/domains/input_map/input_map_tools.hpp"
#include "tools/domains/debugger/debugger_tools.hpp"
#include "tools/domains/doc/doc_tools.hpp"
#include "tools/domains/game/game_tools.hpp"
#include "tools/domains/property/property_tools.hpp"
#include "tools/domains/log/log_tools.hpp"
#include "tools/domains/animation/animation_tools.hpp"
#include "tools/domains/theme/theme_tools.hpp"
#include "tools/domains/test/test_tools.hpp"
#include "tools/domains/analyze/analyze_tools.hpp"
#include "tools/domains/monitor/monitor_tools.hpp"
#include "tools/domains/gridmap/gridmap_tools.hpp"
#include "tools/domains/tween/tween_tools.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/schema_builder.hpp"
#include "tools/base/tool_base.hpp"
#include "tools/base/tool_invoke.hpp"
#include "tools/base/tool_registry.hpp"
#include "tools/base/side_effect_policy.hpp"
#include "tools/base/tool_spec.hpp"
#include "util/json_value.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#ifdef GetObject
#undef GetObject
#endif


namespace godot_autopilot {

static std::shared_ptr<ToolRegistry> g_active_registry = std::make_shared<ToolRegistry>();
static std::mutex g_active_registry_mutex;
static ToolCatalog *g_catalog = nullptr;
static Bm25Index *g_index = nullptr;
static int g_port = 0;

static godot_autopilot::util::JsonValue meta_categories_impl(ToolCatalog& catalog);

static void audit_destructive_declarations(const ToolRegistry &registry) {
  std::vector<side_effect_policy::ToolDeclaration> declarations;
  for (const auto &tool : registry.all_any()) {
    side_effect_policy::ToolDeclaration entry;
    entry.name = tool->meta().name;
    entry.description = tool->meta().description;
    entry.destructive = side_effect_of(*tool) == SideEffect::Destructive;
    declarations.push_back(std::move(entry));
  }
  const std::vector<std::string> suspects =
      side_effect_policy::destructive_suspects(declarations);
  if (suspects.empty()) {
    return;
  }
  std::string names;
  for (const std::string &name : suspects) {
    if (!names.empty())
      names += ", ";
    names += name;
  }
  LogSystem::instance().log(
      LogLevel::Warning, LogCategory::Tools,
      "tools with destruction-shaped names or descriptions are not declared "
      "destructive: " + names);
  monitor::lifecycle(
      "destructive_declaration_audit",
      monitor::build_attrs({{"suspects", std::to_string(suspects.size())}}));
}

static godot_autopilot::util::JsonValue meta_search_tools_impl(const godot_autopilot::util::JsonValue& args,
                                             Bm25Index& index,
                                             ToolCatalog& catalog) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools called");

    std::string text;
    if (auto* q = args.Find("query"); q && q->IsString()) {
        text = q->GetString();
    }

    int64_t limit = 20;
    if (auto* limit_p = args.Find("limit"); limit_p && !limit_p->IsNull()) {
        if (!limit_p->IsInt() || limit_p->GetInt() < 0) {
            godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
            error["error"] = godot_autopilot::util::JsonValue("invalid parameter: limit must be a non-negative integer");
            return error;
        }
        limit = limit_p->GetInt();
    }

    bool categories = false;
    if (auto* categories_p = args.Find("categories"); categories_p && !categories_p->IsNull()) {
        if (!categories_p->IsBool()) {
            godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
            error["error"] = godot_autopilot::util::JsonValue("invalid parameter: categories must be a boolean");
            return error;
        }
        categories = categories_p->GetBool();
    }
    if (categories) {
        return meta_categories_impl(catalog);
    }

    Bm25Index::SearchQuery query;
    query.text = text;

    bool has_category = false;
    if (auto* cat = args.Find("category")) {
        if (!cat->IsNull() && cat->IsString() && !cat->GetString().empty()) {
            query.category = cat->GetString();
            has_category = true;
        }
    }

    bool has_tags = false;
    if (auto* tags_val = args.Find("tags")) {
        if (!tags_val->IsNull() && tags_val->IsArray()) {
            std::vector<std::string> tags;
            for (auto& t : tags_val->GetArray()) {
                tags.push_back(t.GetString());
            }
            if (!tags.empty()) {
                query.tags = std::move(tags);
                has_tags = true;
            }
        }
    }

    if (text.empty() && !has_category && !has_tags) {
        auto all = catalog.get_all_tools();
        std::size_t count = all.size();
        if (limit > 0 && static_cast<int64_t>(count) > limit) {
            count = static_cast<std::size_t>(limit);
        }
        godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
        godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
        for (std::size_t i = 0; i < count; ++i) {
            godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
            item["name"] = godot_autopilot::util::JsonValue(all[i].name);
            item["score"] = godot_autopilot::util::JsonValue(0.0);
            arr.PushBack(std::move(item));
        }
        j["results"] = std::move(arr);
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools completed");
        return j;
    }

    query.max_results = limit == 0
        ? std::numeric_limits<int>::max()
        : static_cast<int>(std::min<int64_t>(limit, std::numeric_limits<int>::max()));
    auto results = index.search(query);

    godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
    godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
    for (auto& r : results) {
        godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
        item["name"] = godot_autopilot::util::JsonValue(r.name);
        item["score"] = godot_autopilot::util::JsonValue(r.score);
        arr.PushBack(std::move(item));
    }
    j["results"] = std::move(arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools completed");
    return j;
}

static godot_autopilot::util::JsonValue meta_categories_impl(ToolCatalog& catalog) {
    auto cats = catalog.get_categories();
    godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
    godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
    for (auto& c : cats) {
        godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
        item["id"] = godot_autopilot::util::JsonValue(c);
        item["name"] = godot_autopilot::util::JsonValue(c);
        int count = 0;
        for (const auto& t : catalog.get_all_tools()) {
            if (t.category == c) {
                ++count;
            }
        }
        item["tool_count"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(count));
        arr.PushBack(std::move(item));
    }
    j["categories"] = std::move(arr);
    return j;
}

godot_autopilot::util::JsonValue tool_detail_for(const std::string &name, ToolCatalog &catalog,
                               const std::shared_ptr<ToolRegistry> &registry) {
    godot_autopilot::util::JsonValue err(godot_autopilot::util::JsonValue::object_tag);
    if (name.empty()) {
        err["error"] = godot_autopilot::util::JsonValue("missing required parameter: name");
        return err;
    }

    std::optional<ToolAlias> alias;
    if (registry) {
        alias = registry->resolve_alias(name);
    }
    const std::string canonical = alias ? alias->canonical : name;
    auto info = catalog.get_tool(canonical);
    if (!info) {
        err["error"] = godot_autopilot::util::JsonValue("tool not found: " + canonical);
        return err;
    }

    godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
    godot_autopilot::util::JsonValue t(godot_autopilot::util::JsonValue::object_tag);
    t["name"] = godot_autopilot::util::JsonValue(info->name);
    t["description"] = godot_autopilot::util::JsonValue(info->description);
    t["category"] = godot_autopilot::util::JsonValue(info->category);
    godot_autopilot::util::JsonValue tags(godot_autopilot::util::JsonValue::array_tag);
    for (auto& tg : info->tags) {
        tags.PushBack(godot_autopilot::util::JsonValue(tg));
    }
    t["tags"] = std::move(tags);
    t["input_schema"] = info->input_schema;
    godot_autopilot::util::JsonValue examples(godot_autopilot::util::JsonValue::array_tag);
    for (const auto &example : info->examples) {
        examples.PushBack(godot_autopilot::util::JsonValue(example));
    }
    t["examples"] = std::move(examples);
    const char* se = "none";
    uint32_t flags = 0;
    if (registry) {
        if (auto tool = registry->find_any(canonical)) {
            se = ::godot_autopilot::side_effect_name(
                ::godot_autopilot::side_effect_of(*tool));
            flags = tool->tool_flags();
        }
    }
    t["side_effect"] = godot_autopilot::util::JsonValue(se);
    t["dynamic"] = godot_autopilot::util::JsonValue((flags & tool_flags::kDynamic) != 0);
    t["mutating"] = godot_autopilot::util::JsonValue((flags & tool_flags::kMutating) != 0);
    if (alias) {
        j["deprecated_alias"] = godot_autopilot::util::JsonValue(name);
        j["canonical_name"] = godot_autopilot::util::JsonValue(canonical);
        j["alias_removal_version"] = godot_autopilot::util::JsonValue(alias->removal_version);
        if (!alias->deprecated_since.empty()) {
            j["deprecated_since"] = godot_autopilot::util::JsonValue(alias->deprecated_since);
        }
        t["canonical_name"] = godot_autopilot::util::JsonValue(canonical);
        LogSystem::instance().log(
            LogLevel::Warning, LogCategory::Tools,
            "tool alias '" + name + "' resolved to '" + canonical + "'");
    }
    j["tool"] = std::move(t);
    return j;
}

static godot_autopilot::util::JsonValue meta_get_tool_detail_impl(
    const godot_autopilot::util::JsonValue& args, ToolCatalog& catalog,
    const std::shared_ptr<ToolRegistry>& registry) {
    const auto *name_p = args.Find("name");
    const std::string name =
        name_p != nullptr && name_p->IsString() ? name_p->GetString() : std::string();
    godot_autopilot::util::JsonValue result = tool_detail_for(name, catalog, registry);
    if (result.Find("error") == nullptr) {
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                  "get_tool_detail completed");
    }
    return result;
}

static godot_autopilot::util::JsonValue meta_call_tool_impl(godot_autopilot::util::JsonValue args) {
    auto *name_p = args.Find("name");
    if (!name_p || !name_p->IsString() || name_p->GetString().empty()) {
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "call_tool failed: missing name");
        godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
        e["error"] = godot_autopilot::util::JsonValue("missing required parameter: name " + util::received_keys_text(args, false));
        return e;
    }

    std::string name = name_p->GetString();
    godot_autopilot::util::JsonValue tool_args = [&]{
        if (auto* a = args.Find("arguments")) return std::move(*a);
        return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
    }();

    if (auto *a = args.Find("arguments"); a && !a->IsObject()) {
        godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
        e["error"] = godot_autopilot::util::JsonValue("invalid parameter for call_tool '" + name + "': arguments must be an object " + util::received_keys_text(args, false));
        return e;
    }
    for (const auto &entry : args.GetObject()) {
        if (entry.first != "name" && entry.first != "arguments" &&
            entry.first != "confirm") {
            godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
            e["error"] = godot_autopilot::util::JsonValue("unknown parameter for call_tool '" + name + "': " + entry.first + " (expected only 'name', 'arguments' and 'confirm') " + util::received_keys_text(args, false));
            return e;
        }
    }
    if (const auto *confirm = args.Find("confirm");
        confirm != nullptr && confirm->IsBool() && confirm->GetBool() &&
        tool_args.IsObject() && tool_args.Find("confirm") == nullptr) {
        tool_args["confirm"] = godot_autopilot::util::JsonValue(true);
    }

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, name + " called via call_tool");
    return dispatch::call_handler(name, tool_args,
                                  dispatch::capture_depth_context());
}

static godot_autopilot::util::JsonValue meta_call_tool_wait(const std::string& name, godot_autopilot::util::JsonValue result) {
    return dispatch::resolve_deferred_wait(name, std::move(result));
}

void refresh_derived(const std::shared_ptr<ToolRegistry>& registry,
                     ToolCatalog& catalog, Bm25Index& index) {
    const auto derived_start = std::chrono::steady_clock::now();
    std::vector<ToolInfo> catalog_tools;
    std::vector<Bm25Index::Entry> index_entries;
    for (const auto& t : registry->all_any()) {
        catalog_tools.push_back(make_tool_info(*t));
        index_entries.push_back({t->meta().name, t->meta().description,
                                 t->meta().category, t->meta().tags});
    }
    catalog.replace_tools(std::move(catalog_tools));
    catalog.replace_aliases(registry->alias_map());
    index.replace_entries(index_entries);

    dispatch::HandlerMap handlers;
    dispatch::HandlerMap meta_handlers;
    for (const auto& t : registry->all()) {
        std::string name = t->meta().name;
        handlers[name] = [name, registry](const godot_autopilot::util::JsonValue& a) -> godot_autopilot::util::JsonValue {
            return registry->find(name)->execute(a);
        };
    }
    for (const auto& t : registry->all_meta()) {
        std::string name = t->meta().name;
        meta_handlers[name] = [name, registry](const godot_autopilot::util::JsonValue& a) -> godot_autopilot::util::JsonValue {
            return registry->find_meta(name)->execute(a);
        };
    }
    dispatch::replace_handlers(std::move(handlers), std::move(meta_handlers));
    {
        std::lock_guard<std::mutex> lock(g_active_registry_mutex);
        g_active_registry = registry;
    }
    const int64_t derived_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - derived_start)
            .count();
    monitor::lifecycle(
        "registry_refresh_derived",
        monitor::build_attrs(
            {{"tools", std::to_string(registry->all_any().size())},
             {"handlers", std::to_string(registry->all().size())},
             {"meta_handlers", std::to_string(registry->all_meta().size())},
             {"duration_ms", std::to_string(derived_duration_ms)}}));
}

static std::shared_ptr<ToolRegistry> build_registry(ToolCatalog& catalog, Bm25Index& index, int port) {

    auto registry = std::make_shared<ToolRegistry>();
    const auto registry_start = std::chrono::steady_clock::now();

    {
        ToolSpec spec{"system_status", "Get server status info", "System", {"status", "info"}};
        spec.side_effect = SideEffect::None;
        spec.flags = tool_flags::kHealthProbe;
        spec.handler = [port, registry, start = std::chrono::steady_clock::now()](const godot_autopilot::util::JsonValue&) -> godot_autopilot::util::JsonValue {
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            godot_autopilot::util::JsonValue status(godot_autopilot::util::JsonValue::object_tag);
            status["version"] = godot_autopilot::util::JsonValue(GDA_VERSION);
            status["port"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(port));
            status["uptime_seconds"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(uptime));
            status["running"] = godot_autopilot::util::JsonValue(true);
            status["tool_count"] =
                godot_autopilot::util::JsonValue(static_cast<int64_t>(registry->all_any().size()));
            godot_autopilot::util::JsonValue main_thread(godot_autopilot::util::JsonValue::object_tag);
            main_thread["last_drain_age_ms"] =
                godot_autopilot::util::JsonValue(static_cast<int64_t>(-1));
            main_thread["queue_depth"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(0));
            main_thread["queue_cancelled"] =
                godot_autopilot::util::JsonValue(static_cast<int64_t>(0));
            main_thread["queue_rejected_full"] =
                godot_autopilot::util::JsonValue(static_cast<int64_t>(0));
            main_thread["oldest_pending_ms"] =
                godot_autopilot::util::JsonValue(static_cast<int64_t>(-1));
            if (runtime_ops::has_editor_queue()) {
                CommandQueue &queue = get_editor_queue();
                const CommandQueue::Stats queue_stats = queue.stats();
                status["queue_depth"] =
                    godot_autopilot::util::JsonValue(static_cast<int64_t>(queue_stats.pending));
                main_thread["last_drain_age_ms"] =
                    godot_autopilot::util::JsonValue(queue.last_drain_age_ms());
                main_thread["queue_depth"] =
                    godot_autopilot::util::JsonValue(static_cast<int64_t>(queue_stats.pending));
                main_thread["queue_cancelled"] =
                    godot_autopilot::util::JsonValue(static_cast<int64_t>(queue_stats.cancelled));
                main_thread["queue_rejected_full"] = godot_autopilot::util::JsonValue(
                    static_cast<int64_t>(queue_stats.rejected_full));
                main_thread["oldest_pending_ms"] =
                    godot_autopilot::util::JsonValue(queue.oldest_pending_age_ms());
            } else {
                status["queue_depth"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(0));
            }
            status["main_thread"] = std::move(main_thread);
            godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
            r["result"] = std::move(status);
            return r;
        };
        spec.raw_schema = schema::build_schema({});
        registry->add(make_spec_tool(with_aliases(
            std::move(spec),
            {ToolAlias{"ping", "system_status", {},
                       [](const godot_autopilot::util::JsonValue &) {
                         godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
                         out["result"] = godot_autopilot::util::JsonValue("pong");
                         return out;
                       },
                       "0.4.0", "0.6.0"}})));
    }

    auto add_domain_tools = [&registry](auto& make_fn) {
        for (auto& t : make_fn()) {
            registry->add(std::move(t));
        }
    };

    add_domain_tools(group_tools::make_tools);
    add_domain_tools(capture_tools::make_tools);
    add_domain_tools(spriteframes_tools::make_tools);
    add_domain_tools(config_tools::make_tools);
    add_domain_tools(csharp_bridge_tools::make_tools);
    add_domain_tools(debug_tools::make_tools);
    add_domain_tools(audio_tools::make_tools);
    add_domain_tools(render_tools::make_tools);
    add_domain_tools(display_tools::make_tools);
    add_domain_tools(editor_tools::make_tools);
    add_domain_tools(input_tools::make_tools);
    add_domain_tools(image_tools::make_tools);
    add_domain_tools(mesh_tools::make_tools);
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
    add_domain_tools(log_tools::make_tools);
    add_domain_tools(animation_tools::make_tools);
    add_domain_tools(theme_tools::make_tools);
    add_domain_tools(test_tools::make_tools);
    add_domain_tools(analyze_tools::make_tools);
    add_domain_tools(monitor_tools::make_tools);
    add_domain_tools(gridmap_tools::make_tools);
    add_domain_tools(tween_tools::make_tools);

    {
        ToolSpec spec{"search_tools", "Search available tools by query, category or tags; categories=true lists categories with tool counts", "Meta", {"search", "discovery"}};
        spec.side_effect = SideEffect::None;
        spec.flags = tool_flags::kMeta;
        spec.handler = [&index, &catalog](const godot_autopilot::util::JsonValue& args) { return meta_search_tools_impl(args, index, catalog); };
        godot_autopilot::util::JsonValue s(godot_autopilot::util::JsonValue::object_tag);
        s["type"] = godot_autopilot::util::JsonValue("object");
        godot_autopilot::util::JsonValue props(godot_autopilot::util::JsonValue::object_tag);
        godot_autopilot::util::JsonValue q_prop(godot_autopilot::util::JsonValue::object_tag);
        q_prop["type"] = godot_autopilot::util::JsonValue("string");
        q_prop["description"] = godot_autopilot::util::JsonValue("Search query; omit or pass an empty string to enumerate all tools");
        props["query"] = std::move(q_prop);
        godot_autopilot::util::JsonValue l_prop(godot_autopilot::util::JsonValue::object_tag);
        l_prop["type"] = godot_autopilot::util::JsonValue("integer");
        l_prop["description"] = godot_autopilot::util::JsonValue("Maximum number of results; 0 returns all tools");
        l_prop["default"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(20));
        l_prop["minimum"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(0));
        props["limit"] = std::move(l_prop);
        godot_autopilot::util::JsonValue c_prop(godot_autopilot::util::JsonValue::object_tag);
        c_prop["type"] = godot_autopilot::util::JsonValue("string");
        c_prop["description"] = godot_autopilot::util::JsonValue("Filter by category");
        props["category"] = std::move(c_prop);
        godot_autopilot::util::JsonValue t_prop(godot_autopilot::util::JsonValue::object_tag);
        t_prop["type"] = godot_autopilot::util::JsonValue("array");
        godot_autopilot::util::JsonValue ti(godot_autopilot::util::JsonValue::object_tag);
        ti["type"] = godot_autopilot::util::JsonValue("string");
        t_prop["items"] = std::move(ti);
        t_prop["description"] = godot_autopilot::util::JsonValue("Filter by tags");
        props["tags"] = std::move(t_prop);
        godot_autopilot::util::JsonValue cat_prop(godot_autopilot::util::JsonValue::object_tag);
        cat_prop["type"] = godot_autopilot::util::JsonValue("boolean");
        cat_prop["description"] = godot_autopilot::util::JsonValue(
            "When true return the category list with per-category tool counts instead of tool results");
        props["categories"] = std::move(cat_prop);
        s["properties"] = std::move(props);
        s["required"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::array_tag);
        spec.raw_schema = std::move(s);
        registry->add(make_spec_tool(with_aliases(
            std::move(spec),
            {ToolAlias{"list_categories", "search_tools",
                       [](const godot_autopilot::util::JsonValue &) {
                         godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
                         out["categories"] = godot_autopilot::util::JsonValue(true);
                         out["limit"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(0));
                         return out;
                       },
                       {}, "0.4.0", "0.6.0"}})));
    }

    {
        ToolSpec spec{"get_tool_detail", "Get complete schema for one tool", "Meta", {"detail", "schema"}};
        spec.side_effect = SideEffect::None;
        spec.flags = tool_flags::kMeta;
        spec.handler = [&catalog](const godot_autopilot::util::JsonValue& args) {
            return meta_get_tool_detail_impl(args, catalog, get_active_registry());
        };
        spec.raw_schema = schema::build_schema({
            {"name", "string", "Tool name (e.g. get_scene_tree, search_tools)", true},
        });
        registry->add(make_spec_tool(std::move(spec)));
    }

    {
        ToolSpec spec{"call_tool",
                      "Execute any tool by name. Use this to call all 251 non-meta tools (246 domain tools plus system_status).",
                      "System", {"call", "dispatch", "proxy"}};
        spec.side_effect = SideEffect::None;
        spec.flags = tool_flags::kMeta;
        spec.handler = [](const godot_autopilot::util::JsonValue& args) -> godot_autopilot::util::JsonValue {
            std::string name = args.Find("name") != nullptr && args["name"].IsString() ? args["name"].GetString() : std::string();
            godot_autopilot::util::JsonValue j = meta_call_tool_wait(name, meta_call_tool_impl(args));
            if (!name.empty()) {
                LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, name + " completed");
            }
            return j;
        };
        spec.raw_schema = schema::build_schema({
            {"name", "string", "Tool name to execute", true},
            {"arguments", "object", "Tool arguments as JSON object", false},
            {"confirm", "boolean", "Confirm high-risk execution after review", false},
        });
        registry->add(make_spec_tool(std::move(spec)));
    }

    {
        ToolSpec spec{"batch_execute",
                      "Execute multiple tools in batch. Each operation runs in sequence; if stop_on_error is true and any operation fails, remaining operations are skipped. Async game tools are refused unless await_async=true, in which case batch_execute waits for the game response and reports it under result. With stop_on_error=true and rollback_on_error=true the changes of already-executed operations are rolled back through the global undo history when the batch stops.",
                      "System", {"batch", "execute", "multi"}};
        spec.side_effect = SideEffect::CodeExecute;
        spec.flags = tool_flags::kMeta | tool_flags::kLongBlocking;
        spec.handler = [](const godot_autopilot::util::JsonValue& a) { return code_exec_ops::handle_batch_execute(a); };
        godot_autopilot::util::JsonValue s(godot_autopilot::util::JsonValue::object_tag);
        s["type"] = godot_autopilot::util::JsonValue("object");
        godot_autopilot::util::JsonValue props(godot_autopilot::util::JsonValue::object_tag);
        godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
        item["type"] = godot_autopilot::util::JsonValue("object");
        godot_autopilot::util::JsonValue iprops(godot_autopilot::util::JsonValue::object_tag);
        godot_autopilot::util::JsonValue tp(godot_autopilot::util::JsonValue::object_tag);
        tp["type"] = godot_autopilot::util::JsonValue("string");
        tp["description"] = godot_autopilot::util::JsonValue("Tool name to execute");
        iprops["tool"] = std::move(tp);
        godot_autopilot::util::JsonValue ap(godot_autopilot::util::JsonValue::object_tag);
        ap["type"] = godot_autopilot::util::JsonValue("object");
        ap["description"] = godot_autopilot::util::JsonValue("Tool arguments");
        iprops["args"] = std::move(ap);
        item["properties"] = std::move(iprops);
        godot_autopilot::util::JsonValue ireq(godot_autopilot::util::JsonValue::array_tag);
        ireq.PushBack(godot_autopilot::util::JsonValue("tool"));
        item["required"] = std::move(ireq);
        godot_autopilot::util::JsonValue ops_prop(godot_autopilot::util::JsonValue::object_tag);
        ops_prop["type"] = godot_autopilot::util::JsonValue("array");
        ops_prop["items"] = std::move(item);
        ops_prop["description"] = godot_autopilot::util::JsonValue("Ordered list of operations to execute; each result carries status 'ok'/'error'. An async game op (execute_game_script, queue_game_input, wait_game_input, sequence_game_inputs, get_game_ui_elements, click_game_ui_element, capture_game_viewport, capture_editor_viewport with target='game') is refused with status 'error' unless await_async=true; its pending record is always released. Response counts: 'total' = executed operations (succeeded+failed), 'skipped' = operations not run after stop_on_error truncation");
        props["operations"] = std::move(ops_prop);
        godot_autopilot::util::JsonValue stop_prop(godot_autopilot::util::JsonValue::object_tag);
        stop_prop["type"] = godot_autopilot::util::JsonValue("boolean");
        stop_prop["description"] = godot_autopilot::util::JsonValue("Stop on first error");
        stop_prop["default"] = godot_autopilot::util::JsonValue(true);
        props["stop_on_error"] = std::move(stop_prop);
        godot_autopilot::util::JsonValue rollback_prop(godot_autopilot::util::JsonValue::object_tag);
        rollback_prop["type"] = godot_autopilot::util::JsonValue("boolean");
        rollback_prop["description"] = godot_autopilot::util::JsonValue(
            "Only effective when stop_on_error is true; when the batch stops "
            "on an error the changes of already-executed operations are rolled "
            "back through the global undo history; the response carries "
            "rolled_back and rollback_partial; changes that never entered the "
            "global undo history are not rolled back");
        rollback_prop["default"] = godot_autopilot::util::JsonValue(false);
        props["rollback_on_error"] = std::move(rollback_prop);
        godot_autopilot::util::JsonValue await_prop(godot_autopilot::util::JsonValue::object_tag);
        await_prop["type"] = godot_autopilot::util::JsonValue("boolean");
        await_prop["description"] = godot_autopilot::util::JsonValue("Wait for async game ops instead of refusing them (default: false). With false an async game op is reported as an error telling you to use call_tool or await_async=true; with true batch_execute waits for the game response (bounded by the op's timeout_ms) and reports ok/error per response under result");
        await_prop["default"] = godot_autopilot::util::JsonValue(false);
        props["await_async"] = std::move(await_prop);
        godot_autopilot::util::JsonValue confirm_prop(godot_autopilot::util::JsonValue::object_tag);
        confirm_prop["type"] = godot_autopilot::util::JsonValue("boolean");
        confirm_prop["description"] = godot_autopilot::util::JsonValue(
            "Confirm high-risk batch execution after reviewing the operations");
        props["confirm"] = std::move(confirm_prop);
        s["properties"] = std::move(props);
        godot_autopilot::util::JsonValue req(godot_autopilot::util::JsonValue::array_tag);
        req.PushBack(godot_autopilot::util::JsonValue("operations"));
        s["required"] = std::move(req);
        spec.raw_schema = std::move(s);
        registry->add(make_spec_tool(std::move(spec)));
    }

    {
        ToolSpec spec{"code_execute",
                      "Execute arbitrary GDScript code. The source code is wrapped in a script that extends Node, compiled, attached to a temporary node, and executed. Returns the function result serialized as JSON. Without a line-start `func ` definition the source is inlined inside the entry-function body: write straight-line code ending in return. When any non-comment line starts with `func ` (leading spaces/tabs stripped; `#` and `//` comment lines ignored) the whole source is preserved verbatim and function_name (default _run) selects the entry point, with a `func <name>(): pass` stub appended when the named entry is missing. Detection needs `func ` with a trailing space: `func<TAB>name():` or `func(` do not switch modes and are rejected in single-function mode; mixing tab and space indentation is rejected. Execution environment exposes SceneRoot (the edited scene root node) for node access; see SceneRoot.get_node(\"Child\").",
                      "System", {"code", "execute", "script", "gdscript"}};
        spec.side_effect = SideEffect::CodeExecute;
        spec.flags = tool_flags::kMeta | tool_flags::kLongBlocking;
        spec.handler = [](const godot_autopilot::util::JsonValue& a) { return code_exec_ops::handle_code_execute(a); };
        godot_autopilot::util::JsonValue s(godot_autopilot::util::JsonValue::object_tag);
        s["type"] = godot_autopilot::util::JsonValue("object");
        godot_autopilot::util::JsonValue props(godot_autopilot::util::JsonValue::object_tag);
        godot_autopilot::util::JsonValue sc(godot_autopilot::util::JsonValue::object_tag);
        sc["type"] = godot_autopilot::util::JsonValue("string");
        sc["description"] = godot_autopilot::util::JsonValue("GDScript source code");
        props["source_code"] = std::move(sc);
        godot_autopilot::util::JsonValue fn(godot_autopilot::util::JsonValue::object_tag);
        fn["type"] = godot_autopilot::util::JsonValue("string");
        fn["description"] = godot_autopilot::util::JsonValue("Function name to call (default: _run)");
        props["function_name"] = std::move(fn);
        godot_autopilot::util::JsonValue tm(godot_autopilot::util::JsonValue::object_tag);
        const int64_t code_exec_max_timeout_ms =
            static_cast<int64_t>(code_exec_ops::max_timeout_ms());
        const int64_t code_exec_default_timeout_ms =
            code_exec_max_timeout_ms < 5000 ? code_exec_max_timeout_ms : 5000;
        tm["type"] = godot_autopilot::util::JsonValue("integer");
        tm["description"] = godot_autopilot::util::JsonValue(
            "Execution timeout in milliseconds (max " +
            std::to_string(code_exec_max_timeout_ms) + ")");
        tm["default"] = godot_autopilot::util::JsonValue(code_exec_default_timeout_ms);
        tm["maximum"] = godot_autopilot::util::JsonValue(code_exec_max_timeout_ms);
        props["timeout_ms"] = std::move(tm);
        godot_autopilot::util::JsonValue ao(godot_autopilot::util::JsonValue::object_tag);
        ao["type"] = godot_autopilot::util::JsonValue("boolean");
        ao["description"] = godot_autopilot::util::JsonValue("Automatically set owner on nodes created during execution so they are saved with the scene (default true)");
        props["auto_owner"] = std::move(ao);
        godot_autopilot::util::JsonValue confirm_prop(godot_autopilot::util::JsonValue::object_tag);
        confirm_prop["type"] = godot_autopilot::util::JsonValue("boolean");
        confirm_prop["description"] = godot_autopilot::util::JsonValue(
            "Confirm arbitrary code execution after reviewing the source");
        props["confirm"] = std::move(confirm_prop);
        s["properties"] = std::move(props);
        godot_autopilot::util::JsonValue req(godot_autopilot::util::JsonValue::array_tag);
        req.PushBack(godot_autopilot::util::JsonValue("source_code"));
        s["required"] = std::move(req);
        spec.raw_schema = std::move(s);
        registry->add(make_spec_tool(std::move(spec)));
    }

    {
        std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
        if (!dynamic_specs::enabled().load()) {
            LogSystem::instance().log_detailed(
                LogLevel::Info, LogCategory::Tools,
                "dynamic tools skipped while disabled",
                "reason=disabled");
        } else {
            for (const auto& spec : dynamic_specs::store()) {
                if (registry->find_any(spec.name) != nullptr) {
                    LogSystem::instance().log_detailed(
                        LogLevel::Warning, LogCategory::Tools,
                        "user tool '" + spec.name +
                            "' skipped: name collides with a built-in tool",
                        "tool=" + spec.name +
                            " params=" + std::to_string(spec.params.size()) +
                            " reason=collision");
                    continue;
                }
                registry->add(make_spec_tool(spec));
            }
        }
    }

    audit_destructive_declarations(*registry);

    const int64_t registry_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - registry_start)
            .count();
    monitor::lifecycle(
        "registry_build",
        monitor::build_attrs(
            {{"tools", std::to_string(registry->all_any().size())},
             {"duration_ms", std::to_string(registry_duration_ms)}}));

    return registry;
}

ToolSurface resolve_tool_surface() {
    const char *raw = std::getenv("GODOT_AUTOPILOT_TOOL_SURFACE");
    if (raw == nullptr || *raw == '\0') {
        return ToolSurface::Meta;
    }
    const std::string value(raw);
    if (value == "full") {
        return ToolSurface::Full;
    }
    if (value != "meta") {
        LogSystem::instance().log(
            LogLevel::Warning, LogCategory::Tools,
            "unknown GODOT_AUTOPILOT_TOOL_SURFACE '" + value +
                "'; falling back to meta");
    }
    return ToolSurface::Meta;
}

bool tool_result_is_error(const godot_autopilot::util::JsonValue &result) {
    return !spec_detail::verdict_of(result).ok;
}

void refresh_dynamic_tools() {
    if (g_catalog == nullptr || g_index == nullptr)
        return;
    auto registry = build_registry(*g_catalog, *g_index, g_port);
    refresh_derived(registry, *g_catalog, *g_index);
    std::size_t dynamic_count = 0;
    {
        std::lock_guard<std::mutex> lock(dynamic_specs::mutex());
        dynamic_count = dynamic_specs::store().size();
    }
    monitor::lifecycle(
        "registry_refresh_dynamic",
        monitor::build_attrs({{"dynamic_tools", std::to_string(dynamic_count)}}));
}

std::shared_ptr<ToolRegistry> install_tool_handlers(CommandQueue &queue, ToolCatalog &catalog,
                                         Bm25Index &index, int port,
                                         ToolSurface surface) {
    (void)queue;
    (void)surface;
    g_catalog = &catalog;
    g_index = &index;
    g_port = port;
    auto registry = build_registry(catalog, index, port);
    refresh_derived(registry, catalog, index);
    const auto count = catalog.size();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        std::to_string(count) + " tools registered via registry");
    monitor::lifecycle(
        "tools_registered",
        monitor::build_attrs({{"count", std::to_string(count)}}));
    return registry;
}

std::shared_ptr<ToolRegistry> get_active_registry() {
    std::lock_guard<std::mutex> lock(g_active_registry_mutex);
    return g_active_registry;
}

void clear_active_registry() {
    std::lock_guard<std::mutex> lock(g_active_registry_mutex);
    g_active_registry = std::make_shared<ToolRegistry>();
    g_catalog = nullptr;
    g_index = nullptr;
}

}
