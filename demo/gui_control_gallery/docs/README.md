# Control Gallery · 设计文档与测试说明

本目录是一份**现状实录型文档集**：描述 `demo/gui_control_gallery/` 这个 Godot Control 控件陈列展示项目**实际是什么样**——场景组织、脚本行为、展示了哪些控件、工程配置与素材。同时作为 GDA 插件的端到端测试床。

- 作为**项目说明**：这不是"要做成什么样"的前瞻设计，而是对磁盘上真实文件的记录。引用路径以 `demo/gui_control_gallery/` 内真实存在的文件为准；不确定的写"未核实"，不猜测。
- 作为**插件测试床**：本工程用于评估 godot-autopilot（GDA）MCP 插件的**编辑器 UI 自动化能力**——打开主场景、在场景树/检查器中定位节点、操作控件、截图验证渲染；测试目的、目标、约定与记录规范见 [test/test-plan.md](./test/test-plan.md)。

> 本项目无玩法、无关卡、无敌人、无得分、无存档。凡模板中"玩法/关卡/敌人"的位置，本文档如实写"不存在"，转而描述真实的交互模型（滚动浏览、分隔拖拽、控件试用）。

## 文档地图

```text
docs/
├── README.md                    # 本文件：入口与阅读路径
├── pitch/
│   └── one-page-design.md       # 一页纸说明（概念、非目标、交互模型、成功标准）
├── gdd/
│   ├── gdd.md                   # 设计文档总纲（系统总览、内容范围、完成定义）
│   ├── mechanics.md             # 交互机制实录（规则、tree.gd 行为、验收）
│   ├── ui-hud.md                # 控件一览与行为（重点：场景结构逐面板记录）
│   └── level-design.md          # 关卡说明（本项目无关卡：单场景浏览路线）
├── art/
│   ├── asset-catalog.md         # 资源清单（磁盘真实文件，非虚构数量）
│   └── art-direction.md         # 美术方向与呈现要求
├── tech/
│   └── tdd.md                   # 技术设计文档（硬约束与实现自由度）
└── test/
    ├── test-plan.md             # 测试计划（目的、目标、约定、证据与记录规范）
    └── acceptance.md            # 验收标准汇总（顶层场景 + 视觉验收 + 分项索引）
```

## 阅读路径

| 你的目的 | 建议顺序 |
|---|---|
| 快速了解这是什么项目 | [一页纸说明](./pitch/one-page-design.md) → [GDD 总纲](./gdd/gdd.md) |
| 核对场景与控件细节 | [控件一览](./gdd/ui-hud.md) → [交互机制](./gdd/mechanics.md) → [资源清单](./art/asset-catalog.md) |
| 执行编辑器验证/验收 | [测试计划](./test/test-plan.md) → [验收标准](./test/acceptance.md) |

## 工程现状（只读事实）

- 主场景：`demo/gui_control_gallery/control_gallery.tscn`（根节点 `ControlGallery`，类型 `Control`，全屏锚点）。
- 唯一脚本：`demo/gui_control_gallery/tree.gd`（`@tool`，`extends Tree`，挂载于 Lists 面板的 `Tree` 节点），配套 `tree.gd.uid`。
- 工程配置：工作树内 `demo/gui_control_gallery/project.godot` 为最小模板（仅名称/特性 `4.7`/图标），**未声明 `run/main_scene`**；源库原件另有主场景、低处理器模式、拉伸、vsync、渲染法等配置，见 [tech/tdd.md](./tech/tdd.md)。
- 项目说明原文：`demo/gui_control_gallery/README.md`（GTK 灵感、三面板、拖拽分隔、Compatibility 渲染）。
- 素材：`demo/gui_control_gallery/icon.webp`（含 `.import`）、截图 `demo/gui_control_gallery/screenshots/control_gallery.webp`。
- 以上仅为背景信息；验证时以实际查看到的工程为准，引用路径必须真实存在。

## 关键约定

1. **单一权威**：场景结构与控件属性以 [ui-hud.md](./gdd/ui-hud.md) 为准，脚本行为以 [mechanics.md](./gdd/mechanics.md) 为准，资产以 [asset-catalog.md](./art/asset-catalog.md) 为准，技术约束以 [tdd.md](./tech/tdd.md) 为准；其他文档只引用，不复述第二份细节。
2. **视觉验证**：涉及渲染效果的验收项，必须**截图编辑器界面或运行画面**核对（面板排布、控件渲染、分隔拖拽、Tree 内容），仅凭日志或逻辑断言不算通过。详见 [test/test-plan.md](./test/test-plan.md)。
3. **严禁编造**：引用的每个文件路径必须在磁盘上真实存在；拿不准的写"未核实"，不猜测、不补全。
4. **顶层验收**：[test/acceptance.md](./test/acceptance.md)（场景 A/B/C/D/E）。
