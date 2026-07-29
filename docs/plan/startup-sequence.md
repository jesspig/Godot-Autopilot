# 启动序列

> **版本**: 0.1.0 | **更新**: 2026-07-29
>
> **摘要**: GDExtension 插件的初始化分为两个 Godot 初始化级别：SCENE 阶段注册 UI 类，EDITOR 阶段创建 ServerContext、注册所有工具并启动 HTTP 服务器。终止时逆向销毁。
>
> **关联文档**:
> - [architecture-overview.md](architecture-overview.md) — 系统组件上下文
> - [thread-model.md](thread-model.md) — _process() drain 的触发机制

---

## 1. 初始化流程

```mermaid
sequenceDiagram
    participant Godot as Godot 编辑器
    participant Entry as GDExtension Entry Point
    participant Server as ServerContext
    participant MT as Meta-Tools
    participant DT as Domain Tools
    
    Godot->>Entry: MODULE_INITIALIZATION_LEVEL_SCENE
    Entry->>Entry: 注册 McpStatusBar 类
    
    Godot->>Entry: MODULE_INITIALIZATION_LEVEL_EDITOR
    Entry->>Server: new ServerContext(queue)
    Entry->>Server: start()
    
    Server->>Server: 解析端口（环境变量 GODOT_SELF_DRIVING_PORT / 默认 9527）
    Server->>Server: 创建 StreamableHttpServerTransport
    Server->>Server: 创建 McpServer
    
    Server->>Server: register_tools()
    Server->>MT: 注册 5 个元工具（ping/search_tools/list_categories/get_tool_detail/call_tool）
    Server->>DT: 注册 ~200 个 g_handlers 条目
    Server->>Server: 填充 ToolCatalog（~205 条目）
    Server->>Server: 构建 BM25 索引
    Server->>Server: 注册 8 个 Resources
    Server->>Server: 注册 5 个 Prompts
    
    Server->>Server: transport->Start()
    
    Entry->>Entry: 注册 McpLogDock + GodotSelfDrivingPlugin
    Entry->>Entry: EditorPlugins::add_by_type<Plugin>()
    
    Godot->>Plugin: _enter_tree()
    Plugin->>Plugin: 添加状态栏 + 日志面板
```

## 2. 初始化级别

| 级别 | 触发时机 | 执行内容 |
|------|----------|----------|
| `MODULE_INITIALIZATION_LEVEL_SCENE` | 场景类型系统就绪 | 注册 `McpStatusBar`（需要 HBoxContainer 基类） |
| `MODULE_INITIALIZATION_LEVEL_EDITOR` | 编辑器完全初始化 | 创建 `ServerContext`、注册所有工具、启动服务器 |

## 3. 终止序列

```mermaid
sequenceDiagram
    participant Godot as Godot 编辑器
    participant Entry as GDExtension Entry Point
    participant Server as ServerContext
    participant Plugin as EditorPlugin
    
    Godot->>Plugin: _exit_tree()
    Plugin->>Plugin: 移除 dock + status bar
    
    Godot->>Entry: MODULE_INITIALIZATION_LEVEL_EDITOR 终止
    Entry->>Server: delete g_server_ctx
    Server->>Server: stop()
    Server->>Server: server->Close()
    Server->>Server: transport->Close()
    
    Godot->>Entry: MODULE_INITIALIZATION_LEVEL_SCENE 终止
    Entry->>Entry: 无清理操作
```

## 4. 启动日志输出

```
[System Info] ==== Godot Self-Driving plugin starting ====
[System Info] Runtime mode: Editor
[Transport Info] Server configured on port 9527
[Transport Info] MCP server started on 127.0.0.1:9527
[Tools Info] 206 tools registered via catalog
[Resources Info] 8 resource handlers registered
[Prompts Info] 5 prompt templates registered
[System Debug] Toolbar status bar attached
[System Debug] Bottom log dock registered
[System Info] Plugin ready
```
