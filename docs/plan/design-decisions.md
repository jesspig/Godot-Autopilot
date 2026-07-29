# 设计决策

> **版本**: 0.1.0 | **更新**: 2026-07-29
>
> **摘要**: 本章记录了所有关键架构决策、选择理由、被舍弃的备选方案、安全边界和性能考量。
>
> **关联文档**:
> - [architecture-overview.md](architecture-overview.md) — 决策对应的系统架构
> - [thread-model.md](thread-model.md) — 线程模型决策的详细实现
> - [tool-discovery.md](tool-discovery.md) — 工具发现决策的详细设计

---

## 1. 关键决策表

| 决策项 | 选择 | 备选方案 | 理由 |
|--------|------|---------|------|
| **传输协议** | Streamable HTTP | stdio / SSE / WebSocket | 零桥接进程，直接嵌入编辑器；避免外部子进程管理 |
| **线程模型** | CommandQueue + `_process()` drain | 直接锁 / 信号量 / 原子操作 | Godot API 线程安全要求强制；`std::promise`/`std::future` 提供同步式编程 |
| **工具发现** | 3 层渐进式 | 全部直接注册 / 纯代码执行 | ~200 工具全量注册 ~50KB+ Token，分层发现将初始 Token 降到 ~2K |
| **执行模式** | 直接工具 + 代码执行混合 | 纯直接工具 / 纯 GDScript 执行 | 高频操作用直接工具（快且精确），长尾操作用代码执行（灵活且 Token 高效） |
| **序列化** | `mcp::JsonValue`（mcp-cpp-sdk 内置） | `nlohmann-json` / `simdjson` | 零额外依赖；mcp-cpp-sdk 内部使用 simdjson 加速解析 |
| **文本搜索** | BM25 | TF-IDF / 向量嵌入 / 全文索引 | 轻量无外部依赖；对工具名/描述检索足够准确 |
| **端口配置** | 环境变量 `GODOT_SELF_DRIVING_PORT` | 硬编码 / 配置文件 / 运行时对话框 | 零依赖运行时配置；环境变量在容器/CI 场景最通用 |
| **构建系统** | CMake 3.28+ + Ninja | MSBuild / SCons / Make | 跨平台一致体验；Ninja 增量编译速度最快 |
| **编译器** | clang-cl（首选） | MSVC / GCC | clang-cl 兼容 MSVC 但编译更快、错误信息更好；自动检测 |
| **依赖管理** | FetchContent | Git Submodules / vcpkg / Conan | 零额外工具安装；`_deps/` 缓存避免重复拉取 |
| **日志显示** | 编辑器内 LogDock | 文件日志 / 控制台 | AI 操作日志对用户可见；不用切出编辑器 |
| **调试信息** | `hlog_disable()` | libhv 默认日志 | 默认关闭 libhv 内部 HTTP 日志，减少干扰 |

## 2. 安全边界

### 2.1 网络层面

- 只监听 `127.0.0.1`（localhost），不接受外部网络连接
- 无身份验证（本地回环接口，信任本地客户端）
- 端口可配置（环境变量），不支持动态端口分配

### 2.2 编辑器层面

- 仅在 `MODULE_INITIALIZATION_LEVEL_EDITOR` 时启动服务器
- 运行时（Run Scene）下不自动加载（取决于 GDExtension 配置）
- 通过 `ModeDetector::is_editor()` 检测运行模式，某些工具限制仅在编辑器可用

### 2.3 执行安全

- `script_execute_gdscript` 可以执行任意 GDScript（设计使然）
- 无沙箱机制——工具直接调用 Godot API
- `resource_remove` / `resource_rename` 可修改文件系统

## 3. 性能考量

### 3.1 BM25 搜索性能

- 索引规模: ~200 文档
- 单次搜索: O(N * Q)，N=200, Q=查询词数
- 参数: `k1=1.5`, `b=0.75`
- 实测: < 1ms

### 3.2 构建性能

| 指标 | Debug | Release |
|------|-------|---------|
| 首次构建（含 FetchContent） | ~5-10 分钟 | ~5-10 分钟 |
| 增量构建（单文件修改） | ~5-15 秒 | ~10-30 秒 |
| 产物体积 | ~5-15MB DLL | ~2-5MB DLL |
| Unity Build | 是 | 是 |
| 并行度 | 主机 CPU 核心数 | 主机 CPU 核心数 |

## 4. 日志系统

### 4.1 级别与分类

| 级别 | 枚举 | 用途 |
|------|------|------|
| Debug | `LogLevel::Debug` | 详细调试信息（默认隐藏） |
| Info | `LogLevel::Info` | 关键节点信息 |
| Warning | `LogLevel::Warning` | 可恢复异常 |
| Error | `LogLevel::Error` | 协议/传输错误 |

| 分类 | 枚举 | 适用于 |
|------|------|--------|
| System | `LogCategory::System` | 插件启动/终止、模式检测 |
| Transport | `LogCategory::Transport` | 客户端连接/断开、MCP 请求 |
| Tools | `LogCategory::Tools` | 工具调用/完成 |
| Resources | `LogCategory::Resources` | Resource 注册/访问 |
| Prompts | `LogCategory::Prompts` | Prompt 注册/获取 |

### 4.2 关键日志点

| 位置 | 日志内容 | 级别 | 分类 |
|------|----------|------|------|
| `_enter_tree()` | `==== Godot Self-Driving plugin starting ====` | Info | System |
| `ServerContext` 构造 | `Server configured on port 9527` | Info | Transport |
| `ServerContext::start()` | `MCP server started on 127.0.0.1:9527` | Info | Transport |
| `register_all_tools()` | `206 tools registered via catalog` | Info | Tools |
| `call_tool` lambda | `physics_3d_ray_cast called via call_tool` | Info | Tools |
| 客户端连接 | `Client connected: Claude Desktop 1.0` | Info | Transport |
| 协议错误 | `Protocol error: ...` | Error | Transport |
