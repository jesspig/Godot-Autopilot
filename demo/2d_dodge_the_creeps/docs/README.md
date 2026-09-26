# Dodge the Creeps · 文档集与测试说明

本目录是一份**已建成游戏的现状实录文档集**，同时作为 GDA 插件的端到端测试床：

- 作为**游戏说明**：描述官方 2D 入门游戏 Dodge the Creeps“实际是什么样”——主场景 `main.tscn`、4 个脚本的分工、敌人生成 / 记分 / 碰撞 / HUD / 重开流程、输入动作、美术与音频资源清单。全部以磁盘源码为准，不做设计发挥。
- 作为**插件测试床**：本工程用于评估 godot-autopilot（GDA）MCP 插件在已有小体量工程上的导航、读写、运行验证与截图核对能力；测试目的、约定与记录规范见 [test/test-plan.md](./test/test-plan.md)。

> 文档只记录**现状行为与可验证事实**。拿不准的写“未核实”，不猜测、不补全。

## 文档地图

```text
docs/
├── README.md                    # 本文件：入口与阅读路径
├── pitch/
│   └── one-page-design.md       # 一页纸说明（概念、循环、范围、现状）
├── gdd/
│   ├── gdd.md                   # 总纲（系统总览、内容范围、完成定义）
│   ├── mechanics.md             # 玩法机制实录（规则、参数、流程、行为契约）
│   ├── ui-hud.md                # 菜单/HUD 实录（节点与显示时机）
│   └── level-design.md          # 关卡/场景实录（主场景结构与生成区间）
├── art/
│   ├── art-direction.md         # 美术方向实录（风格与呈现）
│   └── asset-catalog.md         # 资源清单（art/ 与 fonts/ 逐文件枚举）
├── tech/
│   └── tdd.md                   # 技术实录（场景、脚本、信号、配置差异）
└── test/
    ├── test-plan.md             # 测试计划（目的、约定、证据与记录规范）
    └── acceptance.md            # 验收标准（可在编辑器里逐条执行）
```

## 阅读路径

| 你的目的 | 建议顺序 |
|---|---|
| 快速了解这是什么游戏 | [一页纸说明](./pitch/one-page-design.md) → [GDD 总纲](./gdd/gdd.md) |
| 核对行为细节 | [玩法机制](./gdd/mechanics.md) → [场景实录](./gdd/level-design.md) → [技术实录](./tech/tdd.md) → [资源清单](./art/asset-catalog.md) |
| 执行测试/验收 | [测试计划](./test/test-plan.md) → [验收标准](./test/acceptance.md) |

## 工程现状（只读事实）

- `demo/2d_dodge_the_creeps/` 是一个 Godot 4.x 已建成工程，主场景为 `main.tscn`。
- 场景 4 个：`main.tscn`、`player.tscn`、`mob.tscn`、`hud.tscn`；脚本 4 个：`main.gd`、`player.gd`、`mob.gd`、`hud.gd`。
- `art/` 下有 12 个资源文件（10 张 PNG + 2 个音频，另有同名 `.import` 文件）；`fonts/` 下有 3 个文件（1 个 TTF + 2 个 TXT，另有 `.import` 文件）。逐文件清单见 [asset-catalog.md](./art/asset-catalog.md)。
- 工作树内 `project.godot` 已被清洗为最小模板（仅应用名、特性与图标）；可玩所需的输入映射、显示尺寸与渲染器配置以源库原件（godot-demo-projects `2d/dodge_the_creeps/project.godot`）为准，差异见 [tech/tdd.md](./tech/tdd.md)。
- `screenshots/dodge.png` 为官方截图；`README.md` 为官方英文说明（含美术/音频/字体授权）。

## 关键约定

1. **单一权威**：行为数值以 [mechanics.md](./gdd/mechanics.md) 为准，资源以 [asset-catalog.md](./art/asset-catalog.md) 为准，场景节点与信号以 [tdd.md](./tech/tdd.md) 为准；其他文档只引用，不复述第二份数字。
2. **视觉验证**：涉及画面效果的验收项，必须**截图编辑器界面或游戏运行画面**核对，仅凭日志或逻辑断言不算通过。详见 [test/test-plan.md](./test/test-plan.md)。
3. **偏差记录**：测试中发现文档与实际行为不一致时，以实际源码为准，并在报告中记录。
4. **顶层验收**：[test/acceptance.md](./test/acceptance.md)。
