# 美术资源统计

本文是 `Example/assets/` 的**资产事实来源**。路径均以资产目录为根；引用时保留原始大小写、空格、括号与拼写。视觉与状态的对应规则见 [../gdd/mechanics.md](../gdd/mechanics.md)，首版实际摆放见 [../gdd/level-design.md](../gdd/level-design.md)，呈现要求见 [art-direction.md](./art-direction.md)。

- 图片尺寸与可由整宽度整除关系确定的横向帧数视为事实。
- 文件名中的 `Idle`、`On`、`Hit`、`Appearing` 等只描述视觉状态，**不能单独证明运行行为**。
- "建议用途"是设计参考，不约束实现结构。

## 数量总览

| 顶层目录 | 文件数 | 内容 |
|---|---:|---|
| `Background/` | 7 | 7 张 64×64 纯色系背景 |
| `Main Characters/` | 30 | 4 个角色 × 7 个状态 + 2 个特效 |
| `Items/` | 25 | 8 种水果、收集特效、3 类箱子、起点、检查点、终点 |
| `Menu/` | 63 | 11 个按钮、50 张关卡缩略图、2 张文字图集 |
| `Other/` | 4 | Confetti、Dust Particle、Shadow、Transition |
| `Terrain/` | 1 | 16×16 Terrain 图块图集（22 列 × 11 行） |
| `Traps/` | 43 | 13 类陷阱与机关素材 |
| **合计** | **173** | — |

## Background

| 路径 | 图片尺寸 | 语义 | 建议用途 | 首版 |
|---|---:|---|---|---|
| `Background/Blue.png` | 64 × 64 | 蓝色系背景 | 关卡背景（平铺） | 必需 |
| `Background/Brown.png` | 64 × 64 | 棕色背景 | 主题变体 | 可选 |
| `Background/Gray.png` | 64 × 64 | 灰色背景 | 主题变体 | 可选 |
| `Background/Green.png` | 64 × 64 | 绿色背景 | 主题变体 | 可选 |
| `Background/Pink.png` | 64 × 64 | 粉色背景 | 主题变体 | 可选 |
| `Background/Purple.png` | 64 × 64 | 紫色背景 | 主题变体 | 可选 |
| `Background/Yellow.png` | 64 × 64 | 黄色背景 | 主题变体 | 可选 |

## Main Characters

四个角色目录：`Mask Dude`、`Ninja Frog`、`Pink Man`、`Virtual Guy`。每个目录下七张状态图：

| 文件名 | 图片尺寸 | 单帧 | 帧数 | 语义 |
|---|---|---:|---:|---|
| `Idle (32x32).png` | 352 × 32 | 32 × 32 | 11 | 待机 |
| `Run (32x32).png` | 384 × 32 | 32 × 32 | 12 | 奔跑 |
| `Jump (32x32).png` | 32 × 32 | 32 × 32 | 1 | 上升 |
| `Fall (32x32).png` | 32 × 32 | 32 × 32 | 1 | 下落 |
| `Double Jump (32x32).png` | 192 × 32 | 32 × 32 | 6 | 二段跳 |
| `Hit (32x32).png` | 224 × 32 | 32 × 32 | 7 | 受击/死亡 |
| `Wall Jump (32x32).png` | 160 × 32 | 32 × 32 | 5 | 墙跳 |

- 默认角色为 **Pink Man**；其余三个角色与 Pink Man 共用同一玩法契约，首版不要求选择界面。
- 帧数可用于设计动画节奏（如死亡表现目标 0.70s，可据此选播放速度）。

角色特效（均在 `Main Characters/` 根下）：

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 首版 |
|---|---|---:|---:|---|---|
| `Appearing (96x96).png` | 672 × 96 | 96 × 96 | 7 | 登场特效 | 可选 |
| `Desappearing (96x96).png` | 672 × 96 | 96 × 96 | 7 | 消失特效（拼写就是 `Desappearing`） | 可选 |

## Items

### 水果（`Items/Fruits/`）

8 种水果 + 1 张收集特效，均按 32×32 横向切片，水果各 17 帧：

| 路径 | 帧数 | 首版 |
|---|---:|---|
| `Fruits/Apple.png` | 17 | 必需（首版使用的唯一水果） |
| `Fruits/Bananas.png` / `Cherries.png` / `Kiwi.png` / `Melon.png` / `Orange.png` / `Pineapple.png` / `Strawberry.png` | 各 17 | 可选 |
| `Fruits/Collected.png` | 6 | 可选（收集特效，可用占位效果替代） |

### 箱子（`Items/Boxes/`，首版不实例化）

三个目录 `Box1/`、`Box2/`、`Box3/`，各含：

