#pragma once
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

namespace gda_test {

struct StepExpect {
    std::vector<std::string> has_keys;
    struct FieldCheck {
        std::string key;
        bool has_value = false;
        mcp::JsonValue value;
        bool not_empty = false;
    };
    std::vector<FieldCheck> field_checks;
};

struct Step {
    std::string id;                  // 步骤 id（无则用 tool 名或序号）
    std::string tool;                // 领域工具名；空 = 遍历步骤
    mcp::JsonValue args;             // object；可空
    StepExpect expect;               // 可空（before_all/after_all 常无）
    std::string traverse_kind;       // 仅遍历步骤："domain_tools"
    std::string traverse_mode;       // "empty_args" | "heuristic_smoke"
};

struct TestCase {
    std::string name;
    std::string description;
    bool headless = true;
    std::string on_failure;          // "fail_fast"（默认）| "continue"
    std::vector<Step> before_all;
    std::vector<Step> steps;         // stages 分组平铺后的全部步骤
    std::vector<Step> after_all;
};

// 加载并校验单个 JSON 用例文件；任何 schema 违规抛 std::runtime_error，
// 错误消息含字段路径（如 "pipeline.stages[0].steps[1].expect.field_checks[0].key"）
TestCase load_test_case(const std::string& json_path);

}
