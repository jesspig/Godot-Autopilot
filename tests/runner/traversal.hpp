#pragma once
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

#include "pipeline_executor.hpp"

namespace gda_test {

struct TraversalStats {
    size_t total = 0;        // 工具总数（def 解析）
    size_t excluded = 0;     // 排除数（34 清单）
    size_t passed = 0;       // 通过（不崩溃 + 断言通过/警告容忍）
    size_t failed = 0;       // 失败
    std::vector<std::string> excluded_names;
    std::vector<std::string> warnings;   // 如"schema 必填但空参未报错"清单
};

// 解析 src/tools/*_tools.hpp（路径 PROJECT_ROOT/src/tools）：
// 遍历文件名以 _tools.hpp 结尾的 .hpp，逐行匹配 GDA_TOOL_CLASS(，
// 提取宏第 2 参工具名（ClassName 后第一个逗号之后的首个引号字段）；
// 失败（目录不存在/文件无法打开/无法解析/结果为空）抛 std::runtime_error
std::vector<std::string> parse_domain_tool_names();

// 执行遍历（mode: "empty_args" | "heuristic_smoke"）
// 每工具：get_tool_detail → 读 tool.side_effect，非空即排除（记入 excluded_names）
// → 前置校验 → 调用 → 崩溃由调用方检测
// 返回统计；out_steps 追加每个被调用工具的 StepResult（passed/detail）
TraversalStats run_traversal(McpTestClient& client, const std::string& mode,
                             std::vector<StepResult>& out_steps);

} // namespace gda_test
