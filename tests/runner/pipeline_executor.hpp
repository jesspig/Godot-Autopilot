#pragma once
#include "godot_process.hpp"
#include "config_loader.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace gda_test {

class McpTestClient;

struct StepResult {
    std::string step_id;
    std::string tool;              // 实际调用的工具（遍历步骤填 "<traverse:empty_args>" 样式）
    bool passed = false;
    std::string detail;            // 失败说明或成功摘要
    double duration_ms = 0.0;
};

struct FileResult {
    std::string name;
    std::vector<StepResult> steps;
    bool passed = false;
    double duration_ms = 0.0;
    std::string fatal_error;       // 崩溃/前置失败等致命错误描述（空=无）
    size_t call_count = 0;
    size_t call_success = 0;
};

// 最小闭环：进程已由调用方启动（proc 非空且 proc->alive()==true 前提，若已死亡视为致命错误）
// proc 为 nullptr 表示外部 MCP 模式（--no-auto）：跳过进程存活检查与崩溃检测，
// 此时 external_client 必须非空
// external_client 为 nullptr 时内部创建 McpTestClient(proc->port()) 并 connect
//（connect 失败 → fatal_error 返回）；非 nullptr 时复用（--no-auto 模式）
FileResult run_pipeline(const TestCase& tc, GodotProcess* proc,
                        McpTestClient* external_client = nullptr);

}
