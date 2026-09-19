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
    std::string id;
    std::string tool;
    mcp::JsonValue args;
    StepExpect expect;
    std::string traverse_kind;
    std::string traverse_mode;
};

struct TestCase {
    std::string name;
    std::string description;
    bool headless = true;
    std::string on_failure;
    std::vector<Step> before_all;
    std::vector<Step> steps;
    std::vector<Step> after_all;
};

TestCase load_test_case(const std::string& json_path);

}
