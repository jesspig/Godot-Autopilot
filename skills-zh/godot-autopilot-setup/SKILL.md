---
name: godot-autopilot-setup
description: "从零安装并连接 Godot Autopilot（GDA）：识别操作系统、Godot 工程与已有 gda CLI，安装 CLI 与编辑器插件，安装英文或中文技能书，启动编辑器，用 gda status --json 验证，并根据 mcp_url 生成 MCP 客户端配置。适用于尚未安装 GDA、工程缺少 addons/godot-autopilot，或 CLI 找不到编辑器实例的场景。"
---

# Godot Autopilot 安装与连接

本册把一台机器从零带到可用的 GDA 连接。新机器或新工程先读本册；装好之后，
日常流程读 `godot-autopilot`，调用协议读 `godot-autopilot-tools`。

`gda` CLI 是一等接口。它从插件实例索引发现随机端口，通过 MCP 与插件通信，
并给出 MCP 客户端唯一需要的地址：`gda status --json` 里的 `mcp_url`。不要
硬编码端口，也不要手写 MCP 地址。

## 1. 安装前检查

先收集事实，后续步骤按结果分支：

```bash
gda --version
godot --version
test -f project.godot && echo "project ok"
```

PowerShell 使用 `Get-Command gda`、`Get-Command godot` 与
`Test-Path project.godot`。记录操作系统与 CPU 架构（Windows x64、macOS
arm64/x64、Linux x64/glibc、Linux x64/musl）。每个目标对应一个独立二进制，
架构决定下载哪个资产。

如果 `gda --version` 已可用，跳到第 3 步。如果 Godot 不在 `PATH`，记下编辑器
可执行文件路径，启动时用 `--godot` 显式传入。

## 2. 安装 CLI

使用发行页的独立 `gda` 二进制，不需要 JavaScript 运行时。下载与系统和架构
匹配的资产，先用发行包的 `sha256sums.txt` 校验，再放进 `PATH`：

```bash
sha256sum -c sha256sums.txt --ignore-missing
chmod +x gda
mv gda ~/.local/bin/gda
gda --version
```

macOS 只有在校验通过后才清除隔离属性：
`xattr -d com.apple.quarantine ./gda`。Windows 若被 SmartScreen 标记，在文件
属性里解除锁定。

源码检出可以直接用 Bun 运行 CLI：

```bash
bun install
bun cli/bin/gda.ts --version
```

不要在工程脚本里钉 Git ref 或包版本。CLI 与插件通过 `gda update all` 一起
更新；只在一处钉版本会和另一处漂移。

## 3. 安装编辑器插件

在 Godot 工程根目录（含 `project.godot` 的目录）运行：

```bash
gda install plugin --project .
```

安装器写入 `addons/godot-autopilot`，停止该工程正在运行的编辑器，并在原先
有编辑器运行时重启它。离线且已有缓存时用 `--source cache`。内置源不包含
插件归档；不可用时改用 `online` 或 `cache`。

验证安装：

```bash
test -f addons/godot-autopilot/godot-autopilot.gdextension && echo "plugin ok"
```

## 4. 启动编辑器并验证连接

```bash
gda start --project .
gda list
gda status --json
gda ping
```

`gda start` 启动编辑器并等待插件写入实例记录。`gda list` 显示实例 id、工程、
端口、传输方式与存活状态。`gda status --json` 是权威连接记录。客户端关心的
字段：

```json
{
  "ok": true,
  "project": "/path/to/project",
  "port": 21437,
  "port_source": "persisted",
  "mcp_url": "http://127.0.0.1:21437/mcp",
  "transport": "ipc",
  "ipc_kind": "unix",
  "plugin_version": "<plugin version>",
  "toolset_hash": "1a2b3c4d"
}
```

端口由插件选择并持久化，不由 CLI 选择。不要从文档或别的工程复制端口。每次都
从该输出读取 `mcp_url`；编辑器重启后它仍然正确。

`gda ping` 确认 MCP 握手。失败时先运行 `gda list`：没有存活实例说明编辑器未
打开、插件未启用，或工程路径不对。

