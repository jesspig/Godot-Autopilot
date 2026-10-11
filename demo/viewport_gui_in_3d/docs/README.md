# GUI in 3D · 设计文档与测试说明

本目录是一份**现状实录型文档集**，同时作为 GDA 插件的测试床：

- 作为**项目说明**：如实描述 `demo/viewport_gui_in_3d/` 这个“把 2D 界面经视口实例化到 3D 场景并转发键鼠输入”的演示——双场景分工、输入转发机制、视口/3D 物理拾取链路。本演示**无传统玩法、无关卡、无胜负、无存档**，相关文档只写机制实录，不虚构玩法。
- 作为**插件测试床**：本工程用于评估 godot-autopilot（GDA）MCP 插件在编辑器内验证 3D 中 GUI 的能力，重点是**视口截图**与 **UI 点击**两项能力；测试目的、目标、约定与记录规范见 [test/test-plan.md](./test/test-plan.md)。

> 读者只被两样东西约束：**磁盘文件事实**与**可客观验证的行为**。凡文档中标注“未核实”的，均不得当作事实引用，详见 [tech/tdd.md](./tech/tdd.md)。

## 文档地图

```text
docs/
├── README.md                    # 本文件：入口与阅读路径
├── pitch/
│   └── one-page-design.md       # 一页纸设计（概念、非玩法声明、交互循环、范围、成功标准）
├── gdd/
│   ├── gdd.md                   # 设计总纲（系统总览、内容范围、完成定义）
│   ├── mechanics.md             # 机制规格（视口复合与输入转发契约，唯一权威；无玩法）
│   ├── ui-hud.md                # 2D 面板规格（控件清单与信息契约）
│   └── level-design.md          # 3D 场景布置实录（无传统关卡；观察/点击动线）
├── art/
│   ├── asset-catalog.md         # 资源清单（磁盘文件的逐项登记）
│   └── art-direction.md         # 美术方向与呈现要求
├── tech/
│   └── tdd.md                   # 技术设计（配置权威：源库原件 vs 工作树最小模板）
└── test/
    ├── test-plan.md             # 测试计划（目的、目标、约定、证据与记录规范）
    └── acceptance.md            # 验收标准汇总（顶层场景 A–G + 视觉验收 + 分项索引）
```

## 阅读路径

| 你的目的 | 建议顺序 |
|---|---|
| 快速了解这是什么演示 | [一页纸设计](./pitch/one-page-design.md) → [GDD](./gdd/gdd.md) |
| 理解输入转发原理 | [机制规格](./gdd/mechanics.md) → [脚本 `../gui_3d.gd`](../gui_3d.gd)（只读） |
| 执行测试/验收 | [测试计划](./test/test-plan.md) → [验收标准](./test/acceptance.md) |
| 核对配置与资源 | [TDD](./tech/tdd.md) → [资源清单](./art/asset-catalog.md) |

## 工程现状（只读事实）

- `demo/viewport_gui_in_3d/` 是一个 Godot 4.x 演示工程，源码完整（虽被 git 忽略，磁盘文件齐全）。
- 主场景为 `res://gui_in_3d.tscn`（源库原件配置；工作树内 `project.godot` 当前为最小模板，详见 [tech/tdd.md](./tech/tdd.md)）。
- 核心文件：`gui_in_3d.tscn`（3D 主场景）、`gui_panel_3d.tscn`（GUI 面板场景）、`gui_3d.gd`（输入转发脚本）、`icon.webp`（面板上的纹理与工程图标）、`screenshots/gui.png`（官方截图）。
- 完整文件清单与逐项说明见 [asset-catalog.md](./art/asset-catalog.md)；引用文件路径时以该文为准，不得引用不存在的路径。

## 关键约定

1. **单一权威**：链路与转发规则只在 [mechanics.md](./gdd/mechanics.md)，控件清单只在 [ui-hud.md](./gdd/ui-hud.md)，3D 布置只在 [level-design.md](./gdd/level-design.md)，资产只在 [asset-catalog.md](./art/asset-catalog.md)，配置以 [tdd.md](./tech/tdd.md) 为准；其他文档只引用，不复述第二份数字。
2. **视觉验证**：凡涉及成像与交互的验收项，必须**截图编辑器界面或运行画面**核对（面板是否成像于 Quad、点击是否生效），仅凭日志或逻辑断言不算通过。详见 [test/test-plan.md](./test/test-plan.md)。
3. **严禁编造**：不确定的写“未核实”而不猜测；禁止引用磁盘上不存在的文件路径。
4. **顶层验收**：[test/acceptance.md](./test/acceptance.md)（场景 A/B/C/D/E/F/G）。
5. **只读红线**：编写与执行本文档集期间，不运行 Godot、不构建、不改动 `docs/` 之外的任何文件。
