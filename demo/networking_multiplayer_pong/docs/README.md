# 多人联机 Pong · 设计文档与测试说明

本目录是一份**现状实录型文档集**，同时作为 GDA 插件的端到端测试床：

- 作为**游戏说明**：如实描述 `demo/networking_multiplayer_pong/` 当前磁盘状态——Host/Join 大厅 + 双人联机 Pong 对战"实际是什么样"：四个场景的分工、`logic/` 下四个脚本的网络同步机制、记分与胜负、输入动作、美术资源。本文档只记录**已核实的行为与数值**，不规定新功能，不虚构缺失内容。
- 作为**插件测试床**：本工程用于评估 godot-autopilot（GDA）MCP 插件在真实联机游戏上的表现；测试目的、目标、约定与记录规范见 [test/test-plan.md](./test/test-plan.md)。

> 读者只被两样东西约束：**文件事实**与**可客观验证的行为/数值**。数值以 [gdd/mechanics.md](./gdd/mechanics.md) 为准，坐标以 [gdd/level-design.md](./gdd/level-design.md) 为准，资产以 [art/asset-catalog.md](./art/asset-catalog.md) 为准；其他文档只引用，不复述第二份数字。

## 文档地图

```text
docs/
├── README.md                    # 本文件：入口与阅读路径
├── pitch/
│   └── one-page-design.md       # 一页纸设计（概念、钩子、核心循环、范围、成功标准）
├── gdd/
│   ├── gdd.md                   # 游戏设计总纲（系统总览、内容范围、完成定义）
│   ├── mechanics.md             # 玩法与联机机制规格（规则、参数、同步方式、行为契约）
│   ├── ui-hud.md                # 大厅/对战界面规格（界面信息契约与提示时机）
│   └── level-design.md          # 球场设计（640×400 场地坐标、布置、验收路线）
├── art/
│   ├── asset-catalog.md         # 美术资源清单（3 个 PNG 逐文件枚举 + 关联文件）
│   └── art-direction.md         # 美术方向与呈现要求
├── tech/
│   └── tdd.md                   # 技术设计文档（配置、场景/脚本映射、RPC 表、测试边界）
└── test/
    ├── test-plan.md             # 测试计划（目的、目标、约定、证据与记录规范）
    └── acceptance.md            # 验收标准汇总（顶层场景 + 视觉验收 + 分项索引）
```

## 阅读路径

| 你的目的 | 建议顺序 |
|---|---|
| 快速了解这是什么游戏 | [一页纸设计](./pitch/one-page-design.md) → [GDD](./gdd/gdd.md) |
| 理解联机原理 | [玩法与联机机制](./gdd/mechanics.md) → [技术设计](./tech/tdd.md) |
| 核对画面与资源 | [美术资源](./art/asset-catalog.md) → [美术方向](./art/art-direction.md) → [界面](./gdd/ui-hud.md) → [球场](./gdd/level-design.md) |
| 执行测试/验收 | [测试计划](./test/test-plan.md) → [验收标准](./test/acceptance.md) |

## 工程现状（只读事实）

- `demo/networking_multiplayer_pong/` 含 **4 个场景**：`lobby.tscn`（大厅）、`pong.tscn`（对战场）、`ball.tscn`（球）、`paddle.tscn`（球拍）；**4 个脚本**：`logic/lobby.gd`、`logic/pong.gd`、`logic/ball.gd`、`logic/paddle.gd`。
- `logic/` 下另有 4 个 `.uid` 文件；根目录有 `ball.png`、`paddle.png`、`separator.png` 三张 gameplay PNG、`icon.webp` 图标、`screenshots/pong_multiplayer.png` 截图、`README.md`（官方英文说明）。
- 主场景为 `lobby.tscn`；显示 640×400、整数拉伸、画布纹理最近邻过滤、输入 `move_up`/`move_down`、`gl_compatibility`、未类型化声明警告——以上以源库原件 `project.godot` 为准（工作树内 `project.godot` 当前为最小模板，详见 [tech/tdd.md](./tech/tdd.md) 的"配置现状"节）。
- 以上仅为背景信息；执行测试时以实际查看到的工程为准，本文档与磁盘不一致处以磁盘为准并记录偏差。

## 关键约定

1. **单一权威**：数值只在 [mechanics.md](./gdd/mechanics.md)（含端口、胜分、速度），坐标只在 [level-design.md](./gdd/level-design.md)，资产只在 [asset-catalog.md](./art/asset-catalog.md)，RPC 与权限只在 [tdd.md](./tech/tdd.md)；其他文档只引用，不复述第二份数字。
2. **视觉验证**：涉及画面效果的验收项，必须**截图编辑器界面或游戏运行画面**核对（大厅布局、球场摆位、HUD、胜负标签等），仅凭日志或逻辑断言不算通过。详见 [test/test-plan.md](./test/test-plan.md)。
3. **偏差记录**：文档是现状实录；若实测行为与本文档不符，以实测为准，并在交付说明中记录"哪一条、实测是什么、证据"。
4. **顶层验收**：[test/acceptance.md](./test/acceptance.md)（场景 A/B/C/D/E）。
5. **单机限制**：单机环境下双实例联调只能验证本机回环（`127.0.0.1`）路径；跨机器、端口转发、公网 IP 相关条目按"未核实"处理，详见 [test/test-plan.md](./test/test-plan.md) 的"单机环境限制"节。
