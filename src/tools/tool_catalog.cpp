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
}

} // namespace godot_self_driving
