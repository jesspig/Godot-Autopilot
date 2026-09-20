# Godot-Autopilot

[![许可证](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![构建状态](https://github.com/jesspig/Godot-Autopilot/actions/workflows/ci.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions)
![Godot版本](https://img.shields.io/badge/Godot-4.7%2B-478CBF?logo=godotengine&logoColor=white)
![支持平台](https://img.shields.io/badge/platform-Windows%7CLinux%7CmacOS-lightgrey)
[![协议](https://img.shields.io/badge/MCP-Streamable_HTTP-blueviolet)](https://modelcontextprotocol.io/)
![工具数](https://img.shields.io/badge/tools-385%2B-brightgreen)

> **让 AI 代理在编辑器里帮你搭场景、试玩和排错。**

[English Version](README.en.md)

Godot-Autopilot 是一个住在 **Godot 编辑器内部**的插件型 [MCP](https://modelcontextprotocol.io/) 服务端。装好之后，你的 AI 编程助手（Claude Code、Cursor、Codex、OpenCode 等）就能像你一样操作项目：搭场景、调属性、运行游戏、点按钮、截图、看日志——然后根据看到的结果继续迭代。

不需要桥接进程，也不用把报错复制粘贴到聊天框。打开项目时服务端自动启动，关闭项目时自动停止。

## 端到端演示

端到端测试中 AI 实时控制游戏角色：

![Demo 001](assets/demo-001.gif)

## 能做什么

**385+ 个工具**，覆盖从编辑器到游戏的完整闭环（数量随版本增长，以代理通过 `search_tools` 查到的为准）。举例：

- **搭场景**——创建、改名、移动、删除节点，读写属性，连接信号（如 `create_scene_node`、`property_set`、`signal_connect`、`scene_tree_items`）。
- **画面与声音**——网格材质、灯光环境、音频播放、动画、UI 主题、瓦片地图（如 `create_animation`、`play_audio_player`、`fill_tilemap_rect`）。
- **物理与导航**——射线/形状检测、刚体、导航地图与寻路（如 `intersect_physics_3d_ray`、`create_nav_3d_map`、`get_nav_3d_map_path`）。
- **动手：输入与 UI 自动化**——模拟键盘鼠标手柄，按名称点击编辑器控件，执行编辑器快捷键，点击运行中游戏的按钮（如 `click_input_mouse`、`get_editor_ui_elements`、`click_editor_element`、`run_editor_shortcut`、`click_game_ui_element`）。
- **脚本与排错**——在编辑器或游戏中执行 GDScript，把耗时游戏脚本作为后台任务运行，读取插件/游戏/脚本错误日志（如 `execute_script`、`execute_game_script`、`start_game_job`、`get_plugin_log`）。
- **项目与知识**——读写搜索项目文件，查看项目设置，离线查询引擎内置 API 文档（如 `read_file`、`find_in_files`、`get_project_settings`、`get_docs_class`）。
- **试玩与视觉验证**——运行当前场景，注入定时输入序列，给编辑器和游戏截图并对比前后差异（如 `play_editor_current_scene`、`queue_game_input`、`capture_editor_viewport`、`capture_game_viewport`、`review_scene_visually`）。

## 代理的使用方式

你不需要记住工具名。代理按这个循环工作：

1. **发现**——用 `search_tools` 找候选，用 `get_tool_detail` 确认参数（不要猜名字）。
2. **执行**——单步用 `call_tool`，批量用 `batch_execute`。
3. **验证**——截图看结果，与上次截图对比，有问题就修，再确认。

为了让代理开箱即用，插件内置 **8 册技能书**（总纲 + 场景、资源、脚本、运行时、服务器、内容、C# 分册）与 **7 篇新手指南**（3D 场景、角色控制器、物理排错、输入映射、GUI 等）。在 **MCP Config** 面板点一下，就能把技能渲染到项目的 `.agents/skills/` 目录。

插件自带两个编辑器面板：

- **MCP Config**——服务器状态与端口、一键生成客户端配置、高风险能力开关、技能生成。
- **Log**——仅显示插件日志，支持级别/分类过滤与搜索，方便定位失败调用。

## 快速开始

需要 **Godot 4.7+**。

1. **获取插件**——从项目 GitHub Releases 页（[jesspig/Godot-Autopilot](https://github.com/jesspig/Godot-Autopilot)）下载 `godot-autopilot-<version>.zip`，把其中的 `godot-autopilot/` 文件夹复制到你项目的 `addons/` 目录：
   ```
   your-project/
   └── addons/
       └── godot-autopilot/
           ├── godot-autopilot.dll      (Windows)
           ├── libgodot-autopilot.so    (Linux)
           ├── libgodot-autopilot.dylib (macOS)
           └── godot-autopilot.gdextension
   ```
2. **打开项目**——服务端自动启动，监听插件配置的端口（默认 `http://127.0.0.1:9527/mcp`），无需启用开关。
3. **连接 AI 客户端**——在编辑器的 **MCP Config** 面板选客户端并点击 **Generate**。支持 20 个客户端，包括 Claude Code、Codex、Cursor、OpenCode、Copilot 等；客户端如有提示请批准生成的配置文件。
4. **解锁高级能力（可选）**——脚本执行（`code_execute`）与游戏控制（`game_runtime`）默认关闭，需要时在面板勾选。
5. **提需求**——比如“创建一个能跑能跳的玩家角色，试玩一下并修掉看着不对的地方”。

想从源码构建或参与贡献？见 [docs/wiki/](docs/wiki/)（入口 [index.md](docs/wiki/index.md)，构建见 [build.md](docs/wiki/build.md)）与 [tests/README.md](tests/README.md)。

## 安全说明

服务端只监听本机回环地址（端口取自插件配置，默认 9527，路径固定 `/mcp`），非本机地址拒绝启动。脚本执行、游戏控制、操作系统进程类工具**默认全部拒绝**——只在你信任的本地项目中开启。详见 [docs/wiki/security_contract.md](docs/wiki/security_contract.md)。

## 素材声明

`Example/` 测试项目使用了 [Pixel Adventure 1](https://pixelfrog-assets.itch.io/pixel-adventure-1) 的像素艺术素材，版权归 Pixel Frog 所有。

## 许可证

MIT