| 文件名 | 图片尺寸 | 单帧 | 帧数 |
|---|---|---:|---:|
| `Idle.png` | 28 × 24 | 28 × 24 | 1 |
| `Hit (28x24).png` | Box1 84×24 / Box2 112×24 / Box3 56×24 | 28 × 24 | 3 / 4 / 2 |
| `Break.png` | 112 × 24 | 28 × 24 | 4 |

### 进度物（`Items/Checkpoints/`）

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 首版 |
|---|---|---:|---:|---|---|
| `Checkpoints/Start/Start (Idle).png` | 64 × 64 | 64 × 64 | 1 | 起点静态视觉 | 必需 |
| `Checkpoints/Start/Start (Moving) (64x64).png` | 1088 × 64 | 64 × 64 | 17 | 起点动态视觉 | 可选 |
| `Checkpoints/Checkpoint/Checkpoint (No Flag).png` | 64 × 64 | 64 × 64 | 1 | 未激活检查点 | 必需 |
| `Checkpoints/Checkpoint/Checkpoint (Flag Out) (64x64).png` | 1664 × 64 | 64 × 64 | 26 | 激活过渡 | 可选 |
| `Checkpoints/Checkpoint/Checkpoint (Flag Idle)(64x64).png` | 640 × 64 | 64 × 64 | 10 | 已激活循环（注意 `Idle)` 前无空格） | 可选 |
| `Checkpoints/End/End (Idle).png` | 64 × 64 | 64 × 64 | 1 | 终点静态视觉 | 必需 |
| `Checkpoints/End/End (Pressed) (64x64).png` | 512 × 64 | 64 × 64 | 8 | 终点触发视觉 | 可选 |

## Terrain

| 路径 | 图片尺寸 | 单帧 | 网格 | 首版 |
|---|---|---:|---|---|
| `Terrain/Terrain (16x16).png` | 352 × 176 | 16 × 16 | 22 列 × 11 行（共 242 格） | 必需 |

图集包含多种地貌主题（灰色石砖、草地泥土、岩浆红土、砖块等）。**每个格子的地形语义与碰撞须在实现时逐项确认**，不要按颜色或位置猜测；首版只需要"实体地面/平台"语义，特殊地貌留作扩展。

## Traps（首版仅用 Spikes）

| 类别 | 文件集合 | 图片尺寸 | 单帧 | 帧数 | 语义 | 首版 |
|---|---|---|---|---:|---|---|
| Spikes | `Traps/Spikes/Idle.png` | 16 × 16 | 16 × 16 | 1 | 静态尖刺 | **必需（首版唯一陷阱）** |
| Arrow | `Traps/Arrow/Idle (18x18).png` / `Hit (18x18).png` | 180 × 18 / 72 × 18 | 18 × 18 | 10 / 4 | 箭矢 | 可选 |
| Blocks | `Traps/Blocks/Idle.png`、`Part 1/2 (22x22).png`、`HitTop (22x22).png`、`HitSide (22x22).png` | 22 × 22 系 | 22 × 22 | 1 / 各 3 | 可击碎方块 | 可选 |
| Falling Platforms | `Traps/Falling Platforms/Off.png` / `On (32x10).png` | 32 × 10 / 128 × 10 | 32 × 10 | 1 / 4 | 坠落平台 | 可选 |
| Fan | `Traps/Fan/Off.png` / `On (24x8).png` | 24 × 8 / 96 × 8 | 24 × 8 | 1 / 4 | 风扇 | 可选 |
| Fire | `Traps/Fire/Off.png` / `On (16x32).png` / `Hit (16x32).png` | 16 × 32 系 | 16 × 32 | 1 / 3 / 4 | 火焰 | 可选 |
| Platforms | `Traps/Platforms/Brown Off.png`、`Brown On (32x8).png`、`Grey Off.png`、`Grey On (32x8).png`、`Chain.png` | 32 × 8 系 | 32 × 8 / 8 × 8 | 1 / 8 / 1 | 移动平台与链条（拼写是 `Grey`） | 可选 |
| Rock Head | `Traps/Rock Head/Idle.png`、`Blink (42x42).png`、`Top/Bottom/Left/Right Hit (42x42).png` | 42 × 42 系 | 42 × 42 | 1 / 4 / 各 4 | 石怪头 | 可选 |
| Sand Mud Ice | `Traps/Sand Mud Ice/Sand Mud Ice (16x6).png`、`Sand/Mud/Ice Particle.png` | 176 × 80；粒子 16 × 16 | 布局待确认 | 不宣称帧数 | 沙/泥/冰地表（文件名 16x6 与实际尺寸不符） | 可选 |
| Saw | `Traps/Saw/Off.png` / `On (38x38).png` / `Chain.png` | 38 × 38 / 304 × 38 / 8 × 8 | 38 × 38 | 1 / 8 / 1 | 圆锯 | 可选 |
| Spike Head | `Traps/Spike Head/Idle.png`、`Blink (54x52).png`、`Top/Bottom/Left/Right Hit (54x52).png` | 54 × 52 系 | 54 × 52 | 1 / 4 / 各 4 | 刺头机关 | 可选 |
| Spiked Ball | `Traps/Spiked Ball/Spiked Ball.png` / `Chain.png` | 28 × 28 / 8 × 8 | 28 × 28 | 1 / 1 | 刺球 | 可选 |
| Trampoline | `Traps/Trampoline/Idle.png` / `Jump (28x28).png` | 28 × 28 / 224 × 28 | 28 × 28 | 1 / 8 | 蹦床 | 可选 |

