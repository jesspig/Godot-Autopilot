#include "config_loader.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace gda_test {

namespace {

[[noreturn]] void Fail(const std::string& path, const std::string& msg) {
    throw std::runtime_error(path + ": " + msg);
}

std::string TypeName(const mcp::JsonValue& v) {
    if (v.IsNull())   return "null";
    if (v.IsBool())   return "bool";
    if (v.IsInt())    return "integer";
    if (v.IsDouble()) return "number";
    if (v.IsString()) return "string";
    if (v.IsArray())  return "array";
    if (v.IsObject()) return "object";
    return "unknown";
}

void RequireType(const mcp::JsonValue& v, const std::string& path, const std::string& expected) {
    if (!v.IsObject() && expected == "object")  return Fail(path, "必须是 " + expected + "，实际是 " + TypeName(v));
    if (!v.IsArray() && expected == "array")    return Fail(path, "必须是 " + expected + "，实际是 " + TypeName(v));
    if (!v.IsString() && expected == "string")  return Fail(path, "必须是 " + expected + "，实际是 " + TypeName(v));
    if (!v.IsBool() && expected == "bool")      return Fail(path, "必须是 " + expected + "，实际是 " + TypeName(v));
}

std::string ReadFile(const std::string& json_path) {
    std::ifstream in(json_path, std::ios::binary);
    if (!in) {
        throw std::runtime_error(json_path + ": 无法打开测试用例文件");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string ParseOptionalId(const mcp::JsonValue& v, const std::string& path) {
    const mcp::JsonValue* id = v.Find("id");
    if (!id) return "";
    RequireType(*id, path + ".id", "string");
    return id->GetString();
}

StepExpect ParseExpect(const mcp::JsonValue& v, const std::string& path) {
    StepExpect exp;
    if (const mcp::JsonValue* hk = v.Find("has_keys")) {
        RequireType(*hk, path + ".has_keys", "array");
        const mcp::JsonValue::Array& arr = hk->GetArray();
        for (size_t i = 0; i < arr.size(); ++i) {
            const std::string item_path = path + ".has_keys[" + std::to_string(i) + "]";
            RequireType(arr[i], item_path, "string");
            exp.has_keys.push_back(arr[i].GetString());
        }
    }
    if (const mcp::JsonValue* fc = v.Find("field_checks")) {
        RequireType(*fc, path + ".field_checks", "array");
        const mcp::JsonValue::Array& arr = fc->GetArray();
        for (size_t i = 0; i < arr.size(); ++i) {
            const std::string item_path = path + ".field_checks[" + std::to_string(i) + "]";
            RequireType(arr[i], item_path, "object");
            StepExpect::FieldCheck check;
            const mcp::JsonValue* key = arr[i].Find("key");
            if (!key) Fail(item_path, "缺少必填字段 key");
            RequireType(*key, item_path + ".key", "string");
            check.key = key->GetString();
            if (const mcp::JsonValue* val = arr[i].Find("value")) {
                check.has_value = true;
                check.value = *val;
            }
            if (const mcp::JsonValue* ne = arr[i].Find("not_empty")) {
                RequireType(*ne, item_path + ".not_empty", "bool");
                check.not_empty = ne->GetBool();
            }
            exp.field_checks.push_back(std::move(check));
        }
    }
    return exp;
}

Step ParseToolStep(const mcp::JsonValue& v, const std::string& path) {
    Step step;
    const std::string id = ParseOptionalId(v, path);
    const mcp::JsonValue* tool = v.Find("tool");
    if (!tool) Fail(path, "缺少必填字段 tool");
    RequireType(*tool, path + ".tool", "string");
    step.tool = tool->GetString();
    step.id = id.empty() ? step.tool : id;
    if (const mcp::JsonValue* args = v.Find("args")) {
        RequireType(*args, path + ".args", "object");
        step.args = *args;
    } else {
        step.args = mcp::JsonValue(mcp::JsonValue::object_tag);
    }
    if (const mcp::JsonValue* expect = v.Find("expect")) {
        RequireType(*expect, path + ".expect", "object");
        step.expect = ParseExpect(*expect, path + ".expect");
    }
    return step;
}

Step ParseTraverseStep(const mcp::JsonValue& v, const std::string& path, size_t index) {
    Step step;
    const std::string id = ParseOptionalId(v, path);
    step.id = id.empty() ? ("step_" + std::to_string(index)) : id;
    const mcp::JsonValue* trav = v.Find("traverse");
    RequireType(*trav, path + ".traverse", "object");
    const mcp::JsonValue* kind = trav->Find("kind");
    if (!kind) Fail(path + ".traverse.kind", "缺少必填字段 kind");
    RequireType(*kind, path + ".traverse.kind", "string");
    if (kind->GetString() != "domain_tools") {
        Fail(path + ".traverse.kind", "仅支持 \"domain_tools\"，实际是 \"" + kind->GetString() + "\"");
    }
    step.traverse_kind = kind->GetString();
    const mcp::JsonValue* mode = trav->Find("mode");
    if (mode) {
        RequireType(*mode, path + ".traverse.mode", "string");
        const std::string& m = mode->GetString();
        if (m != "empty_args" && m != "heuristic_smoke") {
            Fail(path + ".traverse.mode", "仅支持 \"empty_args\" 或 \"heuristic_smoke\"，实际是 \"" + m + "\"");
        }
        step.traverse_mode = m;
    } else {
        step.traverse_mode = "empty_args";
    }
    return step;
}

Step ParseStageStep(const mcp::JsonValue& v, const std::string& path, size_t index) {
    if (!v.IsObject()) Fail(path, "必须是 object");
    const bool has_tool = v.Contains("tool");
    const bool has_traverse = v.Contains("traverse");
    if (has_tool == has_traverse) {
        Fail(path, has_tool ? "步骤不能同时包含 tool 和 traverse" : "步骤必须包含 tool 或 traverse 之一");
    }
    if (has_tool) return ParseToolStep(v, path);
    return ParseTraverseStep(v, path, index);
}

Step ParsePlainStep(const mcp::JsonValue& v, const std::string& path) {
    if (!v.IsObject()) Fail(path, "必须是 object");
    if (v.Contains("traverse")) Fail(path + ".traverse", "before_all/after_all 步骤不支持 traverse");
    if (v.Contains("expect"))   Fail(path + ".expect", "before_all/after_all 步骤不支持 expect");
    return ParseToolStep(v, path);
}

std::vector<Step> ParseStepsArray(const mcp::JsonValue& arr, const std::string& path, bool allow_traverse) {
    std::vector<Step> out;
    const mcp::JsonValue::Array& elems = arr.GetArray();
    for (size_t i = 0; i < elems.size(); ++i) {
        const std::string item_path = path + "[" + std::to_string(i) + "]";
        out.push_back(allow_traverse ? ParseStageStep(elems[i], item_path, i)
                                     : ParsePlainStep(elems[i], item_path));
    }
    return out;
}

} // namespace

TestCase load_test_case(const std::string& json_path) {
    const std::string text = ReadFile(json_path);
    mcp::JsonValue root;
    try {
        root = mcp::JsonValue::Parse(text);
    } catch (const std::runtime_error&) {
        throw std::runtime_error(json_path + ": JSON 语法错误");
    }
    if (!root.IsObject()) Fail("", "顶层必须是 object");

    TestCase tc;
    const mcp::JsonValue* name = root.Find("name");
    if (!name) Fail("name", "缺少必填字段");
    RequireType(*name, "name", "string");
    tc.name = name->GetString();

    if (const mcp::JsonValue* desc = root.Find("description")) {
        RequireType(*desc, "description", "string");
        tc.description = desc->GetString();
    }
    if (const mcp::JsonValue* hl = root.Find("headless")) {
        RequireType(*hl, "headless", "bool");
        tc.headless = hl->GetBool();
    }

    const mcp::JsonValue* pipe = root.Find("pipeline");
    if (!pipe) Fail("pipeline", "缺少必填字段");
    RequireType(*pipe, "pipeline", "object");

    if (const mcp::JsonValue* of = pipe->Find("on_failure")) {
        RequireType(*of, "pipeline.on_failure", "string");
        const std::string& s = of->GetString();
        if (s != "fail_fast" && s != "continue") {
            Fail("pipeline.on_failure", "仅支持 \"fail_fast\" 或 \"continue\"，实际是 \"" + s + "\"");
        }
        tc.on_failure = s;
    } else {
        tc.on_failure = "fail_fast";
    }

    if (const mcp::JsonValue* ba = pipe->Find("before_all")) {
        RequireType(*ba, "pipeline.before_all", "array");
        tc.before_all = ParseStepsArray(*ba, "pipeline.before_all", false);
    }
    if (const mcp::JsonValue* stages = pipe->Find("stages")) {
        RequireType(*stages, "pipeline.stages", "array");
        const mcp::JsonValue::Array& stage_list = stages->GetArray();
        for (size_t i = 0; i < stage_list.size(); ++i) {
            const std::string stage_path = "pipeline.stages[" + std::to_string(i) + "]";
            const mcp::JsonValue& stage = stage_list[i];
            RequireType(stage, stage_path, "object");
            if (const mcp::JsonValue* sid = stage.Find("id")) {
                RequireType(*sid, stage_path + ".id", "string");
            }
            const mcp::JsonValue* steps = stage.Find("steps");
            if (!steps) Fail(stage_path, "缺少必填字段 steps");
            RequireType(*steps, stage_path + ".steps", "array");
            std::vector<Step> parsed = ParseStepsArray(*steps, stage_path + ".steps", true);
            tc.steps.insert(tc.steps.end(),
                            std::make_move_iterator(parsed.begin()),
                            std::make_move_iterator(parsed.end()));
        }
    }
    if (const mcp::JsonValue* aa = pipe->Find("after_all")) {
        RequireType(*aa, "pipeline.after_all", "array");
        tc.after_all = ParseStepsArray(*aa, "pipeline.after_all", false);
    }
    return tc;
}

} // namespace gda_test
