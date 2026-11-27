# Godot Autopilot

[![许可证](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![最新版本](https://img.shields.io/github/v/release/jesspig/Godot-Autopilot)](https://github.com/jesspig/Godot-Autopilot/releases)
[![构建状态](https://github.com/jesspig/Godot-Autopilot/actions/workflows/ci.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions)
[![发布工作流](https://github.com/jesspig/Godot-Autopilot/actions/workflows/release.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions/workflows/release.yml)
![Godot版本](https://img.shields.io/badge/Godot-4.7%2B-478CBF?logo=godotengine&logoColor=white)
![支持平台](https://img.shields.io/badge/platform-Windows%7CLinux%7CmacOS-lightgrey)
[![CLI](https://img.shields.io/badge/CLI-gda-原生-blue)](docs/wiki/modules/cli.md)
![工具数](https://img.shields.io/badge/tools-253-brightgreen)

> **让 AI 代理在编辑器里帮你搭场景、试玩和排错。**

[English](README.md) | **简体中文** | [繁體中文](README.zh-TW.md) | [日本語](README.ja.md) | [한국어](README.ko.md)

Godot Autopilot（GDA）是住在 **Godot 编辑器内部**的插件，通过本机单一 `gda` CLI 暴露运行中的工程。没有需要配置的协议或端点：AI 编程助手（Claude Code、Cursor、Codex、OpenCode 等）像你一样通过 CLI 命令操作项目：搭场景、调属性、运行游戏、点按钮、截图、看日志，再根据结果继续迭代。不需要桥接进程，也不用把报错复制到聊天框。图形界面编辑器打开项目时插件自动登记实例，关闭项目时自动撤销。

![Demo 001](assets/demo-001.gif)

**253 个工具**覆盖从编辑器到游戏的完整闭环：247 个域工具 + 5 个元工具 + `system_status`（数量随版本增长，以 `gda tool list` 查到的为准）。325 条旧工具名别名在 0.6.0 前可用 `gda tool describe` 解析、用 `gda tool call` 调用（与实名工具重名的条目按实名工具优先）。技能书为 **1 张入口卡 + 9 篇 references**，教代理按 CLI 调用协议执行。参考：[工具面](docs/wiki/tool_surface_baseline.md)、[技能](docs/wiki/modules/skills.md)、[CLI](docs/wiki/modules/cli.md)。

## 快速开始

需要 **Godot 4.7+**，并使用图形界面编辑器。

1. **安装**——在 Godot 工程根目录运行一键脚本；手动安装与源码构建见 [INSTALL.md](INSTALL.md)。
   - macOS/Linux：

     ```bash
     curl -fsSL https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.sh \
       | bash -s -- --project "$PWD" --skills
     ```

   - Windows PowerShell：

     ```powershell
     & ([scriptblock]::Create((irm https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.ps1))) `
       -Project (Get-Location).Path -Skills
     ```

   脚本会安装 `gda` CLI、识别 `project.godot`、安装插件，并按需安装技能书。
2. **打开项目**——图形界面编辑器会自动向 CLI 登记实例，首次启动时绑定一个空闲的环回端口（按工程持久化，取自 `20000–29999`）。无头/命令行编辑器模式不会提供实例。
3. **通过 CLI 连接**——运行 `gda ping` 与 `gda status --format json`，JSON 连接记录确认实例在线及其本机端点。无需客户端配置：一切操作都是输出 JSON 信封的 `gda` 命令。Claude Code、Codex、Cursor、OpenCode、Copilot 都可直接驱动 CLI。
4. **提需求**——比如“创建一个能跑能跳的玩家角色，试玩一下并修掉看着不对的地方”。

## 安全说明

CLI 只连接本机回环实例（端口由插件按工程选择，由 CLI 自动寻址），非本机地址一律拒绝。高风险工具（`destructive`、`process`、`code_execute`、`game_runtime`）需要 `--confirm`，没有全局白名单。详见 [docs/wiki/security_contract.md](docs/wiki/security_contract.md)。

## 文档

- [INSTALL.md](INSTALL.md)——一键脚本、Release 手动安装、源码构建与代理安装说明。
- [docs/wiki/index.md](docs/wiki/index.md)——项目知识库：架构、模块、测试与约定。
- [docs/wiki/tool_surface_baseline.md](docs/wiki/tool_surface_baseline.md)——工具面、别名、schema 与能力矩阵基线。
- [docs/wiki/build.md](docs/wiki/build.md)——构建、部署与打包。
- [docs/wiki/modules/cli.md](docs/wiki/modules/cli.md)——完整 CLI 参考。
- [docs/wiki/modules/skills.md](docs/wiki/modules/skills.md)——技能书与 `gda install skills`。
- [tests/README.md](tests/README.md)——测试布局与命令。

## 许可证

MIT
