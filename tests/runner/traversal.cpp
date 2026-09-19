#include "traversal.hpp"

#include "integration/mcp_test_client.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

const char* kModeEmptyArgs = "empty_args";
const char* kModeHeuristicSmoke = "heuristic_smoke";

const std::vector<std::string> kMetaToolNames = {
    "ping",      "search_tools",  "list_categories", "get_tool_detail",
    "call_tool", "batch_execute", "code_execute"};

bool has_error_field(const mcp::JsonValue& j) {
    return j.IsObject() && j.Find("error") != nullptr;
}

std::vector<std::string> schema_required_params(const mcp::JsonValue& detail) {
    std::vector<std::string> out;
    const auto* tool = detail.Find("tool");
    if (!tool || !tool->IsObject())
        return out;
    const auto* schema = tool->Find("input_schema");
    if (!schema || !schema->IsObject())
        return out;
    const auto* req = schema->Find("required");
    if (!req || !req->IsArray())
        return out;
    for (const auto& v : req->GetArray()) {
        if (v.IsString())
            out.push_back(v.GetString());
    }
    return out;
}

std::vector<std::pair<std::string, std::string>>
schema_property_types(const mcp::JsonValue& detail) {
    std::vector<std::pair<std::string, std::string>> out;
    const auto* tool = detail.Find("tool");
    if (!tool || !tool->IsObject())
        return out;
    const auto* schema = tool->Find("input_schema");
    if (!schema || !schema->IsObject())
        return out;
    const auto* props = schema->Find("properties");
    if (!props || !props->IsObject())
        return out;
    for (const auto& [key, value] : props->GetObject()) {
        std::string type = "string";
        if (value.IsObject()) {
            if (const auto* t = value.Find("type")) {
                if (t->IsString())
                    type = t->GetString();
            }
        }
        out.emplace_back(key, type);
    }
    return out;
}

mcp::JsonValue heuristic_value(const std::string& type) {
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

mcp::JsonValue call_domain_tool(gda_test::McpTestClient& client,
                                const std::string& name,
                                const mcp::JsonValue& args,
                                std::string* raw_out) {
    mcp::JsonValue envelope(mcp::JsonValue::object_tag);
    envelope["name"] = mcp::JsonValue(name);
    envelope["arguments"] = args;
    const std::string text = client.call_tool("call_tool", envelope.Dump());
    if (raw_out)
        *raw_out = text;
    return mcp::JsonValue::Parse(text);
}

void print_stats(const std::string& mode, const gda_test::TraversalStats& s,
                 size_t results, size_t errors, size_t missing_req,
                 size_t skipped,
                 const std::vector<std::string>& non_object_names) {
    std::cout << "\n=== 工具遍历: " << mode << " ===\n"
              << "  调用总数: " << s.total << " | 通过: " << s.passed
              << " | 失败: " << s.failed
              << " | result: " << results << " | error: " << errors
              << " | error 含 'missing required': " << missing_req
              << " | 跳过(无 schema properties): " << skipped
              << " | 排除(副作用/可变/dynamic): " << s.excluded << "\n";
    for (const auto& n : s.warnings) {
        std::cout << "  [schema 必填但空参未报错] " << n << "\n";
    }
    for (const auto& n : non_object_names) {
        std::cout << "  [响应非 JSON 对象] " << n << "\n";
    }
    for (const auto& n : s.excluded_names) {
        std::cout << "  " << n << "\n";
    }
}

} // namespace

