# 三关卡平台跳跃游戏《Pink Man 大冒险》· 设计文档与测试说明

本目录是一份**标准化的独立游戏设计文档集**，同时作为一个 MCP 插件的测试床：

- 作为**游戏设计文档**：描述一款三关卡 2D 马里奥-like 平台跳跃游戏"要做成什么样"——概念、玩法、敌人、关卡、菜单、美术、技术与验收。目标形态是完整闭环：**主菜单 → 选关 → 游玩 → 暂停 → 结算 → 通关解锁**，含巡逻/轨道/喷发三型敌人、问号箱顶水果、命数与 GameOver、解锁存档。文档只约束**行为与表现**，不规定实现结构、工具与操作步骤。
- 作为**插件测试床**：本工程用于评估 godot-autopilot（GDA）MCP 插件在真实游戏开发任务中的表现；测试目的、目标、约定与记录规范见 [test/test-plan.md](./test/test-plan.md)。

> 实现者只被两样东西约束：**素材事实**与**可客观验证的行为/数值**。其余一切（目录结构、节点/脚本组织、命名、碰撞层位值、实现顺序、工具选择、调参）由实现者自行决策，详见 [tech/tdd.md](./tech/tdd.md)。

## 文档地图

```text
docs/
├── README.md                    # 本文件：入口与阅读路径
├── pitch/
│   └── one-page-design.md       # 一页纸设计（概念、钩子、核心循环、范围、成功标准）
├── gdd/
│   ├── gdd.md                   # 游戏设计文档总纲（系统总览、内容范围、完成定义）
│   ├── mechanics.md             # 玩法机制规格（规则、参数、敌人、命数、菜单状态、行为契约）
│   ├── ui-hud.md                # 菜单/HUD 规格（界面信息契约与提示时机）
│   └── level-design.md          # 三关卡设计（`level_01/02/03` 的坐标、布置、验收路线）
├── art/
│   ├── asset-catalog.md         # 美术资源统计（173 个 PNG 的完整清单 + 完整版资源映射）
│   └── art-direction.md         # 美术方向与呈现要求
├── tech/
│   └── tdd.md                   # 技术设计文档（硬约束与实现自由度，含唯一写盘例外）
└── test/
    ├── test-plan.md             # 测试计划（目的、目标、约定、证据与记录规范）
    └── acceptance.md            # 验收标准汇总（顶层场景 + 视觉验收 + 分项索引）
```

## 阅读路径

| 你的目的 | 建议顺序 |
|---|---|
| 快速了解这是什么游戏 | [一页纸设计](./pitch/one-page-design.md) → [GDD](./gdd/gdd.md) |
| 动手实现 | [测试计划增量循环节](./test/test-plan.md#增量循环执行法) → [TDD（自由度边界）](./tech/tdd.md) → [玩法机制](./gdd/mechanics.md) → [关卡设计](./gdd/level-design.md) → [美术资源](./art/asset-catalog.md) → [美术方向](./art/art-direction.md) → [菜单/HUD](./gdd/ui-hud.md) |
| 执行测试/验收 | [测试计划](./test/test-plan.md) → [验收标准](./test/acceptance.md) |

## 工程现状（只读事实）

- `Example/` 是一个 Godot 4.x 工程，当前**不包含**任何游戏代码、场景或存档。
- `Example/assets/` 下有 **173 张 PNG**：背景 7、主角色 30、道具 25、菜单 63、其他 4、地形 1、陷阱 43。
- 完整版启用的关键素材：三型敌人出自 `Traps/`（石怪/圆锯/火焰）、问号箱出自 `Items/Boxes/`、菜单按钮与 `Levels/01-03.png` 缩略图出自 `Menu/`，映射关系见 [asset-catalog.md](./art/asset-catalog.md) 的"完整版资源映射"。
- 素材路径包含空格、括号、大小写差异与个别拼写怪癖（如 `Desappearing`、`Flag Idle)(64x64)`），引用时保持原样。
- 以上仅为背景信息；实现者不应对"现有工程状态"做假设，以实际查看到的工程为准。

## 关键约定

1. **单一权威**：数值只在 [mechanics.md](./gdd/mechanics.md)（含敌人速度/周期、命数规则），坐标只在 [level-design.md](./gdd/level-design.md)（含三关地形与敌人区间），资产只在 [asset-catalog.md](./art/asset-catalog.md)，存档键语义以 [tdd.md](./tech/tdd.md) 为准；其他文档只引用，不复述第二份数字。
2. **视觉验证**：涉及画面效果的验收项，必须**截图编辑器界面或游戏运行画面**核对（摆放、相机、动画、贴地、HUD、菜单、敌人等），仅凭日志或逻辑断言不算通过。详见 [test/test-plan.md](./test/test-plan.md)。
3. **偏差记录**：允许为实现可行性微调设计数值或几何，但必须在交付说明中记录"改了什么、为什么、验证证据"。
4. **顶层验收**：[test/acceptance.md](./test/acceptance.md)（场景 A/B/C/D/E/F/G）。
5. **增量循环**：开发按 [test-plan.md](./test/test-plan.md) 的“增量循环执行法”推进（搭→写→看→定，单环单功能增量）。