## 5. 安装技能书

技能书与插件相互独立，可单独更新：

```bash
gda install skills
gda install skills --zh
gda install skills --offline
gda skills list
```

默认目标为 `~/.agents/skills`。`--path <dir>` 可装到工程内目录。`--zh` 选择
中文技能书；`--offline` 不访问网络，使用缓存或内置归档。在已有安装上切换
语言需要 `--force`，因为册名相同而内容不同。`gda skills list` 列出已安装
册与文件数；`gda skills show <name>` 无需运行编辑器即可打印一册。

## 6. 生成 MCP 客户端配置（可选）

有 shell 时优先通过 CLI 使用 GDA。需要让客户端同时看到原生 MCP 工具时，从
实时状态生成配置，不要手改模板：

1. 运行 `gda status --json`，读取 `mcp_url`。
2. 把该地址写入客户端的 MCP server 条目。streamable HTTP 形如：

```json
{
  "mcpServers": {
    "godot-autopilot": {
      "url": "http://127.0.0.1:21437/mcp"
    }
  }
}
```

3. 重启或重连客户端，确认 GDA 工具出现。
4. 编辑器重启后工具消失时，重新读取 `mcp_url` 并重连；端口可能变化，但发现
   命令总能给出当前值。

服务只监听环回地址。不要把端口转发、代理或发布到网络。没有“安装 MCP 配置”
的命令；配置由 Agent 根据 `mcp_url` 生成，因为只有 Agent 知道使用哪个客户端。

## 7. 首次端到端检查

```bash
gda tools search "scene"
gda tools describe create_scene_node
gda call ping
gda call system_status
```

`gda tools search` 读取缓存目录，没有编辑器也能用。`gda tools describe` 在首次
写入前显示输入 schema 与 `side_effect` 标记。`gda call` 对实时编辑器执行并
输出结果 JSON。每次响应都读 `new_errors_since_last_call` 水位；为 0 表示没有
记录到新的引擎错误。

## 故障排查

| 现象 | 检查 | 处理 |
|---|---|---|
| 找不到 `gda` | `gda --version` | 安装独立二进制并把目录加入 `PATH`。 |
| 没有实例 | `gda list --all` | 打开工程，或运行 `gda start --project .`。 |
| 插件缺失 | `addons/godot-autopilot` 是否存在 | 运行 `gda install plugin --project .`。 |
| 插件未启用 | 编辑器插件对话框 | 启用 Godot Autopilot 并重启编辑器。 |
| 连接被拒 | `gda status --json` | 重新读取 `mcp_url`；不要复用旧端口。 |
| 技能书缺失 | `gda skills list` | 运行 `gda install skills` 或 `--zh`；断网时加 `--offline`。 |
| 确认错误 | 响应中的 `code` | 核对目标后加 `--confirm` 重试。 |
| 离线更新 | `gda cache list` | 用 `--source cache`；内置源只带技能书。 |

## 安全规则

- MCP 端点只监听环回地址且没有认证，必须保持在本地。
- 高风险调用（`destructive`、`process`、`code_execute`、`game_runtime`）返回
  `confirmation_required`；核对工具说明与目标路径后才传 `--confirm`。
- 写入后优先读回，而不是假设成功。`ok` 只表示调用送达，不表示效果符合意图。

## 参见

- `godot-autopilot` - 日常入口：连接、路由、观察循环。
- `godot-autopilot-tools` - CLI 命令参考、发现、确认、错误协议。
- `godot-autopilot-scene-system` - 场景、节点、属性、信号。
- `godot-autopilot-resources` - 资源、导入、重命名与移动。
- `godot-autopilot-scripting` - GDScript 创建与执行通道。
- `godot-autopilot-runtime` - 运行与调试游戏。
- `godot-autopilot-servers` - RenderingServer、PhysicsServer、NavigationServer。
- `godot-autopilot-content` - 瓦片、动画、音频、UI 主题。
- `godot-autopilot-csharp` - C# 工程、构建循环、反射桥。
