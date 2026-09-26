# Squash the Creeps（3D）· 文档集与测试说明

本目录是一份**已建成游戏的现状实录文档集**，同时作为 GDA 插件的端到端测试床：

- 作为**游戏说明**：描述官方 3D 入门游戏 Squash the Creeps（追逐并压扁小怪）**实际是什么样**——主场景结构、Player/Mob 脚本行为、挤压判定、跳跃、敌人生成、音乐自动加载、物理与层配置、输入动作、美术资源清单。所有陈述以磁盘上的工程文件为准，不描述"计划做什么"。
- 作为**插件测试床**：本工程用于评估 godot-autopilot（GDA）MCP 插件在**已建成 3D 工程**上的端到端表现（打开场景、检查节点、运行验证、截图取证）；测试目的、目标、约定与记录规范见 [test/test-plan.md](./test/test-plan.md)。

> 文档只记录**现状事实**与**可客观验证的行为/数值**。引用的每个文件路径在磁盘上真实存在；无法确认的内容标注"未核实"，不猜测。

## 文档地图

```text
docs/
├── README.md                  # 本文件：入口与阅读路径
├── pitch/
│   └── one-page-design.md     # 一页纸设计（概念、钩子、核心循环、范围、成功标准）
├── gdd/
│   ├── gdd.md                 # GDD 总纲（系统总览、内容范围、完成定义）
│   ├── mechanics.md           # 玩法机制实录（规则与数值的唯一权威）
│   ├── ui-hud.md              # 菜单/HUD 实录（界面信息契约与出现时机）
│   └── level-design.md        # 关卡实录（竞技场几何的唯一权威）
├── art/
│   ├── art-direction.md       # 美术方向与呈现方式
│   └── asset-catalog.md       # 美术资源清单（逐文件枚举的唯一权威）
├── tech/
│   └── tdd.md                 # 技术设计实录（场景/脚本/配置接线的唯一权威）
└── test/
    ├── test-plan.md           # 测试计划（目的、目标、约定、证据与记录规范）
    └── acceptance.md          # 验收标准（可在编辑器里执行的验证点汇总）
```

## 阅读路径

| 你的目的 | 建议顺序 |
|---|---|
| 快速了解这是什么游戏 | [一页纸设计](./pitch/one-page-design.md) → [GDD 总纲](./gdd/gdd.md) |
| 核对玩法数值 | [玩法机制](./gdd/mechanics.md)（数值唯一权威） |
| 核对场景几何 | [关卡设计](./gdd/level-design.md)（坐标唯一权威） |
| 核对资源 | [资源清单](./art/asset-catalog.md)（资产唯一权威） |
| 核对接线与配置 | [TDD](./tech/tdd.md)（技术接线唯一权威） |
| 执行测试/验收 | [测试计划](./test/test-plan.md) → [验收标准](./test/acceptance.md) |

## 工程现状（只读事实）

- 工程目录：`demo/3d_squash_the_creeps/`，Godot 4.x 工程（`project.godot` 记载 `config/features=PackedStringArray("4.7")`）。
- 主场景：`Main.tscn`（源库原件 `run/main_scene="res://Main.tscn"`；工作树内 `project.godot` 已被清洗为最小模板，仅保留名称/特性/图标，详见 [tech/tdd.md](./tech/tdd.md)）。
- 脚本 4 个：`Main.gd`、`Player.gd`、`Mob.gd`、`ScoreLabel.gd`（另有同名 `.uid` 文件）。
- 场景 4 个：`Main.tscn`、`Player.tscn`、`Mob.tscn`、`MusicPlayer.tscn`。
- `art/` 目录 13 个条目：10 个真实资产 + 3 个 `.import` 文件；`fonts/` 3 个条目；`screenshots/` 2 个条目。逐文件清单见 [asset-catalog.md](./art/asset-catalog.md)。
- 官方 README：`demo/3d_squash_the_creeps/README.md`（玩法一句话、键位表、教程链接、Renderer: Forward+、许可说明）。
- 游戏截图：`screenshots/squash_the_creeps.webp`。

## 关键约定

1. **单一权威**：数值只在 [mechanics.md](./gdd/mechanics.md)，几何只在 [level-design.md](./gdd/level-design.md)，资产只在 [asset-catalog.md](./art/asset-catalog.md)，场景/信号/配置接线只在 [tdd.md](./tech/tdd.md)；其他文档只引用，不复述第二份数字。
2. **视觉验证**：涉及画面效果的验收项，必须**截图编辑器界面或游戏运行画面**核对（场景摆放、3D 视角、动画、HUD、重试面板、小怪行为等），仅凭日志或逻辑断言不算通过。详见 [test/test-plan.md](./test/test-plan.md)。
3. **偏差记录**：文档是现状实录，若发现文档与磁盘文件不一致，以磁盘文件为准，并在测试报告中记录"哪份文档、哪句话、实际是什么、证据"。
4. **顶层验收**：[test/acceptance.md](./test/acceptance.md)。
5. **不确定标注**：无法确认的内容写"未核实"，不猜测、不编造。