namespace gda_test {

std::vector<std::string> list_tool_names(McpTestClient& client) {
    const std::string text = client.call_tool("search_tools", "{}");
    const mcp::JsonValue parsed = mcp::JsonValue::Parse(text);
    const mcp::JsonValue* results =
        parsed.IsObject() ? parsed.Find("results") : nullptr;
    if (!results || !results->IsArray()) {
        throw std::runtime_error("search_tools 空 query 未返回 results 数组: " +
                                 text.substr(0, 300));
    }
    std::vector<std::string> names;
    for (const auto& item : results->GetArray()) {
        if (!item.IsObject())
            continue;
        const mcp::JsonValue* name = item.Find("name");
        if (!name || !name->IsString())
            continue;
        const std::string tool_name = name->GetString();
        if (std::find(kMetaToolNames.begin(), kMetaToolNames.end(),
                      tool_name) != kMetaToolNames.end()) {
            continue;
        }
        names.push_back(tool_name);
    }
    if (names.empty())
        throw std::runtime_error("search_tools 返回空工具清单");
    return names;
}

TraversalStats run_traversal(McpTestClient& client, const std::string& mode,
                             std::vector<StepResult>& out_steps) {
    if (mode != kModeEmptyArgs && mode != kModeHeuristicSmoke) {
        throw std::invalid_argument("run_traversal: 未知 mode '" + mode +
                                    "'（仅支持 empty_args / heuristic_smoke）");
    }
    const bool is_empty_args = (mode == kModeEmptyArgs);

    std::vector<std::string> names = list_tool_names(client);
    if (!is_empty_args) {
        std::sort(names.begin(), names.end());
    }

    TraversalStats stats;
    size_t results = 0;
    size_t errors = 0;
    size_t missing_req = 0;
    size_t skipped = 0;
    std::vector<std::string> non_object_names;

    for (const std::string& name : names) {
        mcp::JsonValue detail_args(mcp::JsonValue::object_tag);
        detail_args["name"] = mcp::JsonValue(name);
        const std::string detail_text =
            client.call_tool("get_tool_detail", detail_args.Dump());
        const mcp::JsonValue detail = mcp::JsonValue::Parse(detail_text);
        const auto* tool = detail.IsObject() ? detail.Find("tool") : nullptr;

        std::string side_effect;
        bool is_mutating = false;
        bool is_dynamic = false;
        if (detail.IsObject() && tool && tool->IsObject()) {
            const auto* se = tool->Find("side_effect");
            if (se && se->IsString())
                side_effect = se->GetString();
            const auto* mutating = tool->Find("mutating");
            if (mutating && mutating->IsBool())
                is_mutating = mutating->GetBool();
            const auto* dynamic = tool->Find("dynamic");
            if (dynamic && dynamic->IsBool())
                is_dynamic = dynamic->GetBool();
        }
        std::string exclude_reason;
        if (!side_effect.empty() || is_mutating)
            exclude_reason = "副作用/可变";
        if (is_dynamic) {
            exclude_reason = exclude_reason.empty()
                                 ? "dynamic"
                                 : exclude_reason + "/dynamic";
        }
        if (!exclude_reason.empty()) {
            stats.excluded_names.push_back("[排除-" + exclude_reason +
                                           "，跳过调用] " + name);
            ++stats.excluded;
            continue;
        }

        if (!detail.IsObject() || !tool || !tool->IsObject()) {
            StepResult sr;
            sr.passed = false;
            sr.detail = "get_tool_detail('" + name +
                        "') 响应非 JSON 对象或缺 tool 字段: " +
                        detail_text.substr(0, 300);
            out_steps.push_back(std::move(sr));
            non_object_names.push_back(name);
            ++stats.failed;
            continue;
        }
        const auto* tool_name = tool->Find("name");
        if (!tool_name || !tool_name->IsString() ||
            tool_name->GetString() != name) {
            StepResult sr;
            sr.passed = false;
            sr.detail = "get_tool_detail('" + name + "') 返回的工具名不匹配";
            out_steps.push_back(std::move(sr));
            ++stats.failed;
            continue;
        }

        if (is_empty_args) {
            const std::vector<std::string> required =
                schema_required_params(detail);
            std::string raw;
            const mcp::JsonValue resp = call_domain_tool(
                client, name, mcp::JsonValue(mcp::JsonValue::object_tag),
                &raw);
            ++stats.total;
            if (!resp.IsObject()) {
                StepResult sr;
                sr.passed = false;
                sr.detail = "工具 '" + name +
                            "' 空参数响应非 JSON 对象: " + raw.substr(0, 300);
                out_steps.push_back(std::move(sr));
                non_object_names.push_back(name);
                ++stats.failed;
                continue;
            }
            const bool errored = has_error_field(resp);
            if (errored) {
                ++errors;
                const auto* err = resp.Find("error");
                const std::string msg =
                    err && err->IsString() ? err->GetString() : "";
                if (msg.find("missing required") != std::string::npos) {
                    ++missing_req;
                }
            } else {
                ++results;
            }
            if (!required.empty() && !errored) {
                stats.warnings.push_back("schema 必填但空参未报错: " + name);
            }
            StepResult sr;
            sr.passed = true;
            sr.detail = "空参调用完成: " + name +
                        (errored ? " (error)" : " (result)");
            out_steps.push_back(std::move(sr));
            ++stats.passed;
        } else {
            const auto props = schema_property_types(detail);
            if (props.empty()) {
                ++skipped;
                continue;
            }
            mcp::JsonValue args(mcp::JsonValue::object_tag);
            for (const auto& [key, type] : props) {
                args[key] = heuristic_value(type);
            }
            std::string raw;
            const mcp::JsonValue resp = call_domain_tool(client, name, args,
                                                         &raw);
            ++stats.total;
            if (!resp.IsObject()) {
                StepResult sr;
                sr.passed = false;
                sr.detail = "工具 '" + name +
                            "' 启发式参数响应非 JSON 对象: " + raw.substr(0, 300);
                out_steps.push_back(std::move(sr));
                non_object_names.push_back(name);
                ++stats.failed;
                continue;
            }
            const bool errored = has_error_field(resp);
            if (errored) {
                ++errors;
            } else {
                ++results;
            }
            StepResult sr;
            sr.passed = true;
            sr.detail = "启发式冒烟完成: " + name +
                        (errored ? " (error)" : " (result)");
            out_steps.push_back(std::move(sr));
            ++stats.passed;
        }
    }

    print_stats(mode, stats, results, errors, missing_req, skipped,
                non_object_names);
    return stats;
}

} // namespace gda_test
