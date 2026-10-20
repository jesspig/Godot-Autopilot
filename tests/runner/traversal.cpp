#include "traversal.hpp"

#include "integration/mcp_test_client.hpp"

#include "util/owned_exception.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

const char* kModeEmptyArgs = "empty_args";
const char* kModeHeuristicSmoke = "heuristic_smoke";

const char* kSideEffectNone = "none";

constexpr size_t kBaselineEnumerated = 452;
constexpr size_t kBaselineExcluded = 147;
constexpr size_t kBaselineCandidates = 305;
constexpr size_t kBaselineWarnings = 1;

constexpr size_t kErrorDetailMax = 300;

const char* const kExpectedErrorMarkers[] = {
    "requir",      "missing",    "invalid",     "must be",   "expected",
    "unknown",     "not found",  "not available", "is not",  "not running",
    "not ready",   "no scene",   "no edited scene", "no window", "no theme",
    "unsupported", "cannot",     "failed to",   "does not exist", "has no",
    "already",     "did not return a resource", "mutually exclusive",
    "must contain", "must have", "no object found",
    "out of range", "unsaved changes"};

const std::vector<std::string> kMetaToolNames = {
    "ping",      "search_tools",  "list_categories", "get_tool_detail",
    "call_tool", "batch_execute", "code_execute"};

bool has_error_field(const mcp::JsonValue& j) {
    return j.IsObject() && j.Find("error") != nullptr;
}

std::string error_message_text(const mcp::JsonValue& resp) {
    if (!resp.IsObject()) {
        return std::string();
    }
    const auto* err = resp.Find("error");
    if (!err) {
        return std::string();
    }
    if (err->IsString()) {
        return err->GetString();
    }
    return err->Dump();
}

std::string truncate_text(const std::string& text) {
    if (text.size() <= kErrorDetailMax) {
        return text;
    }
    return text.substr(0, kErrorDetailMax) + " (truncated)";
}

bool contains_ignore_case(const std::string& text, const char* marker) {
    const size_t marker_len = std::string(marker).size();
    if (marker_len == 0 || text.size() < marker_len) {
        return false;
    }
    for (size_t i = 0; i + marker_len <= text.size(); ++i) {
        size_t j = 0;
        while (j < marker_len &&
               std::tolower(static_cast<unsigned char>(text[i + j])) ==
                   std::tolower(static_cast<unsigned char>(marker[j]))) {
            ++j;
        }
        if (j == marker_len) {
            return true;
        }
    }
    return false;
}

bool is_expected_error(const mcp::JsonValue& resp, const std::string& message) {
    if (resp.IsObject() && resp.Find("structured_error") != nullptr) {
        return true;
    }
    for (const char* marker : kExpectedErrorMarkers) {
        if (contains_ignore_case(message, marker)) {
            return true;
        }
    }
    return false;
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
              << "  枚举数: " << s.enumerated << " | 排除数: " << s.excluded
              << " | 候选数: " << s.candidates << "\n"
              << "  调用总数: " << s.total << " | 通过: " << s.passed
              << " | 失败: " << s.failed
              << " | result: " << results << " | error: " << errors
              << " | 豁免错误: " << s.exempt_errors
              << " | error 含 'missing required': " << missing_req
              << " | 跳过(无 schema properties): " << skipped
              << " | 排除(副作用/可变/dynamic): " << s.excluded << "\n";
    std::cout << "  基线比对: " << (s.baseline_matched ? "一致" : "不一致")
              << "\n";
    for (const auto& d : s.baseline_diffs) {
        std::cout << "  [基线差异] " << d << "\n";
    }
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

void compare_baseline(const std::string& mode, gda_test::TraversalStats& stats) {
    struct BaselineItem {
        const char* name;
        size_t actual;
        size_t expected;
    };
    const size_t expected_warnings =
        (mode == kModeEmptyArgs) ? kBaselineWarnings : 0;
    const BaselineItem items[] = {
        {"enumerated", stats.enumerated, kBaselineEnumerated},
        {"excluded", stats.excluded, kBaselineExcluded},
        {"candidates", stats.candidates, kBaselineCandidates},
        {"warnings", stats.warnings.size(), expected_warnings},
    };
    for (const BaselineItem& item : items) {
        if (item.actual != item.expected) {
            stats.baseline_diffs.push_back(
                std::string(item.name) + ": 实际 " + std::to_string(item.actual) +
                " / 基线 " + std::to_string(item.expected));
        }
    }
    stats.baseline_matched = stats.baseline_diffs.empty();
}

} // namespace

