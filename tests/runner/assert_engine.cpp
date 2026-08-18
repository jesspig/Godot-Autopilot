#include "assert_engine.hpp"

#include <vector>

namespace gda_test {
namespace {

std::string join_keys(const std::vector<std::string>& keys) {
    std::string joined;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) joined += ", ";
        joined += keys[i];
    }
    return joined;
}

bool numbers_equal(const mcp::JsonValue& expected, const mcp::JsonValue& actual) {
    const double expected_value = expected.IsInt() ? static_cast<double>(expected.GetInt())
                                                   : expected.GetDouble();
    const double actual_value = actual.IsInt() ? static_cast<double>(actual.GetInt())
                                               : actual.GetDouble();
    return expected_value == actual_value;
}

bool values_equal(const mcp::JsonValue& expected, const mcp::JsonValue& actual) {
    if (expected.IsNumber() && actual.IsNumber()) {
        return numbers_equal(expected, actual);
    }
    if (expected.IsString() && actual.IsString()) {
        return expected.GetString() == actual.GetString();
    }
    if (expected.IsBool() && actual.IsBool()) {
        return expected.GetBool() == actual.GetBool();
    }
    return expected.Dump() == actual.Dump();
}

bool is_not_empty(const mcp::JsonValue& value) {
    if (value.IsString()) return !value.GetString().empty();
    if (value.IsArray() || value.IsObject()) return value.Size() > 0;
    if (value.IsNull()) return false;
    return true;
}

}  // namespace

const mcp::JsonValue* json_lookup(const mcp::JsonValue& root, const std::string& dotted_path) {
    if (dotted_path.empty()) return &root;
    const mcp::JsonValue* current = &root;
    size_t start = 0;
    while (start < dotted_path.size()) {
        if (!current->IsObject()) return nullptr;
        const size_t dot = dotted_path.find('.', start);
        const std::string segment = (dot == std::string::npos)
                                        ? dotted_path.substr(start)
                                        : dotted_path.substr(start, dot - start);
        current = current->Find(segment);
        if (!current) return nullptr;
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return current;
}

bool check_expectations(const mcp::JsonValue& response, const StepExpect& expect,
                        std::string& out_detail) {
    std::vector<std::string> missing_keys;
    for (const std::string& key : expect.has_keys) {
        if (!response.IsObject() || response.Find(key) == nullptr) {
            missing_keys.push_back(key);
        }
    }
    if (!missing_keys.empty()) {
        out_detail = "missing top-level key(s): " + join_keys(missing_keys);
        return false;
    }

    for (const StepExpect::FieldCheck& check : expect.field_checks) {
        const mcp::JsonValue* value = json_lookup(response, check.key);
        if (value == nullptr) {
            out_detail = "key '" + check.key + "' not found in response";
            return false;
        }
        if (check.has_value && !values_equal(check.value, *value)) {
            out_detail = "field '" + check.key + "': expected " + check.value.Dump() +
                         ", got " + value->Dump();
            return false;
        }
        if (check.not_empty && !is_not_empty(*value)) {
            out_detail = "field '" + check.key + "' is empty: " + value->Dump();
            return false;
        }
    }
    return true;
}

}  // namespace gda_test
