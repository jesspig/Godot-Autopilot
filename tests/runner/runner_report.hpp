#pragma once
#include "pipeline_executor.hpp"
#include <string>
#include <vector>

namespace gsd_test {

// stdout 汇总：每文件一行（name | steps 通过/总数 | 耗时 | 状态 PASS/FAIL/ERROR），
// 失败文件后跟失败步骤明细（step_id/tool/detail）；末尾总统计（文件数/通过/失败/总耗时）
void print_console_report(const std::vector<FileResult>& results);

// 输出 JSON 报告到 dir/report-<YYYYmmdd_HHMMSS>.json（Windows 用 std::put_time 或手拼）
// 结构：{"generated_at": "...", "total_files": N, "passed_files": N,
//        "files": [ {"name","passed","duration_ms","fatal_error","steps":[{...}]} ]}
// 目录不存在则创建（std::filesystem::create_directories）
// 返回保存的完整路径
std::string save_json_report(const std::vector<FileResult>& results, const std::string& dir);

}