namespace gda_test {

using godot_autopilot::RuntimeError;
using godot_autopilot::InvalidArgument;

std::vector<std::string> list_tool_names(McpTestClient& client) {
    const std::string text = client.call_tool("search_tools", "{}");
    const mcp::JsonValue parsed = mcp::JsonValue::Parse(text);
    const mcp::JsonValue* results =
        parsed.IsObject() ? parsed.Find("results") : nullptr;
    if (!results || !results->IsArray()) {
        throw RuntimeError("search_tools 空 query 未返回 results 数组: " +
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
        throw RuntimeError("search_tools 返回空工具清单");
    return names;
}

TraversalStats run_traversal(McpTestClient& client, const std::string& mode,
                             std::vector<StepResult>& out_steps) {
    if (mode != kModeEmptyArgs && mode != kModeHeuristicSmoke) {
        throw InvalidArgument("run_traversal: 未知 mode '" + mode +
                                    "'（仅支持 empty_args / heuristic_smoke）");
    }
    const bool is_empty_args = (mode == kModeEmptyArgs);

    std::vector<std::string> names = list_tool_names(client);
    if (!is_empty_args) {
        std::sort(names.begin(), names.end());
    }

    TraversalStats stats;
    stats.enumerated = names.size();
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
        const bool has_side_effect =
            !side_effect.empty() && side_effect != kSideEffectNone;
        if (has_side_effect || is_mutating)
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
            std::string error_text;
            if (errored) {
                ++errors;
                error_text = error_message_text(resp);
                if (error_text.find("missing required") != std::string::npos) {
                    ++missing_req;
                }
            } else {
                ++results;
            }
            if (!required.empty() && !errored) {
                stats.warnings.push_back("schema 必填但空参未报错: " + name);
            }
            StepResult sr;
            if (errored && !is_expected_error(resp, error_text)) {
                sr.passed = false;
                sr.detail = "工具 '" + name + "' 空参调用返回错误: " +
                            truncate_text(error_text);
            } else {
                sr.passed = true;
                if (errored) {
                    ++stats.exempt_errors;
                    sr.detail = "空参调用完成: " + name +
                                " (预期错误: " + truncate_text(error_text) + ")";
                } else {
                    sr.detail = "空参调用完成: " + name + " (result)";
                }
            }
            out_steps.push_back(std::move(sr));
            if (sr.passed) {
                ++stats.passed;
            } else {
                ++stats.failed;
            }
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
            std::string error_text;
            if (errored) {
                ++errors;
                error_text = error_message_text(resp);
            } else {
                ++results;
            }
            StepResult sr;
            if (errored && !is_expected_error(resp, error_text)) {
                sr.passed = false;
                sr.detail = "工具 '" + name + "' 启发式冒烟返回错误: " +
                            truncate_text(error_text);
            } else {
                sr.passed = true;
                if (errored) {
                    ++stats.exempt_errors;
                    sr.detail = "启发式冒烟完成: " + name +
                                " (预期错误: " + truncate_text(error_text) + ")";
                } else {
                    sr.detail = "启发式冒烟完成: " + name + " (result)";
                }
            }
            out_steps.push_back(std::move(sr));
            if (sr.passed) {
                ++stats.passed;
            } else {
                ++stats.failed;
            }
        }
    }

    stats.candidates = stats.enumerated - stats.excluded;
    compare_baseline(mode, stats);
    print_stats(mode, stats, results, errors, missing_req, skipped,
                non_object_names);
    return stats;
}

} // namespace gda_test