## Menu（首版全部不使用）

| 类别 | 内容 | 尺寸 | 首版 |
|---|---|---|---|
| Buttons | 11 个按钮：`Play`、`Settings`、`Volume`、`Levels`、`Achievements`、`Leaderboard`、`Next`、`Previous`、`Restart`、`Back`、`Close` | 21 × 22（Back/Close 为 15 × 16） | 可选 |
| Levels | `Levels/01.png` 到 `50.png`（前导零必须保留） | 19 × 17 | 可选 |
| Text | `Text/Text (Black) (8x10).png`、`Text (White) (8x10).png` | 80 × 50，10 列 × 5 行字符（8 × 10 每字符） | 可选 |

## Other

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 首版 |
|---|---|---:|---:|---|---|
| `Other/Confetti (16x16).png` | 96 × 16 | 16 × 16 | 6 | 庆祝特效 | 可选（胜利表现可用） |
| `Other/Dust Particle.png` | 16 × 16 | 16 × 16 | 1 | 灰尘粒子 | 可选 |
| `Other/Shadow.png` | 16 × 16 | 16 × 16 | 1 | 角色阴影 | 可选 |
| `Other/Transition.png` | 44 × 44 | 44 × 44 | 1 | 场景过渡 | 可选 |

## 首版最小资源集合

| 需求 | 路径 |
|---|---|
| 默认角色 | `Main Characters/Pink Man/` 下七张状态图 |
| 地形 | `Terrain/Terrain (16x16).png` |
| 背景 | `Background/Blue.png` |
| 水果 | `Items/Fruits/Apple.png` |
| 收集特效 | `Items/Fruits/Collected.png`（可用占位效果替代） |
| 起点 | `Items/Checkpoints/Start/Start (Idle).png` |
| 检查点 | `Items/Checkpoints/Checkpoint/Checkpoint (No Flag).png` |
| 终点 | `Items/Checkpoints/End/End (Idle).png` |
| 静态危险 | `Traps/Spikes/Idle.png` |

## 完整版资源映射（《Pink Man 大冒险》三关）

本节只做"系统 → 本文件已登记资产"的映射，不新增资产事实；路径以本文既有表格为准。与既有"首版"列冲突处以本节为准（完整版启用首版未用素材）。

### 敌人三型（出自 `Traps/`，行为见 [../gdd/mechanics.md](../gdd/mechanics.md)，区间见 [../gdd/level-design.md](../gdd/level-design.md)）

| 系统 | 路径 | 用途 |
|---|---|---|
| `rock_head` 巡逻怪 | `Traps/Rock Head/Idle.png`、`Blink (42x42).png`、`Top/Bottom/Left/Right Hit (42x42).png` | 巡逻移动体；Blink 作预警；Hit 作受击/踩杀表现 |
| `saw` 轨道锯 | `Traps/Saw/Off.png`、`On (38x38).png`、`Chain.png` | 轨道往复移动体；Chain 作轨道视觉 |
| `fire` 定时火 | `Traps/Fire/Off.png`、`On (16x32).png`、`Hit (16x32).png` | 熄/燃两态；燃时为危险体 |

### 问号箱（出自 `Items/Boxes/`）

| 系统 | 路径 | 用途 |
|---|---|---|
| 可顶箱子 | `Box1/`、`Box2/`、`Box3/` 的 `Idle.png` 与 `Hit (28x24).png` | 未触发用 Idle；顶后切 Hit；三类箱子分关使用 |
| 箱子碎裂表现 | 各目录 `Break.png` | 顶后可选碎裂表现在关卡结算时使用（可选） |

### 水果（出自 `Items/Fruits/`，分关使用）

| 关卡 | 水果 | 特效 |
|---|---|---|
| `level_01` | `Apple.png` | `Collected.png` |
| `level_02` | `Bananas.png`、`Cherries.png` | `Collected.png` |
| `level_03` | `Melon.png`、`Orange.png` | `Collected.png` |

