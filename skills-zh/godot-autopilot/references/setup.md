# 安装与连接

GDA 未安装、项目没有 `addons/godot-autopilot`，或 CLI 找不到编辑器实例时读本篇。`gda` CLI 是一等接口；端口由插件拥有。

## 检查机器

安装前先收集三件事：

```bash
gda --version
godot --version
test -f project.godot && echo "project ok"
```

记录操作系统与 CPU 架构。发布包为每个目标提供独立 `gda` 二进制，架构决定资产名。`gda --version` 可用则跳过 CLI 安装。Godot 不在 `PATH` 时，找到编辑器并在启动时传 `--godot`。

## 安装 CLI

使用发布页的独立二进制，并用 `sha256sums.txt` 校验后再放进 `PATH`。macOS 只在校验通过后清除 quarantine 属性；Windows 若被 SmartScreen 标记，先解除锁定。

源码检出可直接运行：

```bash
bun install
bun cli/bin/gda.ts --version
```

禁止在项目脚本里钉 Git ref 或包版本。用 `gda update all` 一起更新 CLI 与插件；一处钉版本必然与另一处漂移。

## 安装编辑器插件

在包含 `project.godot` 的目录执行：

```bash
gda install plugin --project .
```

安装器写入 `addons/godot-autopilot`，停止该项目的运行中编辑器，并在原先有实例时重启。无网络且存在缓存包时用 `--source cache`；内置源不包含插件包。验证：

```bash
test -f addons/godot-autopilot/godot-autopilot.gdextension && echo "plugin ok"
```

## 启动并验证连接

```bash
gda start --project .
gda list
gda status --json
gda ping
```

`gda start` 等待插件的实例记录。`gda list` 显示实例 id、项目、端口、通道与存活状态。`gda status --json` 是权威连接记录：

- `mcp_url` 是 MCP 客户端唯一需要的端点。
- `transport` 与 `ipc_kind` 说明本地通道。
- `port_source` 解释端口来源。
- `plugin_version` 标识运行中的插件。

端口由插件选择并持久化。禁止从文档或其它项目复制端口，也禁止跨编辑器重启复用端口。`gda ping` 确认 MCP 握手；失败时先跑 `gda list`。没有 live 实例表示编辑器未开、插件未启用或项目路径不对。

## 安装技能卡

技能与插件独立，单独更新：

```bash
gda install skills
gda install skills --zh
gda install skills --offline
gda install skills --path "<project>/.agents/skills"
gda install skills --prune
```

默认目标为 `~/.agents/skills`。`--zh` 选中文内容；`--offline` 使用缓存或内置包。已有安装在语言间切换需要 `--force`，因为目录名相同而内容不同。`--prune` 删除目标目录中的退役技能目录；不加时只报告 `stale`，不删除任何内容。`gda skills list` 与 `gda skills show <name>` 无需运行中的编辑器。

## MCP 客户端配置

客户端需要原生 MCP 工具时，从 `mcp_url` 生成条目：

```json
{
  "mcpServers": {
    "godot-autopilot": {
      "url": "<mcp_url>"
    }
  }
}
```

每次编辑器重启后重新读取 `mcp_url` 并重连。服务只绑环回地址；禁止转发、代理或公开端口。

## 首次端到端检查

```bash
gda search scene
gda describe create_scene_node
gda call ping
gda call system_status
gda call get_editor_state --args '{"action": "info"}'
```

`gda search` 需要 live 编辑器。`gda describe` 在首次写入前显示 schema 与 `side_effect` 标记。每次响应都读 `new_errors_since_last_call`；为零表示没有记录新引擎错误。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 找不到 `gda` | 二进制不在 `PATH` | 安装独立二进制并重开终端 |
| 没有实例 | 编辑器未开、插件未启用或项目不对 | `gda list --all`，再打开项目或 `gda start --project .` |
| 插件缺失 | 没有 `addons/godot-autopilot` | `gda install plugin --project .` |
| 连接被拒 | 端口过期或编辑器已重启 | 重读 `mcp_url`；不要复用旧端口 |
| 技能缺失 | 未安装或语言不对 | `gda skills list`，再按需 `gda install skills --zh` |
| 退役目录残留 | 旧安装未清理 | `gda install skills --prune` |
