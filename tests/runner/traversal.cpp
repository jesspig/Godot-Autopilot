#include "traversal.hpp"

#include "integration/mcp_test_client.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef PROJECT_ROOT
#error "PROJECT_ROOT 编译宏未定义（tests/CMakeLists.txt 已为 gda_test_runner 配置）"
#endif

namespace {

const char* kModeEmptyArgs = "empty_args";
const char* kModeHeuristicSmoke = "heuristic_smoke";

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

// 经 call_tool 元工具代理调用领域工具；崩溃后响应文本可能为空/非 JSON。
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
              << " | 排除(副作用): " << s.excluded << "\n";
    for (const auto& n : s.warnings) {
        std::cout << "  [schema 必填但空参未报错] " << n << "\n";
    }
    for (const auto& n : non_object_names) {
        std::cout << "  [响应非 JSON 对象] " << n << "\n";
    }
    for (const auto& n : s.excluded_names) {
        std::cout << "  [排除-副作用，跳过调用] " << n << "\n";
    }
}

} // namespace

namespace gda_test {

std::vector<std::string> parse_domain_tool_names() {
    const std::filesystem::path dir =
        std::filesystem::path(std::string(PROJECT_ROOT)) / "src" / "tools";
    if (!std::filesystem::exists(dir) ||
        !std::filesystem::is_directory(dir)) {
        throw std::runtime_error("工具源目录不存在或非目录: " + dir.string());
    }
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        const std::string fname = entry.path().filename().string();
        if (!entry.is_regular_file() ||
            entry.path().extension() != ".hpp" ||
            !(fname.size() >= 10 &&
              fname.compare(fname.size() - 10, 10, "_tools.hpp") == 0)) {
            continue;
        }
        const std::string file_path = entry.path().string();
        std::ifstream in(file_path);
        if (!in.is_open()) {
            throw std::runtime_error("无法打开 " + file_path);
        }
        std::string line;
        size_t line_no = 0;
        while (std::getline(in, line)) {
            ++line_no;
            const size_t p = line.find("GDA_TOOL_CLASS(");
            if (p == std::string::npos)
                continue;
            const size_t comma = line.find(',', p);
            if (comma == std::string::npos) {
                throw std::runtime_error(file_path + ":" +
                                         std::to_string(line_no) +
                                         " 含 GDA_TOOL_CLASS( 但缺逗号，无法解析工具名");
            }
            const size_t q1 = line.find('"', comma);
            if (q1 == std::string::npos) {
                throw std::runtime_error(file_path + ":" +
                                         std::to_string(line_no) +
                                         " 缺工具名引号字段，无法解析工具名");
            }
            const size_t q2 = line.find('"', q1 + 1);
            if (q2 == std::string::npos) {
                throw std::runtime_error(file_path + ":" +
                                         std::to_string(line_no) +
                                         " 工具名引号字段未闭合，无法解析工具名");
            }
            names.push_back(line.substr(q1 + 1, q2 - q1 - 1));
        }
    }
    if (names.empty()) {
        throw std::runtime_error(
            dir.string() + " 下未解析出任何工具名（未匹配 *_tools.hpp 或 GDA_TOOL_CLASS(）");
    }
    return names;
}

TraversalStats run_traversal(McpTestClient& client, const std::string& mode,
                             std::vector<StepResult>& out_steps) {
    if (mode != kModeEmptyArgs && mode != kModeHeuristicSmoke) {
        throw std::invalid_argument("run_traversal: 未知 mode '" + mode +
                                    "'（仅支持 empty_args / heuristic_smoke）");
    }
    const bool is_empty_args = (mode == kModeEmptyArgs);

    std::vector<std::string> names = parse_domain_tool_names();
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

        // 副作用判定：读取 get_tool_detail 返回的 tool.side_effect，非空即排除
        std::string side_effect;
        if (detail.IsObject() && tool && tool->IsObject()) {
            const auto* se = tool->Find("side_effect");
            if (se && se->IsString())
                side_effect = se->GetString();
        }
        if (!side_effect.empty()) {
            stats.excluded_names.push_back(name);
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
            // 历史教训：create_scene_node/get_resource_extensions/
            // reimport_resource_files 带默认值的必填参数不校验 → 仅记 warnings，
            // 不 FAIL。
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
                // SCHEMA_NONE / 无 properties：只走空参契约，不冒烟
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