剩余 `Kiwi.png`、`Pineapple.png`、`Strawberry.png` 为备用，不进入三关布置。

### 菜单（出自 `Menu/`，界面见 [../gdd/ui-hud.md](../gdd/ui-hud.md)）

| 界面元素 | 路径 | 说明 |
|---|---|---|
| 主菜单 Play/选关入口 | `Menu/Buttons/Play.png`、`Levels.png` | Play 进最高解锁关，Levels 进选关 |
| 退出游戏 | `Menu/Buttons/Close.png` | 主菜单与暂停菜单的退出项（素材无 Quit 按钮，以 Close 承担） |
| 返回上级 | `Menu/Buttons/Back.png` | 选关/暂停菜单的返回项 |
| 结算与暂停 | `Menu/Buttons/Next.png`、`Restart.png` | Next 下一关，Restart 重打本关 |
| 选关翻页 | `Menu/Buttons/Previous.png` | 三关一屏时可不用；多屏时启用 |
| 选关缩略图 | `Menu/Levels/01.png`、`02.png`、`03.png` | 未解锁置灰锁定；前导零保留 |
| 菜单文字 | `Menu/Text/Text (White) (8x10).png`、`Text (Black) (8x10).png` | 按钮标签与标题按深浅底选用 |

`Settings`、`Volume`、`Achievements`、`Leaderboard` 本次不用。`Levels/04.png` 到 `50.png` 本次不用。

### 终点与氛围

| 系统 | 路径 | 用途 |
|---|---|---|
| 终点触发 | `Items/Checkpoints/End/End (Pressed) (64x64).png` | 触碰终点后播放一次 |
| 通关庆祝 | `Other/Confetti (16x16).png` | 结算面板表现（可选） |
| 起点/检查点动态 | `Start (Moving) (64x64).png`、`Checkpoint (Flag Out) (64x64).png`、`Checkpoint (Flag Idle)(64x64).png` | 出生与激活过渡（可选，静态图为必需底线） |
| 场景过渡 | `Other/Transition.png` | 菜单与关卡切换过渡（可选） |

各关背景主题（`Background/` 七色）分配见 [../gdd/level-design.md](../gdd/level-design.md)。

### 完整版资源集合

| 需求 | 路径 |
|---|---|
| 默认角色 | `Main Characters/Pink Man/` 下七张状态图 |
| 地形 | `Terrain/Terrain (16x16).png` |
| 背景 | `Background/Blue.png`（L1）+ 两关主题背景（分配见关卡设计） |
| 水果 | `Items/Fruits/` 下 Apple/Bananas/Cherries/Melon/Orange + `Collected.png` |
| 问号箱 | `Items/Boxes/Box1/`、`Box2/`、`Box3/` |
| 起点/检查点/终点 | `Items/Checkpoints/` 下 Start/Checkpoint/End（含 Pressed/Flag 系列） |
| 静态危险 | `Traps/Spikes/Idle.png` |
| 敌人 | `Traps/Rock Head/`、`Traps/Saw/`、`Traps/Fire/` |
| 菜单 | `Menu/Buttons/` 下 Play/Levels/Back/Close/Next/Restart（+Previous 按需）、`Menu/Levels/01-03.png`、`Menu/Text/` |
| 氛围 | `Other/Confetti (16x16).png`、`Other/Transition.png`（可选） |

### 完整版使用检查

- [ ] 完整版资源集合全部被实际引用且可加载；`04-50.png` 与 Settings 系按钮未被引用。
- [ ] 使用的每个路径都能在本文表格中找到，未虚构不存在的文件（含 `Quit.png` 不存在，不得引用）。
- [ ] 敌人动画按各自单帧尺寸切片（Rock Head 42×42、Saw 38×38、Fire 16×32、箱子 28×24）。

## 命名与使用注意

- 路径中的空格、括号、大小写**必须原样保留**；自动化引用时注意转义。
- 易错点：`Desappearing`（非 Disappearing）、`Grey`（非 Gray）、`Checkpoint (Flag Idle)(64x64).png`（`Idle)` 前无空格）、关卡缩略图前导零。
- 素材是只读输入：不要修改或重命名 `assets/` 下的原始文件；如需整理，在工程内复制引用。
- 未列出的分辨率或帧数与表格不符时，以实际图片为准，并在交付说明中记录。

## 资源使用检查

- [ ] 首版最小资源集合全部被实际引用且可加载。
- [ ] 使用的每个路径都能在本文表格中找到，未虚构不存在的文件。
- [ ] 角色动画按 32×32 切片；水果/特效按 32×32；地形图集按 16×16。
- [ ] 未把"文件存在"当作"功能已实现"。
- [ ] 画面中的资源显示符合 [art-direction.md](./art-direction.md)（像素清晰、无拉伸变形）。
