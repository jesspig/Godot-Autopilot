#pragma once
#include <chrono>
#include <memory>
#include <string>

namespace gda_test {

class GodotProcess {
public:
    // pimpl 前置声明须公开：cpp 中定义 struct GodotProcess::Impl
    // 与自由函数 close_process/append_log 以 GodotProcess::Impl& 引用，
    // private 区声明会导致类外不可见
    struct Impl;

    struct Options {
        std::string godot_path;      // Godot 可执行文件路径（必填）
        std::string project_path;    // Example 项目绝对路径
        int port = 0;                // MCP 端口；0 = 随机空闲端口
        bool headless = true;
    };

    explicit GodotProcess(Options opts);
    ~GodotProcess();

    // 启动：含首次 --headless --editor --import 幂等同步执行（失败不致命，记录日志继续）
    // + 常驻 --headless --editor --path <project> 启动（注入 GODOT_AUTOPILOT_PORT）
    // + 就绪轮询（TCP 探测 + MCP initialize 握手，200ms 间隔，ready_timeout 上限）
    // 返回 true=就绪；false=失败（last_error() 含捕获的编辑器日志摘要）
    bool start(std::chrono::seconds ready_timeout = std::chrono::seconds(90));

    // 优雅终止（taskkill 软杀）→ 5s 超时 → TerminateProcess 兜底
    void stop();

    bool alive() const;              // 进程是否存活
    int port() const;                // 实际端口
    std::string capture_logs();      // 取回 stdout+stderr 合并日志并清空缓冲
    std::string last_error() const;  // 最近一次失败的说明

    // 解析 Godot 路径：进程环境变量 GODOT_PATH → 仓库根 .env 的 GODOT_PATH → 空串
    static std::string resolve_godot_path();

private:
    std::unique_ptr<Impl> impl_;
};

}
