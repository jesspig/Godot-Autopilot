# Godot-Autopilot

> **Godot 引擎的 MCP 服务端 — 在原生 API 层面实现 AI 对引擎的全面控制**

[English Version](README.md)

Godot-Autopilot 是一个基于 [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) 协议的服务端，它在 **原生引擎 API 层面** 将 AI Agent 与 Godot 引擎连接。与传统工具从"用户 UI 视角"出发（模拟点击或编辑器操作）不同，本项目赋予 AI Agent 对 Godot 整个引擎表面的直接、程序化访问能力 —— 场景树操作、物理服务器、渲染服务器、音频、导航、输入模拟、脚本执行等。

这是一个**进程内 GDExtension 插件**，直接加载到 Godot 编辑器中。无需独立的桥接进程 —— MCP 服务端随项目打开而启动，随项目关闭而停止。

## 架构

```
MCP 主机 (Claude Desktop, Cursor 等)
  │ POST http://127.0.0.1:9527/mcp
  ▼
Godot 编辑器
  └── Godot-Autopilot (GDExtension)
      ├── libhv (内部 HTTP 线程)
      ├── mcp-cpp-sdk: McpServer + Streamable HTTP
      ├── 命令队列 (libhv → Godot 主线程桥接)
      ├── ~339 个 MCP 工具，覆盖 23 个类别（数量随插件版本变化，以 MCP search_tools 返回为准）
      ├── 内置文档 (离线引擎 API 文档)
      └── 自定义日志面板 (专属插件输出面板)
```

### 关键设计决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| **传输协议** | Streamable HTTP (POST /mcp) | MCP 标准协议，无桥接进程 |
| **线程模型** | 命令队列 + 帧同步回调 | 安全访问 Godot 主线程独占 API |
| **端口** | 9527 | 可通过 `GODOT_AUTOPILOT_PORT` 环境变量配置 |
| **发现机制** | 三层渐进式 (目录→检视→执行) | ~339 个工具场景下节省上下文窗口（数量随插件版本变化） |
| **搜索** | BM25 关键词 | 工具按命名空间 + 描述组织 |
| **构建** | CMake 3.28+ / C++17 | 跨平台，自动优化构建 |

## 功能特性

### 🎮 完整引擎控制 (~339 工具，数量随插件版本变化，以 MCP search_tools 返回为准)

| 类别 | 数量 | 说明 |
|------|:----:|------|
| **渲染** | 49 | 画布项、摄像机、灯光、网格、视口、材质 |
| **物理** | 47 | 2D/3D 射线检测、物体创建、施力、关节 |
| **显示** | 24 | 窗口、视口和屏幕属性 |
| **编辑器** | 22 | 选中、撤销/重做、场景保存、插件管理 |
| **资源** | 21 | 加载、保存、创建、列出资源 |
| **音频** | 20 | 总线管理、音频流播放、效果 |
| **输入** | 19 | 按键/鼠标/手柄模拟、操作查询（含 InputMap） |
| **OS** | 16 | 操作系统、环境变量与剪贴板访问 |
| **调试** | 16 | 性能监视器、性能分析、诊断 |
| **导航** | 15 | 导航网格、路径查询、代理 |
| **配置** | 13 | 项目设置、引擎属性 |
| **场景** | 12 | 节点创建、删除、场景树检查（如 `create_scene_node`） |
| **脚本** | 10 | 执行 GDScript 和 C#，调用任意节点方法 |
| **文本** | 10 | 字符串处理、解析与格式化 |
| **TileMap** | 7 | 瓦片地图创建、格子操作与查询 |
| **调试器** | 7 | 调试器会话控制与检视 |
| **游戏** | 7 | 游戏循环控制与引擎级状态 |
| **属性** | 5 | 读写属性、列出属性、连接信号 |
| **文档** | 4 | 查询离线 Godot API 文档 |
| **分组** | 3 | 节点分组管理与成员查询 |
| **SpriteFrames** | 3 | 精灵帧集创建与动画管理 |
| **系统** | 1 | 插件级系统信息 |
| **截图** | 1 | 编辑器视口截图 |

### 📖 内联 API 文档

通过 MCP 工具直接查询 Godot 的内置离线文档。无需网络搜索 —— 每个类、方法、属性、信号的文档都来自引擎自身的 `DocTools` 缓存：

- `get_docs_class` — 完整类文档（描述、方法、属性、信号）
- `find_docs_class` — 按名称或关键词搜索类
- `get_docs_method` — 方法签名与说明
- `get_docs_property` — 属性类型与说明

### 📋 MCP 资源

服务端将引擎状态暴露为可读的 MCP Resources：

```
godot://engine/version              — 引擎版本信息
godot://scene/tree                  — 当前场景节点树 (JSON)
godot://scene/{path}                — 按路径查看节点属性
godot://filesystem/tree             — 项目文件系统结构
godot://filesystem/{path}           — 文件/目录内容
godot://editor/selection            — 当前选中
godot://editor/settings/{key}       — 编辑器设置
```

### 📝 专属日志面板

自定义 `EditorDock` 底部面板，仅显示插件日志（系统、工具、传输层、资源、提示词），支持：

- 级别过滤 (Debug / Info / Warning / Error)
- 分类过滤
- 文本搜索
- 折叠重复消息
- 主题一致样式（匹配 Godot 编辑器主题）

## 快速开始

### 前提条件

- CMake 3.28+
- C++17 编译器（推荐 Clang，支持 MSVC/GCC）
- Godot 4.3+（支持 GDExtension）

### 构建

```bash
git clone https://github.com/jesspig/Godot-Autopilot.git
cd Godot-Autopilot

# 推荐：一步完成构建 + 部署到 Example/addons/
uv run build.py             # Debug
uv run build.py --release   # Release（先清理）

# 或手动 CMake（presets: debug、release，均使用 Ninja）
cmake --preset release && cmake --build --preset release
```

编译产物 `.dll` / `.so` / `.dylib` 位于 `build/release/`。`build.py` 同时生成 `.gdextension` 文件并将产物复制到 `Example/addons/godot-autopilot/`。

### 安装

复制到你的 Godot 项目：

```
your-project/
└── addons/
    └── godot-autopilot/
        ├── godot-autopilot.dll      (或 .so / .dylib)
        └── godot-autopilot.gdextension
```

### 配置 MCP 主机

```json
{
  "mcpServers": {
    "godot-engine": {
      "type": "streamable-http",
      "url": "http://127.0.0.1:9527/mcp"
    }
  }
}
```

打开你的 Godot 项目 —— 服务端自动启动。端口号显示在编辑器状态栏中。

## 技术栈

| 层次 | 技术 |
|------|------|
| **引擎** | Godot 4.x (GDExtension) |
| **绑定层** | godot-cpp (FetchContent) |
| **MCP 协议** | [modelcontextprotocol-cpp-sdk](https://github.com/jesspig/modelcontextprotocol-cpp-sdk) |
| **HTTP / 异步** | libhv (内部) |
| **JSON** | mcp::JsonValue (SDK 内置) |
| **构建** | CMake 3.28+ / C++17 |
| **优化** | Clang 优先, ThinLTO, Ninja, sccache, Unity Build |

## 素材声明

`Example/` 测试项目使用了 [Pixel Adventure 1](https://pixelfrog-assets.itch.io/pixel-adventure-1) 的像素艺术素材，版权归 Pixel Frog 所有。

## 许可证

MIT
