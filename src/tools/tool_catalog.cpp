#include "tools/tool_catalog.hpp"

#include <algorithm>

namespace godot_self_driving {

ToolCatalog& ToolCatalog::instance() {
    static ToolCatalog inst;
    return inst;
}

void ToolCatalog::add_tool(const ToolInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_[info.name] = info;
}

const ToolInfo* ToolCatalog::get_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<const ToolInfo*> ToolCatalog::get_all_tools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const ToolInfo*> result;
    result.reserve(tools_.size());
    for (const auto& [_, info] : tools_) {
        result.push_back(&info);
    }
    return result;
}

std::vector<std::string> ToolCatalog::get_categories() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [_, info] : tools_) {
        if (std::find(result.begin(), result.end(), info.category) == result.end()) {
            result.push_back(info.category);
        }
    }
    return result;
}

size_t ToolCatalog::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.size();
}

void ToolCatalog::populate_default_tools() {
    if (size() > 0) return;

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"ping", "Health check ping", "Meta", {"health", "ping"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"system_status", "Get server status info", "System", {"status", "info"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");

        mcp::JsonValue props(mcp::JsonValue::object_tag);

        mcp::JsonValue query_prop(mcp::JsonValue::object_tag);
        query_prop["type"] = mcp::JsonValue("string");
        props["query"] = std::move(query_prop);

        mcp::JsonValue cat_prop(mcp::JsonValue::object_tag);
        cat_prop["type"] = mcp::JsonValue("string");
        props["category"] = std::move(cat_prop);

        mcp::JsonValue tags_prop(mcp::JsonValue::object_tag);
        tags_prop["type"] = mcp::JsonValue("array");
        mcp::JsonValue items(mcp::JsonValue::object_tag);
        items["type"] = mcp::JsonValue("string");
        tags_prop["items"] = std::move(items);
        props["tags"] = std::move(tags_prop);

        s["properties"] = std::move(props);

        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("query"));
        s["required"] = std::move(req);

        add_tool({"search_tools", "Search available tools by query", "Meta", {"search", "discovery"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"list_categories", "List all tool categories", "Meta", {"categories", "discovery"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");

        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue name_prop(mcp::JsonValue::object_tag);
        name_prop["type"] = mcp::JsonValue("string");
        props["name"] = std::move(name_prop);
        s["properties"] = std::move(props);

        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(req);

        add_tool({"get_tool_detail", "Get complete schema for one tool", "Meta", {"detail", "schema"}, std::move(s)});
    }

    // system_status (already in g_handlers but needs catalog entry)
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"system_status", "Get server status info", "System", {"status", "info"}, std::move(s)});
    }

    // call_tool — proxy
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        props["name"] = std::move(np);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("object");
        props["arguments"] = std::move(ap);
        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(req);
        add_tool({"call_tool", "Execute any tool by name", "Meta", {"proxy", "execute"}, std::move(s)});
    }

    // scene_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["parent_path"] = std::move(pp);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        props["name"] = std::move(np);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        props["type"] = std::move(tp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("parent_path"));
        s["required"] = std::move(rq);
        add_tool({"scene_node_create", "Create a new scene node as child of a parent", "Scene", {"node", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"scene_node_delete", "Delete a scene node by path", "Scene", {"node", "delete"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"scene_tree_get", "Get the full scene tree", "Scene", {"tree", "structure"}, std::move(s)});
    }

    // property_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        props["path"] = std::move(pp);
        mcp::JsonValue prp(mcp::JsonValue::object_tag);
        prp["type"] = mcp::JsonValue("string");
        props["property"] = std::move(prp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        rq.PushBack(mcp::JsonValue("property"));
        s["required"] = std::move(rq);
        add_tool({"property_get", "Get a property value from a scene node", "Properties", {"property", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        props["path"] = std::move(pp);
        mcp::JsonValue prp(mcp::JsonValue::object_tag);
        prp["type"] = mcp::JsonValue("string");
        props["property"] = std::move(prp);
        mcp::JsonValue vl(mcp::JsonValue::object_tag);
        vl["type"] = mcp::JsonValue("object");
        props["value"] = std::move(vl);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        rq.PushBack(mcp::JsonValue("property"));
        rq.PushBack(mcp::JsonValue("value"));
        s["required"] = std::move(rq);
        add_tool({"property_set", "Set a property value on a scene node", "Properties", {"property", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"property_get_list", "List all properties of a scene node", "Properties", {"property", "list"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue sp(mcp::JsonValue::object_tag);
        sp["type"] = mcp::JsonValue("string");
        props["source_path"] = std::move(sp);
        mcp::JsonValue sg(mcp::JsonValue::object_tag);
        sg["type"] = mcp::JsonValue("string");
        props["signal"] = std::move(sg);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        props["target_path"] = std::move(tp);
        mcp::JsonValue mt(mcp::JsonValue::object_tag);
        mt["type"] = mcp::JsonValue("string");
        props["method"] = std::move(mt);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("source_path"));
        rq.PushBack(mcp::JsonValue("signal"));
        rq.PushBack(mcp::JsonValue("target_path"));
        rq.PushBack(mcp::JsonValue("method"));
        s["required"] = std::move(rq);
        add_tool({"signal_connect", "Connect a signal from one node to another", "Properties", {"signal", "connect"}, std::move(s)});
    }

    // resource_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue thp(mcp::JsonValue::object_tag);
        thp["type"] = mcp::JsonValue("string");
        thp["description"] = mcp::JsonValue("Optional type hint for loading");
        props["type_hint"] = std::move(thp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_load", "Load a resource from file path", "Resources", {"resource", "load"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue thp(mcp::JsonValue::object_tag);
        thp["type"] = mcp::JsonValue("string");
        thp["description"] = mcp::JsonValue("Optional type hint");
        props["type_hint"] = std::move(thp);
        mcp::JsonValue usp(mcp::JsonValue::object_tag);
        usp["type"] = mcp::JsonValue("boolean");
        usp["description"] = mcp::JsonValue("Use sub-threads for loading");
        props["use_sub_threads"] = std::move(usp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_load_threaded", "Start threaded resource load", "Resources", {"resource", "load", "threaded"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_load_threaded_get_status", "Get status of threaded resource load", "Resources", {"resource", "load", "status"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_load_threaded_wait", "Wait for threaded resource load to complete", "Resources", {"resource", "load", "wait"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Source resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue dp(mcp::JsonValue::object_tag);
        dp["type"] = mcp::JsonValue("string");
        dp["description"] = mcp::JsonValue("Destination file path (defaults to same as path)");
        props["dest_path"] = std::move(dp);
        mcp::JsonValue fp(mcp::JsonValue::object_tag);
        fp["type"] = mcp::JsonValue("integer");
        fp["description"] = mcp::JsonValue("Saver flags bitfield");
        props["flags"] = std::move(fp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_save", "Save a resource to file", "Resources", {"resource", "save"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        tp["description"] = mcp::JsonValue("Resource class type (e.g. PackedScene, Material)");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["type"] = std::move(tp);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Optional resource name");
        props["name"] = std::move(np);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("type"));
        s["required"] = std::move(rq);
        add_tool({"resource_create", "Create a new resource instance by class type", "Resources", {"resource", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path to duplicate");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue dp(mcp::JsonValue::object_tag);
        dp["type"] = mcp::JsonValue("boolean");
        dp["description"] = mcp::JsonValue("Deep copy (default: false)");
        props["deep"] = std::move(dp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_duplicate", "Duplicate/instance a loaded resource", "Resources", {"resource", "duplicate"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_get_type", "Get the class type of a resource", "Resources", {"resource", "type"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_exists", "Check if a resource exists at path", "Resources", {"resource", "exists"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"resource_list_types", "List all instantiable Resource subclass types", "Resources", {"resource", "types", "list"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        tp["description"] = mcp::JsonValue("Resource type to get extensions for");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["type"] = std::move(tp);
        s["properties"] = std::move(props);
        add_tool({"resource_get_extensions", "Get recognized file extensions for a resource type", "Resources", {"resource", "extensions"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Directory path to list");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_list_dir", "List resource files in a directory", "Resources", {"resource", "directory", "list"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_get_uid", "Get the UID of a resource file", "Resources", {"resource", "uid", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue up(mcp::JsonValue::object_tag);
        up["type"] = mcp::JsonValue("integer");
        up["description"] = mcp::JsonValue("UID value (auto-generated if omitted)");
        props["uid"] = std::move(up);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_set_uid", "Set or assign a UID to a resource file", "Resources", {"resource", "uid", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path to remove");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_remove", "Delete a resource file from disk", "Resources", {"resource", "remove", "delete"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue fp(mcp::JsonValue::object_tag);
        fp["type"] = mcp::JsonValue("string");
        fp["description"] = mcp::JsonValue("Current file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["from"] = std::move(fp);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        tp["description"] = mcp::JsonValue("New file path");
        props["to"] = std::move(tp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("from"));
        rq.PushBack(mcp::JsonValue("to"));
        s["required"] = std::move(rq);
        add_tool({"resource_rename", "Rename/move a resource file", "Resources", {"resource", "rename", "move"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_get_dependencies", "List all dependencies of a resource", "Resources", {"resource", "dependencies", "list"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue dp(mcp::JsonValue::object_tag);
        dp["type"] = mcp::JsonValue("string");
        dp["description"] = mcp::JsonValue("Dependency path to check");
        props["dependency"] = std::move(dp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        rq.PushBack(mcp::JsonValue("dependency"));
        s["required"] = std::move(rq);
        add_tool({"resource_has_dependency", "Check if a resource depends on another file", "Resources", {"resource", "dependency", "check"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Resource file path to import");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"resource_import", "Import a single resource file (editor only)", "Resources", {"resource", "import"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Single file path to reimport");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("array");
        ap["description"] = mcp::JsonValue("Array of file paths to reimport");
        mcp::JsonValue ai(mcp::JsonValue::object_tag);
        ai["type"] = mcp::JsonValue("string");
        ap["items"] = std::move(ai);
        props["files"] = std::move(ap);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("files"));
        s["required"] = std::move(rq);
        add_tool({"resource_reimport", "Reimport one or more resource files (editor only)", "Resources", {"resource", "reimport"}, std::move(s)});
    }

    // script_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue ep(mcp::JsonValue::object_tag);
        ep["type"] = mcp::JsonValue("string");
        ep["description"] = mcp::JsonValue("GDScript expression to evaluate");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["expression"] = std::move(ep);
        mcp::JsonValue inp(mcp::JsonValue::object_tag);
        inp["type"] = mcp::JsonValue("array");
        inp["description"] = mcp::JsonValue("Variable names for the expression context");
        mcp::JsonValue ini(mcp::JsonValue::object_tag);
        ini["type"] = mcp::JsonValue("string");
        inp["items"] = std::move(ini);
        props["input_names"] = std::move(inp);
        mcp::JsonValue ivp(mcp::JsonValue::object_tag);
        ivp["type"] = mcp::JsonValue("array");
        ivp["description"] = mcp::JsonValue("Input values corresponding to input_names");
        mcp::JsonValue ivi(mcp::JsonValue::object_tag);
        ivi["type"] = mcp::JsonValue("object");
        ivp["items"] = std::move(ivi);
        props["input_values"] = std::move(ivp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("expression"));
        s["required"] = std::move(rq);
        add_tool({"script_execute_gdscript", "Execute an arbitrary GDScript expression", "Scripts", {"script", "execute", "gdscript"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Script file path to load");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"script_load", "Load a script from file path", "Scripts", {"script", "load"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("File path to save the new script");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue scp(mcp::JsonValue::object_tag);
        scp["type"] = mcp::JsonValue("string");
        scp["description"] = mcp::JsonValue("GDScript source code");
        props["source_code"] = std::move(scp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        rq.PushBack(mcp::JsonValue("source_code"));
        s["required"] = std::move(rq);
        add_tool({"script_create", "Create and save a new GDScript file", "Scripts", {"script", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Node path to attach script to");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["node_path"] = std::move(np);
        mcp::JsonValue sp(mcp::JsonValue::object_tag);
        sp["type"] = mcp::JsonValue("string");
        sp["description"] = mcp::JsonValue("Script file path to attach");
        props["script_path"] = std::move(sp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        rq.PushBack(mcp::JsonValue("script_path"));
        s["required"] = std::move(rq);
        add_tool({"script_attach_to_node", "Attach a script to a scene node", "Scripts", {"script", "attach", "node"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Node path to detach script from");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["node_path"] = std::move(np);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        s["required"] = std::move(rq);
        add_tool({"script_detach_from_node", "Detach script from a scene node", "Scripts", {"script", "detach", "node"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Property name to read");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["property"] = std::move(pp);
        mcp::JsonValue ssp(mcp::JsonValue::object_tag);
        ssp["type"] = mcp::JsonValue("string");
        ssp["description"] = mcp::JsonValue("Script path (optional, defaults to node if node_path set)");
        props["script_path"] = std::move(ssp);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Node path (optional)");
        props["node_path"] = std::move(npp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("property"));
        s["required"] = std::move(rq);
        add_tool({"script_get_property", "Get a script property's default value or a node's property", "Scripts", {"script", "property", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Node path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["node_path"] = std::move(npp);
        mcp::JsonValue prp(mcp::JsonValue::object_tag);
        prp["type"] = mcp::JsonValue("string");
        prp["description"] = mcp::JsonValue("Property name to set");
        props["property"] = std::move(prp);
        mcp::JsonValue vl(mcp::JsonValue::object_tag);
        vl["type"] = mcp::JsonValue("object");
        vl["description"] = mcp::JsonValue("Value to set");
        props["value"] = std::move(vl);
        mcp::JsonValue thp(mcp::JsonValue::object_tag);
        thp["type"] = mcp::JsonValue("string");
        thp["description"] = mcp::JsonValue("Optional type hint for deserialization");
        props["type_hint"] = std::move(thp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        rq.PushBack(mcp::JsonValue("property"));
        rq.PushBack(mcp::JsonValue("value"));
        s["required"] = std::move(rq);
        add_tool({"script_set_property", "Set a property on a node via script", "Scripts", {"script", "property", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Node path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["node_path"] = std::move(npp);
        mcp::JsonValue fp(mcp::JsonValue::object_tag);
        fp["type"] = mcp::JsonValue("string");
        fp["description"] = mcp::JsonValue("Function/method name to call");
        props["function"] = std::move(fp);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("array");
        ap["description"] = mcp::JsonValue("Arguments to pass (optional)");
        mcp::JsonValue ai(mcp::JsonValue::object_tag);
        ai["type"] = mcp::JsonValue("object");
        ap["items"] = std::move(ai);
        props["arguments"] = std::move(ap);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        rq.PushBack(mcp::JsonValue("function"));
        s["required"] = std::move(rq);
        add_tool({"script_call_function", "Call a function on a node via its script", "Scripts", {"script", "call", "function"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Script file path to reload");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        mcp::JsonValue ksp(mcp::JsonValue::object_tag);
        ksp["type"] = mcp::JsonValue("boolean");
        ksp["description"] = mcp::JsonValue("Keep state on reload (default: false)");
        props["keep_state"] = std::move(ksp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"script_reload", "Reload a script from disk", "Scripts", {"script", "reload"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Script file path");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"script_get_variable_list", "List all script variables and their types", "Scripts", {"script", "variables", "list"}, std::move(s)});
    }

    // physics_ops — 2D
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_space_get_direct_state", "Get the direct state of a 2D physics space", "Physics", {"physics", "2d", "space"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_ray_cast", "Cast a ray in 2D physics space", "Physics", {"physics", "2d", "ray"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_shape_cast", "Cast a shape in 2D physics space", "Physics", {"physics", "2d", "shape"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_point_query", "Query a point in 2D physics space", "Physics", {"physics", "2d", "point"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_intersect_shape", "Intersect a shape in 2D physics space", "Physics", {"physics", "2d", "intersect", "shape"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_intersect_point", "Intersect a point in 2D physics space", "Physics", {"physics", "2d", "intersect", "point"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_body_create", "Create a 2D physics body", "Physics", {"physics", "2d", "body", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_body_set_mode", "Set the mode of a 2D physics body", "Physics", {"physics", "2d", "body", "mode"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_body_apply_force", "Apply force to a 2D physics body", "Physics", {"physics", "2d", "body", "force"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_body_apply_impulse", "Apply impulse to a 2D physics body", "Physics", {"physics", "2d", "body", "impulse"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_body_set_state", "Set state of a 2D physics body", "Physics", {"physics", "2d", "body", "state"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_body_get_state", "Get state of a 2D physics body", "Physics", {"physics", "2d", "body", "state"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_joint_create", "Create a 2D physics joint", "Physics", {"physics", "2d", "joint", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_area_create", "Create a 2D physics area", "Physics", {"physics", "2d", "area", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_2d_area_set_monitorable", "Set monitorable flag on a 2D area", "Physics", {"physics", "2d", "area", "monitorable"}, std::move(s)});
    }

    // physics_ops — 3D
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_space_get_direct_state", "Get the direct state of a 3D physics space", "Physics", {"physics", "3d", "space"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_ray_cast", "Cast a ray in 3D physics space", "Physics", {"physics", "3d", "ray"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_shape_cast", "Cast a shape in 3D physics space", "Physics", {"physics", "3d", "shape"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_point_query", "Query a point in 3D physics space", "Physics", {"physics", "3d", "point"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_intersect_shape", "Intersect a shape in 3D physics space", "Physics", {"physics", "3d", "intersect", "shape"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_intersect_point", "Intersect a point in 3D physics space", "Physics", {"physics", "3d", "intersect", "point"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_create", "Create a 3D physics body", "Physics", {"physics", "3d", "body", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_set_mode", "Set the mode of a 3D physics body", "Physics", {"physics", "3d", "body", "mode"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_apply_force", "Apply force to a 3D physics body", "Physics", {"physics", "3d", "body", "force"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_apply_impulse", "Apply impulse to a 3D physics body", "Physics", {"physics", "3d", "body", "impulse"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_set_state", "Set state of a 3D physics body", "Physics", {"physics", "3d", "body", "state"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_get_state", "Get state of a 3D physics body", "Physics", {"physics", "3d", "body", "state"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_joint_create", "Create a 3D physics joint", "Physics", {"physics", "3d", "joint", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_area_create", "Create a 3D physics area", "Physics", {"physics", "3d", "area", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_area_set_monitorable", "Set monitorable flag on a 3D area", "Physics", {"physics", "3d", "area", "monitorable"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_apply_torque", "Apply torque to a 3D physics body", "Physics", {"physics", "3d", "body", "torque"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_set_axis_lock", "Set axis lock on a 3D physics body", "Physics", {"physics", "3d", "body", "axis", "lock"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_add_collision_exception", "Add collision exception to a 3D body", "Physics", {"physics", "3d", "body", "collision", "exception"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_body_remove_collision_exception", "Remove collision exception from a 3D body", "Physics", {"physics", "3d", "body", "collision", "exception"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_joint_set_param", "Set parameter on a 3D physics joint", "Physics", {"physics", "3d", "joint", "param"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_area_set_space_override", "Set space override on a 3D area", "Physics", {"physics", "3d", "area", "space", "override"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_space_set_gravity", "Set gravity on a 3D physics space", "Physics", {"physics", "3d", "space", "gravity"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_space_set_debug", "Set debug flag on a 3D physics space", "Physics", {"physics", "3d", "space", "debug"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_soft_body_create", "Create a 3D soft body", "Physics", {"physics", "3d", "soft", "body", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"physics_3d_soft_body_set_mesh", "Set mesh on a 3D soft body", "Physics", {"physics", "3d", "soft", "body", "mesh"}, std::move(s)});
    }

    // render_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"canvas_item_create", "Create a canvas item", "Render", {"render", "canvas", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"canvas_item_draw_rect", "Draw a rectangle on a canvas item", "Render", {"render", "canvas", "draw", "rect"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"canvas_item_draw_circle", "Draw a circle on a canvas item", "Render", {"render", "canvas", "draw", "circle"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"canvas_item_draw_texture", "Draw a texture on a canvas item", "Render", {"render", "canvas", "draw", "texture"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"canvas_item_draw_line", "Draw a line on a canvas item", "Render", {"render", "canvas", "draw", "line"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"canvas_item_set_transform", "Set transform on a canvas item", "Render", {"render", "canvas", "transform"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"canvas_item_set_visible", "Set visibility on a canvas item", "Render", {"render", "canvas", "visible"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"scenario_create", "Create a rendering scenario", "Render", {"render", "scenario", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"scenario_set_environment", "Set environment on a scenario", "Render", {"render", "scenario", "environment"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"camera_create", "Create a camera", "Render", {"render", "camera", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"camera_set_transform", "Set transform on a camera", "Render", {"render", "camera", "transform"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"camera_set_perspective", "Set perspective on a camera", "Render", {"render", "camera", "perspective"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"camera_set_orthogonal", "Set orthogonal on a camera", "Render", {"render", "camera", "orthogonal"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"light_create", "Create a light", "Render", {"render", "light", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"light_set_param", "Set parameter on a light", "Render", {"render", "light", "param"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"light_set_color", "Set color on a light", "Render", {"render", "light", "color"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"mesh_create", "Create a mesh", "Render", {"render", "mesh", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"mesh_add_surface", "Add a surface to a mesh", "Render", {"render", "mesh", "surface"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"mesh_set_material", "Set material on a mesh", "Render", {"render", "mesh", "material"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"material_create", "Create a material", "Render", {"render", "material", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"material_set_param", "Set parameter on a material", "Render", {"render", "material", "param"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"viewport_create", "Create a viewport", "Render", {"render", "viewport", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"viewport_set_size", "Set size on a viewport", "Render", {"render", "viewport", "size"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"viewport_set_clear_mode", "Set clear mode on a viewport", "Render", {"render", "viewport", "clear"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"particle_create", "Create a particle system", "Render", {"render", "particle", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"environment_set_bg_color", "Set background color on an environment", "Render", {"render", "environment", "bg", "color"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"environment_set_ambient", "Set ambient light on an environment", "Render", {"render", "environment", "ambient"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"fog_create", "Create a fog volume", "Render", {"render", "fog", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"shader_create", "Create a shader", "Render", {"render", "shader", "create"}, std::move(s)});
    }

    // nav_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_2d_map_create", "Create a 2D navigation map", "Nav", {"nav", "2d", "map", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_2d_region_create", "Create a 2D navigation region", "Nav", {"nav", "2d", "region", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_2d_path_query", "Query a 2D navigation path", "Nav", {"nav", "2d", "path", "query"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_2d_agent_create", "Create a 2D navigation agent", "Nav", {"nav", "2d", "agent", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_2d_agent_set_target", "Set target velocity for a 2D navigation agent", "Nav", {"nav", "2d", "agent", "target"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_map_create", "Create a 3D navigation map", "Nav", {"nav", "3d", "map", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_region_create", "Create a 3D navigation region", "Nav", {"nav", "3d", "region", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_path_query", "Query a 3D navigation path", "Nav", {"nav", "3d", "path", "query"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_path_query_segment", "Query the closest point on a segment in 3D navigation space", "Nav", {"nav", "3d", "path", "segment"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_agent_create", "Create a 3D navigation agent", "Nav", {"nav", "3d", "agent", "create"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_agent_set_velocity", "Set velocity for a 3D navigation agent", "Nav", {"nav", "3d", "agent", "velocity"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_agent_get_next_path", "Get next path position for a 3D navigation agent", "Nav", {"nav", "3d", "agent", "path"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_map_set_cell_size", "Set cell size on a 3D navigation map", "Nav", {"nav", "3d", "map", "cell"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_region_set_nav_mesh", "Set navigation mesh on a 3D region", "Nav", {"nav", "3d", "region", "mesh"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"nav_3d_obstacle_create", "Create a 3D navigation obstacle", "Nav", {"nav", "3d", "obstacle", "create"}, std::move(s)});
    }

    // audio_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"audio_bus_get_layout", "Get the audio bus layout", "Audio", {"audio", "bus"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue lp(mcp::JsonValue::object_tag);
        lp["type"] = mcp::JsonValue("object");
        lp["description"] = mcp::JsonValue("AudioBusLayout resource data");
        props["layout"] = std::move(lp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("layout"));
        s["required"] = std::move(rq);
        add_tool({"audio_bus_set_layout", "Set the audio bus layout", "Audio", {"audio", "bus"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"audio_bus_get_count", "Get the number of audio buses", "Audio", {"audio", "bus", "count"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue bip(mcp::JsonValue::object_tag);
        bip["type"] = mcp::JsonValue("integer");
        bip["description"] = mcp::JsonValue("Bus index");
        props["bus_index"] = std::move(bip);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("bus_index"));
        s["required"] = std::move(rq);
        add_tool({"audio_bus_get_name", "Get the name of an audio bus", "Audio", {"audio", "bus", "name"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue bip(mcp::JsonValue::object_tag);
        bip["type"] = mcp::JsonValue("integer");
        bip["description"] = mcp::JsonValue("Bus index");
        props["bus_index"] = std::move(bip);
        mcp::JsonValue vp(mcp::JsonValue::object_tag);
        vp["type"] = mcp::JsonValue("number");
        vp["description"] = mcp::JsonValue("Volume in dB");
        props["volume_db"] = std::move(vp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("bus_index"));
        rq.PushBack(mcp::JsonValue("volume_db"));
        s["required"] = std::move(rq);
        add_tool({"audio_bus_set_volume", "Set volume of an audio bus", "Audio", {"audio", "bus", "volume"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue bip(mcp::JsonValue::object_tag);
        bip["type"] = mcp::JsonValue("integer");
        bip["description"] = mcp::JsonValue("Bus index");
        props["bus_index"] = std::move(bip);
        mcp::JsonValue mp(mcp::JsonValue::object_tag);
        mp["type"] = mcp::JsonValue("boolean");
        mp["description"] = mcp::JsonValue("Whether the bus is muted");
        props["muted"] = std::move(mp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("bus_index"));
        rq.PushBack(mcp::JsonValue("muted"));
        s["required"] = std::move(rq);
        add_tool({"audio_bus_set_mute", "Mute or unmute an audio bus", "Audio", {"audio", "bus", "mute"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue bip(mcp::JsonValue::object_tag);
        bip["type"] = mcp::JsonValue("integer");
        bip["description"] = mcp::JsonValue("Bus index");
        props["bus_index"] = std::move(bip);
        mcp::JsonValue bp(mcp::JsonValue::object_tag);
        bp["type"] = mcp::JsonValue("boolean");
        bp["description"] = mcp::JsonValue("Whether to bypass effects");
        props["bypass"] = std::move(bp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("bus_index"));
        rq.PushBack(mcp::JsonValue("bypass"));
        s["required"] = std::move(rq);
        add_tool({"audio_bus_set_bypass", "Bypass or enable effects on an audio bus", "Audio", {"audio", "bus", "bypass"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue bip(mcp::JsonValue::object_tag);
        bip["type"] = mcp::JsonValue("integer");
        bip["description"] = mcp::JsonValue("Bus index");
        props["bus_index"] = std::move(bip);
        mcp::JsonValue etp(mcp::JsonValue::object_tag);
        etp["type"] = mcp::JsonValue("string");
        etp["description"] = mcp::JsonValue("Effect type name (e.g. AudioEffectReverb)");
        props["effect_type"] = std::move(etp);
        mcp::JsonValue app(mcp::JsonValue::object_tag);
        app["type"] = mcp::JsonValue("integer");
        app["description"] = mcp::JsonValue("Optional position to insert the effect");
        props["at_position"] = std::move(app);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("bus_index"));
        rq.PushBack(mcp::JsonValue("effect_type"));
        s["required"] = std::move(rq);
        add_tool({"audio_effect_add", "Add an audio effect to a bus", "Audio", {"audio", "effect", "add"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue bip(mcp::JsonValue::object_tag);
        bip["type"] = mcp::JsonValue("integer");
        bip["description"] = mcp::JsonValue("Bus index");
        props["bus_index"] = std::move(bip);
        mcp::JsonValue eip(mcp::JsonValue::object_tag);
        eip["type"] = mcp::JsonValue("integer");
        eip["description"] = mcp::JsonValue("Effect index to remove");
        props["effect_index"] = std::move(eip);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("bus_index"));
        rq.PushBack(mcp::JsonValue("effect_index"));
        s["required"] = std::move(rq);
        add_tool({"audio_effect_remove", "Remove an audio effect from a bus", "Audio", {"audio", "effect", "remove"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Path to AudioStreamPlayer node");
        props["node_path"] = std::move(npp);
        mcp::JsonValue spp(mcp::JsonValue::object_tag);
        spp["type"] = mcp::JsonValue("string");
        spp["description"] = mcp::JsonValue("Optional path to audio stream resource");
        props["stream_path"] = std::move(spp);
        mcp::JsonValue fpp(mcp::JsonValue::object_tag);
        fpp["type"] = mcp::JsonValue("number");
        fpp["description"] = mcp::JsonValue("Optional playback start position in seconds");
        props["from_position"] = std::move(fpp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        s["required"] = std::move(rq);
        add_tool({"audio_stream_play", "Play an audio stream on a player node", "Audio", {"audio", "stream", "play"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Path to AudioStreamPlayer node");
        props["node_path"] = std::move(npp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        s["required"] = std::move(rq);
        add_tool({"audio_stream_stop", "Stop an audio stream on a player node", "Audio", {"audio", "stream", "stop"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Path to AudioStreamPlayer node");
        props["node_path"] = std::move(npp);
        mcp::JsonValue vp(mcp::JsonValue::object_tag);
        vp["type"] = mcp::JsonValue("number");
        vp["description"] = mcp::JsonValue("Volume in dB");
        props["volume_db"] = std::move(vp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        rq.PushBack(mcp::JsonValue("volume_db"));
        s["required"] = std::move(rq);
        add_tool({"audio_stream_set_volume", "Set volume of an audio stream player", "Audio", {"audio", "stream", "volume"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Path to AudioStreamPlayer node");
        props["node_path"] = std::move(npp);
        mcp::JsonValue psp(mcp::JsonValue::object_tag);
        psp["type"] = mcp::JsonValue("number");
        psp["description"] = mcp::JsonValue("Pitch scale factor");
        props["pitch_scale"] = std::move(psp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        rq.PushBack(mcp::JsonValue("pitch_scale"));
        s["required"] = std::move(rq);
        add_tool({"audio_stream_set_pitch", "Set pitch scale of an audio stream player", "Audio", {"audio", "stream", "pitch"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Path to AudioStreamPlayer node");
        props["node_path"] = std::move(npp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        s["required"] = std::move(rq);
        add_tool({"audio_stream_get_playback_position", "Get current playback position of an audio stream", "Audio", {"audio", "stream", "position"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Path to AudioStreamPlayer node");
        props["node_path"] = std::move(npp);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("number");
        tp["description"] = mcp::JsonValue("Position to seek to in seconds");
        props["to_position"] = std::move(tp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        rq.PushBack(mcp::JsonValue("to_position"));
        s["required"] = std::move(rq);
        add_tool({"audio_stream_seek", "Seek an audio stream to a position", "Audio", {"audio", "stream", "seek"}, std::move(s)});
    }

    // input_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("string");
        ap["description"] = mcp::JsonValue("Input action name");
        props["action"] = std::move(ap);
        mcp::JsonValue sp(mcp::JsonValue::object_tag);
        sp["type"] = mcp::JsonValue("number");
        sp["description"] = mcp::JsonValue("Optional action strength (0.0-1.0)");
        props["strength"] = std::move(sp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("action"));
        s["required"] = std::move(rq);
        add_tool({"input_action_press", "Press an input action", "Input", {"input", "action", "press"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("string");
        ap["description"] = mcp::JsonValue("Input action name");
        props["action"] = std::move(ap);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("action"));
        s["required"] = std::move(rq);
        add_tool({"input_action_release", "Release an input action", "Input", {"input", "action", "release"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("string");
        ap["description"] = mcp::JsonValue("Input action name");
        props["action"] = std::move(ap);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("action"));
        s["required"] = std::move(rq);
        add_tool({"input_is_action_pressed", "Check if an input action is pressed", "Input", {"input", "action", "pressed"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("string");
        ap["description"] = mcp::JsonValue("Input action name");
        props["action"] = std::move(ap);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("action"));
        s["required"] = std::move(rq);
        add_tool({"input_is_action_just_pressed", "Check if an input action was just pressed", "Input", {"input", "action", "just_pressed"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue kp(mcp::JsonValue::object_tag);
        kp["type"] = mcp::JsonValue("string");
        kp["description"] = mcp::JsonValue("Key name (e.g. SPACE, ENTER, A)");
        props["key"] = std::move(kp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("key"));
        s["required"] = std::move(rq);
        add_tool({"input_key_press", "Simulate a key press", "Input", {"input", "key", "press"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue kp(mcp::JsonValue::object_tag);
        kp["type"] = mcp::JsonValue("string");
        kp["description"] = mcp::JsonValue("Key name (e.g. SPACE, ENTER, A)");
        props["key"] = std::move(kp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("key"));
        s["required"] = std::move(rq);
        add_tool({"input_key_release", "Simulate a key release", "Input", {"input", "key", "release"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("object");
        pp["description"] = mcp::JsonValue("Mouse position {x, y}");
        props["position"] = std::move(pp);
        mcp::JsonValue rp(mcp::JsonValue::object_tag);
        rp["type"] = mcp::JsonValue("object");
        rp["description"] = mcp::JsonValue("Optional relative movement {x, y}");
        props["relative"] = std::move(rp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("position"));
        s["required"] = std::move(rq);
        add_tool({"input_mouse_move", "Move the mouse cursor", "Input", {"input", "mouse", "move"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue bp(mcp::JsonValue::object_tag);
        bp["type"] = mcp::JsonValue("string");
        bp["description"] = mcp::JsonValue("Mouse button (LEFT, RIGHT, MIDDLE)");
        props["button"] = std::move(bp);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("object");
        pp["description"] = mcp::JsonValue("Optional mouse position {x, y}");
        props["position"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("button"));
        s["required"] = std::move(rq);
        add_tool({"input_mouse_button_press", "Press a mouse button", "Input", {"input", "mouse", "button", "press"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue bp(mcp::JsonValue::object_tag);
        bp["type"] = mcp::JsonValue("string");
        bp["description"] = mcp::JsonValue("Mouse button (LEFT, RIGHT, MIDDLE)");
        props["button"] = std::move(bp);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("object");
        pp["description"] = mcp::JsonValue("Optional mouse position {x, y}");
        props["position"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("button"));
        s["required"] = std::move(rq);
        add_tool({"input_mouse_button_release", "Release a mouse button", "Input", {"input", "mouse", "button", "release"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("string");
        ap["description"] = mcp::JsonValue("Gamepad action (STOP, VIBRATE)");
        props["action"] = std::move(ap);
        mcp::JsonValue dp(mcp::JsonValue::object_tag);
        dp["type"] = mcp::JsonValue("integer");
        dp["description"] = mcp::JsonValue("Optional device id");
        props["device"] = std::move(dp);
        mcp::JsonValue wmp(mcp::JsonValue::object_tag);
        wmp["type"] = mcp::JsonValue("number");
        wmp["description"] = mcp::JsonValue("Optional weak motor magnitude");
        props["weak_magnitude"] = std::move(wmp);
        mcp::JsonValue smp(mcp::JsonValue::object_tag);
        smp["type"] = mcp::JsonValue("number");
        smp["description"] = mcp::JsonValue("Optional strong motor magnitude");
        props["strong_magnitude"] = std::move(smp);
        mcp::JsonValue durp(mcp::JsonValue::object_tag);
        durp["type"] = mcp::JsonValue("number");
        durp["description"] = mcp::JsonValue("Optional vibration duration in seconds");
        props["duration"] = std::move(durp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("action"));
        s["required"] = std::move(rq);
        add_tool({"input_gamepad_simulate", "Simulate gamepad actions (stop/vibrate)", "Input", {"input", "gamepad", "simulate"}, std::move(s)});
    }

    // editor_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_get_selection", "Get currently selected nodes in the editor", "Editor", {"editor", "selection", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("array");
        mcp::JsonValue pi(mcp::JsonValue::object_tag);
        pi["type"] = mcp::JsonValue("string");
        pp["items"] = std::move(pi);
        pp["description"] = mcp::JsonValue("Array of node paths to select");
        props["paths"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("paths"));
        s["required"] = std::move(rq);
        add_tool({"editor_set_selection", "Set editor selection to specified nodes", "Editor", {"editor", "selection", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_get_edited_scene_root", "Get the root node of the currently edited scene", "Editor", {"editor", "scene", "root"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_save_scene", "Save the currently edited scene", "Editor", {"editor", "scene", "save"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_save_all_scenes", "Save all open scenes in the editor", "Editor", {"editor", "scene", "save_all"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue spp(mcp::JsonValue::object_tag);
        spp["type"] = mcp::JsonValue("string");
        spp["description"] = mcp::JsonValue("Optional scene file path to reload");
        props["scene_path"] = std::move(spp);
        s["properties"] = std::move(props);
        add_tool({"editor_reload_scene", "Reload a scene from disk", "Editor", {"editor", "scene", "reload"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue rpp(mcp::JsonValue::object_tag);
        rpp["type"] = mcp::JsonValue("string");
        rpp["description"] = mcp::JsonValue("Path to the resource to inspect");
        props["resource_path"] = std::move(rpp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("resource_path"));
        s["required"] = std::move(rq);
        add_tool({"editor_inspect_object", "Open the inspector for a resource", "Editor", {"editor", "inspect", "object"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue anp(mcp::JsonValue::object_tag);
        anp["type"] = mcp::JsonValue("string");
        anp["description"] = mcp::JsonValue("Name for the undo/redo action");
        props["action_name"] = std::move(anp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("action_name"));
        s["required"] = std::move(rq);
        add_tool({"editor_undo_redo_start", "Start an undo/redo action", "Editor", {"editor", "undo", "redo"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_undo_redo_commit", "Commit the current undo/redo action", "Editor", {"editor", "undo", "redo"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Node path to call method on");
        props["node_path"] = std::move(npp);
        mcp::JsonValue mp(mcp::JsonValue::object_tag);
        mp["type"] = mcp::JsonValue("string");
        mp["description"] = mcp::JsonValue("Method name to call");
        props["method"] = std::move(mp);
        mcp::JsonValue vp(mcp::JsonValue::object_tag);
        vp["type"] = mcp::JsonValue("object");
        vp["description"] = mcp::JsonValue("Optional value argument");
        props["value"] = std::move(vp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        rq.PushBack(mcp::JsonValue("method"));
        s["required"] = std::move(rq);
        add_tool({"editor_undo_redo_add_do", "Add a do-method to the current undo/redo action", "Editor", {"editor", "undo", "redo"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue npp(mcp::JsonValue::object_tag);
        npp["type"] = mcp::JsonValue("string");
        npp["description"] = mcp::JsonValue("Node path to call method on");
        props["node_path"] = std::move(npp);
        mcp::JsonValue mp(mcp::JsonValue::object_tag);
        mp["type"] = mcp::JsonValue("string");
        mp["description"] = mcp::JsonValue("Method name to call");
        props["method"] = std::move(mp);
        mcp::JsonValue vp(mcp::JsonValue::object_tag);
        vp["type"] = mcp::JsonValue("object");
        vp["description"] = mcp::JsonValue("Optional value argument");
        props["value"] = std::move(vp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("node_path"));
        rq.PushBack(mcp::JsonValue("method"));
        s["required"] = std::move(rq);
        add_tool({"editor_undo_redo_add_undo", "Add an undo-method to the current undo/redo action", "Editor", {"editor", "undo", "redo"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Optional filesystem path to browse");
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        add_tool({"editor_file_system_get_resources", "Browse the editor resource filesystem", "Editor", {"editor", "filesystem", "browse"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_file_system_scan", "Rescan the editor filesystem for changes", "Editor", {"editor", "filesystem", "scan"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Path to the resource to import");
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"editor_import_resource", "Import a resource (load and return its type)", "Editor", {"editor", "import", "resource"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Path to the scene file");
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        add_tool({"editor_set_main_scene", "Set the main scene for the project", "Editor", {"editor", "main_scene", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_play_current_scene", "Play the currently edited scene", "Editor", {"editor", "play", "scene"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_stop_playing", "Stop the currently running scene", "Editor", {"editor", "stop", "play"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_get_resource_filesystem", "Get the editor resource filesystem state", "Editor", {"editor", "filesystem", "status"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"editor_get_plugin_list", "List editor plugins (limited info)", "Editor", {"editor", "plugin", "list"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Plugin name");
        props["plugin"] = std::move(pp);
        mcp::JsonValue ep(mcp::JsonValue::object_tag);
        ep["type"] = mcp::JsonValue("boolean");
        ep["description"] = mcp::JsonValue("Whether to enable the plugin");
        props["enabled"] = std::move(ep);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("plugin"));
        rq.PushBack(mcp::JsonValue("enabled"));
        s["required"] = std::move(rq);
        add_tool({"editor_set_plugin_enabled", "Enable or disable an editor plugin", "Editor", {"editor", "plugin", "enable"}, std::move(s)});
    }

    // config_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Setting name");
        props["name"] = std::move(np);
        mcp::JsonValue dp(mcp::JsonValue::object_tag);
        dp["type"] = mcp::JsonValue("object");
        dp["description"] = mcp::JsonValue("Optional default value if not set");
        props["default"] = std::move(dp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(rq);
        add_tool({"project_settings_get", "Get a project setting by name", "Config", {"config", "project", "settings", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Setting name");
        props["name"] = std::move(np);
        mcp::JsonValue vp(mcp::JsonValue::object_tag);
        vp["type"] = mcp::JsonValue("object");
        vp["description"] = mcp::JsonValue("Value to set");
        props["value"] = std::move(vp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("name"));
        rq.PushBack(mcp::JsonValue("value"));
        s["required"] = std::move(rq);
        add_tool({"project_settings_set", "Set a project setting by name", "Config", {"config", "project", "settings", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Setting name");
        props["name"] = std::move(np);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(rq);
        add_tool({"project_settings_has", "Check if a project setting exists", "Config", {"config", "project", "settings", "has"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"project_settings_save", "Save project settings to disk", "Config", {"config", "project", "settings", "save"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"engine_get_version", "Get the Godot engine version info", "Config", {"config", "engine", "version"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"engine_get_fps", "Get the current FPS", "Config", {"config", "engine", "fps"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"engine_get_frames_drawn", "Get total frames drawn since engine start", "Config", {"config", "engine", "frames"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue sp(mcp::JsonValue::object_tag);
        sp["type"] = mcp::JsonValue("number");
        sp["description"] = mcp::JsonValue("Time scale factor");
        props["scale"] = std::move(sp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("scale"));
        s["required"] = std::move(rq);
        add_tool({"engine_set_time_scale", "Set the engine time scale", "Config", {"config", "engine", "time_scale", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"engine_get_time_scale", "Get the current engine time scale", "Config", {"config", "engine", "time_scale", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue fp(mcp::JsonValue::object_tag);
        fp["type"] = mcp::JsonValue("integer");
        fp["description"] = mcp::JsonValue("Max FPS value");
        props["fps"] = std::move(fp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("fps"));
        s["required"] = std::move(rq);
        add_tool({"engine_set_max_fps", "Set the engine max FPS cap", "Config", {"config", "engine", "max_fps", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Editor setting name");
        props["name"] = std::move(np);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(rq);
        add_tool({"editor_settings_get", "Get an editor setting by name", "Config", {"config", "editor", "settings", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Editor setting name");
        props["name"] = std::move(np);
        mcp::JsonValue vp(mcp::JsonValue::object_tag);
        vp["type"] = mcp::JsonValue("object");
        vp["description"] = mcp::JsonValue("Value to set");
        props["value"] = std::move(vp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("name"));
        rq.PushBack(mcp::JsonValue("value"));
        s["required"] = std::move(rq);
        add_tool({"editor_settings_set", "Set an editor setting by name", "Config", {"config", "editor", "settings", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        np["description"] = mcp::JsonValue("Editor setting name");
        props["name"] = std::move(np);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(rq);
        add_tool({"editor_settings_has", "Check if an editor setting exists", "Config", {"config", "editor", "settings", "has"}, std::move(s)});
    }

    // debug_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue mp(mcp::JsonValue::object_tag);
        mp["type"] = mcp::JsonValue("string");
        mp["description"] = mcp::JsonValue("Message to print");
        props["message"] = std::move(mp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("message"));
        s["required"] = std::move(rq);
        add_tool({"debug_print", "Print a debug log message", "Debug", {"debug", "print", "log"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ivp(mcp::JsonValue::object_tag);
        ivp["type"] = mcp::JsonValue("boolean");
        ivp["description"] = mcp::JsonValue("Include variable values in stack trace");
        props["include_variables"] = std::move(ivp);
        s["properties"] = std::move(props);
        add_tool({"debug_print_stack", "Print the current script call stack", "Debug", {"debug", "stack", "trace"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue mp(mcp::JsonValue::object_tag);
        mp["type"] = mcp::JsonValue("integer");
        mp["description"] = mcp::JsonValue("Performance monitor ID (0-58)");
        props["monitor"] = std::move(mp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("monitor"));
        s["required"] = std::move(rq);
        add_tool({"debug_get_performance_monitor", "Get a specific performance monitor value by ID", "Debug", {"debug", "performance", "monitor", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"debug_list_performance_monitors", "List all available performance monitors", "Debug", {"debug", "performance", "monitor", "list"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"debug_get_object_count", "Get total live object count", "Debug", {"debug", "objects", "count"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"debug_get_object_count_by_class", "Get object count by class (limited availability)", "Debug", {"debug", "objects", "class"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"debug_get_memory_usage", "Get current static memory usage in bytes", "Debug", {"debug", "memory", "usage"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"debug_profile_start", "Start profiling (not available via godot-cpp)", "Debug", {"debug", "profile", "start"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"debug_profile_stop", "Stop profiling (not available via godot-cpp)", "Debug", {"debug", "profile", "stop"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        add_tool({"debug_profile_get_data", "Get profiling data (not available via godot-cpp)", "Debug", {"debug", "profile", "data"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue fp(mcp::JsonValue::object_tag);
        fp["type"] = mcp::JsonValue("integer");
        fp["description"] = mcp::JsonValue("FPS limit value");
        props["fps"] = std::move(fp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("fps"));
        s["required"] = std::move(rq);
        add_tool({"debug_set_fps_limit", "Set a dynamic FPS limit for the engine", "Debug", {"debug", "fps", "limit", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue fp(mcp::JsonValue::object_tag);
        fp["type"] = mcp::JsonValue("integer");
        fp["description"] = mcp::JsonValue("Physics FPS value");
        props["fps"] = std::move(fp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("fps"));
        s["required"] = std::move(rq);
        add_tool({"debug_set_physics_fps", "Set the physics FPS (ticks per second)", "Debug", {"debug", "physics", "fps", "set"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ep(mcp::JsonValue::object_tag);
        ep["type"] = mcp::JsonValue("boolean");
        ep["description"] = mcp::JsonValue("Enable or disable collision debug visualization");
        props["enabled"] = std::move(ep);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("enabled"));
        s["required"] = std::move(rq);
        add_tool({"debug_collision_debug", "Toggle collision debug visualization", "Debug", {"debug", "collision", "visualize"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ep(mcp::JsonValue::object_tag);
        ep["type"] = mcp::JsonValue("boolean");
        ep["description"] = mcp::JsonValue("Enable or disable navigation debug visualization");
        props["enabled"] = std::move(ep);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("enabled"));
        s["required"] = std::move(rq);
        add_tool({"debug_navigation_debug", "Toggle navigation debug visualization", "Debug", {"debug", "navigation", "visualize"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue ep(mcp::JsonValue::object_tag);
        ep["type"] = mcp::JsonValue("boolean");
        ep["description"] = mcp::JsonValue("Enable or disable performance debug overlay");
        props["enabled"] = std::move(ep);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("enabled"));
        s["required"] = std::move(rq);
        add_tool({"debug_performance_debug", "Toggle performance debug overlay in editor", "Debug", {"debug", "performance", "overlay"}, std::move(s)});
    }

    // doc_ops
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue cp(mcp::JsonValue::object_tag);
        cp["type"] = mcp::JsonValue("string");
        cp["description"] = mcp::JsonValue("Class name to get documentation for");
        props["class"] = std::move(cp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("class"));
        s["required"] = std::move(rq);
        add_tool({"doc_get_class", "Get detailed documentation for a Godot class", "Docs", {"docs", "class", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue qp(mcp::JsonValue::object_tag);
        qp["type"] = mcp::JsonValue("string");
        qp["description"] = mcp::JsonValue("Search query");
        props["query"] = std::move(qp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("query"));
        s["required"] = std::move(rq);
        add_tool({"doc_search", "Search Godot classes by name", "Docs", {"docs", "search", "class"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue cp(mcp::JsonValue::object_tag);
        cp["type"] = mcp::JsonValue("string");
        cp["description"] = mcp::JsonValue("Class name");
        props["class"] = std::move(cp);
        mcp::JsonValue mp(mcp::JsonValue::object_tag);
        mp["type"] = mcp::JsonValue("string");
        mp["description"] = mcp::JsonValue("Method name");
        props["method"] = std::move(mp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("class"));
        rq.PushBack(mcp::JsonValue("method"));
        s["required"] = std::move(rq);
        add_tool({"doc_get_method", "Get signature info for a specific method on a class", "Docs", {"docs", "method", "get"}, std::move(s)});
    }

    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue cp(mcp::JsonValue::object_tag);
        cp["type"] = mcp::JsonValue("string");
        cp["description"] = mcp::JsonValue("Class name");
        props["class"] = std::move(cp);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        pp["description"] = mcp::JsonValue("Property name");
        props["property"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("class"));
        rq.PushBack(mcp::JsonValue("property"));
        s["required"] = std::move(rq);
        add_tool({"doc_get_property", "Get property info for a specific property on a class", "Docs", {"docs", "property", "get"}, std::move(s)});
    }
}

} // namespace godot_self_driving
