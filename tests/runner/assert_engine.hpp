#pragma once
#include "config_loader.hpp"   // StepExpect 定义于 T1.2 的 config_loader.hpp（同目录，直接 include）
#include <mcp/JsonValue.hpp>
#include <string>

namespace gda_test {

// 点路径取值："result.position.x"；不支持数组下标（仅对象点路径）
// 路径任一段不存在返回 nullptr
const mcp::JsonValue* json_lookup(const mcp::JsonValue& root, const std::string& dotted_path);

// 对响应 JSON 执行断言；通过返回 true；失败返回 false 且 out_detail 含
// 可读的期望/实际说明（含 step 上下文由调用方拼接）
bool check_expectations(const mcp::JsonValue& response, const StepExpect& expect,
                        std::string& out_detail);

}
