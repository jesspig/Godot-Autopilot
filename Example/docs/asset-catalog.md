# PNG 资产目录

本文件是 `Example/assets/` 的唯一资产事实来源。路径均使用 Godot 资源路径 `res://assets/...`，必须保留原始大小写、空格、括号和拼写。

## 权威交叉引用

- 玩法消费规则: [gameplay-spec.md](./gameplay-spec.md)。
- 节点和资源加载关系: [architecture.md](./architecture.md)。
- 首版实际摆放: [level-spec.md](./level-spec.md)。
- 范围和必选内容: [game-overview.md](./game-overview.md)。

## 事实等级

- 本文件中路径、文件存在、PNG 图片尺寸和可由整除关系确定的横向帧数是 `FACT`。
- 消费节点、首版必选或可选是 `TARGET`。
- `Idle`、`On`、`Off`、`Hit`、`Appearing` 等名称只描述文件或视觉状态，不能单独证明运行行为。
- `Sand Mud Ice (16x6).png` 的实际图片是 `176x80`；本文件不把它写成未经验证的固定帧数动画。
- 项目当前没有运行时引用这些 PNG；是否导入成功、SpriteFrames 是否创建、TileSet 是否配置都属于 `VERIFY`。

## 数量总览

| 顶层目录 | 文件数 | 事实范围 |
|---|---:|---|
| `Background/` | 7 | 7 张 64 x 64 背景 |
| `Main Characters/` | 30 | 4 个角色各 7 个状态，加 2 个特效 |
| `Items/` | 25 | 8 种水果、收集特效、3 类箱子、Start、Checkpoint、End |
| `Menu/` | 63 | 11 个按钮、`01.png` 到 `50.png`、2 张文字图集 |
| `Other/` | 4 | Confetti、Dust Particle、Shadow、Transition |
| `Terrain/` | 1 | Terrain 图块图集 |
| `Traps/` | 43 | 13 类陷阱资源 |
| **合计** | **173** | 当前 PNG 目录枚举 |

## Background

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---:|---|---|---|---|
| `res://assets/Background/Blue.png` | 64 x 64 | 64 x 64 | 1 | 蓝色背景 | Background | 必选 | FACT 文件存在，TODO 导入 |
| `res://assets/Background/Brown.png` | 64 x 64 | 64 x 64 | 1 | 棕色背景 | Background | 可选 | FACT |
| `res://assets/Background/Gray.png` | 64 x 64 | 64 x 64 | 1 | 灰色背景 | Background | 可选 | FACT |
| `res://assets/Background/Green.png` | 64 x 64 | 64 x 64 | 1 | 绿色背景 | Background | 可选 | FACT |
| `res://assets/Background/Pink.png` | 64 x 64 | 64 x 64 | 1 | 粉色背景 | Background | 可选 | FACT |
| `res://assets/Background/Purple.png` | 64 x 64 | 64 x 64 | 1 | 紫色背景 | Background | 可选 | FACT |
| `res://assets/Background/Yellow.png` | 64 x 64 | 64 x 64 | 1 | 黄色背景 | Background | 可选 | FACT |

## Main Characters

### 角色状态图集

下表的角色目录集合是完整可枚举集合: `Mask Dude`、`Ninja Frog`、`Pink Man`、`Virtual Guy`。每个状态行与这四个目录各组合一次，正好 28 个实际路径。示例路径使用默认角色 Pink Man；尖括号只是目录模式说明，不是可加载路径。

| 角色目录集合 | 文件名 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---|---:|---:|---:|---|---|---|---|
| `Mask Dude`、`Ninja Frog`、`Pink Man`、`Virtual Guy` | `Idle (32x32).png` | 352 x 32 | 32 x 32 | 11 | 待机 | Player AnimatedSprite2D | Pink Man 必选，其余可选 | FACT |
| 同上 | `Run (32x32).png` | 384 x 32 | 32 x 32 | 12 | 奔跑 | Player AnimatedSprite2D | Pink Man 必选，其余可选 | FACT |
| 同上 | `Jump (32x32).png` | 32 x 32 | 32 x 32 | 1 | 上升 | Player AnimatedSprite2D | Pink Man 必选，其余可选 | FACT |
| 同上 | `Fall (32x32).png` | 32 x 32 | 32 x 32 | 1 | 下落 | Player AnimatedSprite2D | Pink Man 必选，其余可选 | FACT |
| 同上 | `Double Jump (32x32).png` | 192 x 32 | 32 x 32 | 6 | 二段跳视觉 | Player AnimatedSprite2D | Pink Man 必选，其余可选 | FACT |
| 同上 | `Hit (32x32).png` | 224 x 32 | 32 x 32 | 7 | 受击或死亡视觉 | Player AnimatedSprite2D | Pink Man 必选，其余可选 | FACT |
| 同上 | `Wall Jump (32x32).png` | 160 x 32 | 32 x 32 | 5 | 墙跳视觉 | Player AnimatedSprite2D | Pink Man 必选，其余可选 | FACT |

完整实际路径模式:

```text
res://assets/Main Characters/<Mask Dude|Ninja Frog|Pink Man|Virtual Guy>/<上述七个文件名>
```

这是枚举模式，不是实际文件名；实现时只能把尖括号替换为四个已列出的真实目录名。首版默认路径例如 `res://assets/Main Characters/Pink Man/Idle (32x32).png`。

### 角色特效

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---:|---|---|---|---|
| `res://assets/Main Characters/Appearing (96x96).png` | 672 x 96 | 96 x 96 | 7 | 登场视觉 | Player FX | 可选 | FACT |
| `res://assets/Main Characters/Desappearing (96x96).png` | 672 x 96 | 96 x 96 | 7 | 消失视觉 | Player FX | 可选 | FACT，拼写必须是 `Desappearing` |

## Items

### Fruits

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---:|---|---|---|---|
| `res://assets/Items/Fruits/Apple.png` | 544 x 32 | 32 x 32 | 17 | 苹果水果 | Fruit | 必选 | FACT |
| `res://assets/Items/Fruits/Bananas.png` | 544 x 32 | 32 x 32 | 17 | 香蕉水果 | Fruit | 可选 | FACT |
| `res://assets/Items/Fruits/Cherries.png` | 544 x 32 | 32 x 32 | 17 | 樱桃水果 | Fruit | 可选 | FACT |
| `res://assets/Items/Fruits/Kiwi.png` | 544 x 32 | 32 x 32 | 17 | 猕猴桃水果 | Fruit | 可选 | FACT |
| `res://assets/Items/Fruits/Melon.png` | 544 x 32 | 32 x 32 | 17 | 甜瓜水果 | Fruit | 可选 | FACT |
| `res://assets/Items/Fruits/Orange.png` | 544 x 32 | 32 x 32 | 17 | 橙子水果 | Fruit | 可选 | FACT |
| `res://assets/Items/Fruits/Pineapple.png` | 544 x 32 | 32 x 32 | 17 | 菠萝水果 | Fruit | 可选 | FACT |
| `res://assets/Items/Fruits/Strawberry.png` | 544 x 32 | 32 x 32 | 17 | 草莓水果 | Fruit | 可选 | FACT |
| `res://assets/Items/Fruits/Collected.png` | 192 x 32 | 32 x 32 | 6 | 收集视觉 | Fruit FX | 可选 | FACT |

### Boxes

箱子路径按 `Box1`、`Box2`、`Box3` 三个已存在目录与下表三种文件名组合，共 9 个 PNG。首版不实例化箱子。

| 目录集合 | 文件名 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---|---:|---:|---:|---|---|---|---|
| `Box1`、`Box2`、`Box3` | `Idle.png` | 28 x 24 | 28 x 24 | 1 | 箱子待机视觉 | Box | 可选 | FACT |
| `Box1` | `Hit (28x24).png` | 84 x 24 | 28 x 24 | 3 | 箱子被击视觉 | Box | 可选 | FACT |
| `Box2` | `Hit (28x24).png` | 112 x 24 | 28 x 24 | 4 | 箱子被击视觉 | Box | 可选 | FACT |
| `Box3` | `Hit (28x24).png` | 56 x 24 | 28 x 24 | 2 | 箱子被击视觉 | Box | 可选 | FACT |
| `Box1`、`Box2`、`Box3` | `Break.png` | 112 x 24 | 28 x 24 | 4 | 箱子破碎视觉 | Box | 可选 | FACT |

完整路径模式为 `res://assets/Items/Boxes/<Box1|Box2|Box3>/<Idle.png|Hit (28x24).png|Break.png>`，尖括号仅表示上述有限集合。

### Checkpoints

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---:|---|---|---|---|
| `res://assets/Items/Checkpoints/Start/Start (Idle).png` | 64 x 64 | 64 x 64 | 1 | 起点静态视觉 | Start | 必选 | FACT |
| `res://assets/Items/Checkpoints/Start/Start (Moving) (64x64).png` | 1088 x 64 | 64 x 64 | 17 | 起点视觉状态 | Start | 可选 | FACT |
| `res://assets/Items/Checkpoints/Checkpoint/Checkpoint (No Flag).png` | 64 x 64 | 64 x 64 | 1 | 未激活检查点视觉 | Checkpoint | 必选 | FACT |
| `res://assets/Items/Checkpoints/Checkpoint/Checkpoint (Flag Out) (64x64).png` | 1664 x 64 | 64 x 64 | 26 | 激活过渡视觉 | Checkpoint | 可选 | FACT |
| `res://assets/Items/Checkpoints/Checkpoint/Checkpoint (Flag Idle)(64x64).png` | 640 x 64 | 64 x 64 | 10 | 已激活循环视觉 | Checkpoint | 可选 | FACT，注意 `Idle)` 前没有空格 |
| `res://assets/Items/Checkpoints/End/End (Idle).png` | 64 x 64 | 64 x 64 | 1 | 终点静态视觉 | End | 必选 | FACT |
| `res://assets/Items/Checkpoints/End/End (Pressed) (64x64).png` | 512 x 64 | 64 x 64 | 8 | 终点触发视觉 | End | 可选 | FACT |

文件名中的视觉状态不能替代 `Checkpoint` 或 `End` 脚本逻辑。

## Terrain

| 路径 | 图片尺寸 | 单帧 | 网格 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---|---|---|---|---|
| `res://assets/Terrain/Terrain (16x16).png` | 352 x 176 | 16 x 16 | 22 列 x 11 行 | Terrain atlas | TileSet 与 TileMapLayer | 必选 | FACT |

图集包含 242 个网格位置，但每个位置的地形语义和碰撞必须在 Godot TileSet 中逐项确认。不要因为旧文档的颜色描述而推断某个 tile 的碰撞。

## Traps

下表覆盖 13 个陷阱目录和 43 个 PNG。行为列只描述资产可支持的视觉用途；首版只有 Spikes 作为实际危险，其他行为仍是 `TARGET` 或 `VERIFY`。

| 类别 | 路径或完整文件集合 | 图片尺寸 | 单帧 | 帧数 | 视觉语义 | 消费节点 | 首版 | 状态 |
|---|---|---:|---:|---:|---|---|---|---|
| Arrow | `res://assets/Traps/Arrow/Idle (18x18).png` | 180 x 18 | 18 x 18 | 10 | Idle 视觉 | Arrow | 可选 | FACT |
| Arrow | `res://assets/Traps/Arrow/Hit (18x18).png` | 72 x 18 | 18 x 18 | 4 | Hit 视觉 | Arrow | 可选 | FACT |
| Blocks | `res://assets/Traps/Blocks/Idle.png` | 22 x 22 | 22 x 22 | 1 | 待机视觉 | Blocks | 可选 | FACT |
| Blocks | `res://assets/Traps/Blocks/Part 1 (22x22).png` | 66 x 22 | 22 x 22 | 3 | Part 1 视觉 | Blocks | 可选 | FACT |
| Blocks | `res://assets/Traps/Blocks/Part 2 (22x22).png` | 66 x 22 | 22 x 22 | 3 | Part 2 视觉 | Blocks | 可选 | FACT |
| Blocks | `res://assets/Traps/Blocks/HitTop (22x22).png` | 66 x 22 | 22 x 22 | 3 | 顶部 Hit 视觉 | Blocks | 可选 | FACT |
| Blocks | `res://assets/Traps/Blocks/HitSide (22x22).png` | 66 x 22 | 22 x 22 | 3 | 侧面 Hit 视觉 | Blocks | 可选 | FACT |
| Falling Platforms | `res://assets/Traps/Falling Platforms/Off.png` | 32 x 10 | 32 x 10 | 1 | Off 视觉 | FallingPlatform | 可选 | FACT |
| Falling Platforms | `res://assets/Traps/Falling Platforms/On (32x10).png` | 128 x 10 | 32 x 10 | 4 | On 视觉 | FallingPlatform | 可选 | FACT |
| Fan | `res://assets/Traps/Fan/Off.png` | 24 x 8 | 24 x 8 | 1 | Off 视觉 | Fan | 可选 | FACT |
| Fan | `res://assets/Traps/Fan/On (24x8).png` | 96 x 8 | 24 x 8 | 4 | On 视觉 | Fan | 可选 | FACT |
| Fire | `res://assets/Traps/Fire/Off.png` | 16 x 32 | 16 x 32 | 1 | Off 视觉 | Fire | 可选 | FACT |
| Fire | `res://assets/Traps/Fire/On (16x32).png` | 48 x 32 | 16 x 32 | 3 | On 视觉 | Fire | 可选 | FACT |
| Fire | `res://assets/Traps/Fire/Hit (16x32).png` | 64 x 32 | 16 x 32 | 4 | Hit 视觉 | Fire | 可选 | FACT |
| Platforms | `res://assets/Traps/Platforms/Brown Off.png` | 32 x 8 | 32 x 8 | 1 | 棕色 Off 视觉 | MovingPlatform | 可选 | FACT |
| Platforms | `res://assets/Traps/Platforms/Brown On (32x8).png` | 256 x 8 | 32 x 8 | 8 | 棕色 On 视觉 | MovingPlatform | 可选 | FACT |
| Platforms | `res://assets/Traps/Platforms/Grey Off.png` | 32 x 8 | 32 x 8 | 1 | 灰色 Off 视觉 | MovingPlatform | 可选 | FACT，拼写是 `Grey` |
| Platforms | `res://assets/Traps/Platforms/Grey On (32x8).png` | 256 x 8 | 32 x 8 | 8 | 灰色 On 视觉 | MovingPlatform | 可选 | FACT |
| Platforms | `res://assets/Traps/Platforms/Chain.png` | 8 x 8 | 8 x 8 | 1 | 链条视觉 | MovingPlatform | 可选 | FACT |
| Rock Head | `res://assets/Traps/Rock Head/Idle.png` | 42 x 42 | 42 x 42 | 1 | 待机视觉 | RockHead | 可选 | FACT |
| Rock Head | `res://assets/Traps/Rock Head/Blink (42x42).png` | 168 x 42 | 42 x 42 | 4 | Blink 视觉 | RockHead | 可选 | FACT |
| Rock Head | `res://assets/Traps/Rock Head/Top Hit (42x42).png` | 168 x 42 | 42 x 42 | 4 | Top Hit 视觉 | RockHead | 可选 | FACT |
| Rock Head | `res://assets/Traps/Rock Head/Bottom Hit (42x42).png` | 168 x 42 | 42 x 42 | 4 | Bottom Hit 视觉 | RockHead | 可选 | FACT |
| Rock Head | `res://assets/Traps/Rock Head/Left Hit (42x42).png` | 168 x 42 | 42 x 42 | 4 | Left Hit 视觉 | RockHead | 可选 | FACT |
| Rock Head | `res://assets/Traps/Rock Head/Right Hit (42x42).png` | 168 x 42 | 42 x 42 | 4 | Right Hit 视觉 | RockHead | 可选 | FACT |
| Sand Mud Ice | `res://assets/Traps/Sand Mud Ice/Sand Mud Ice (16x6).png` | 176 x 80 | 文件名标记 16 x 6 | 不宣称 | 沙泥冰图集，帧布局待确认 | Surface | 可选 | FACT 尺寸，VERIFY 切片 |
| Sand Mud Ice | `res://assets/Traps/Sand Mud Ice/Sand Particle.png` | 16 x 16 | 16 x 16 | 1 | 沙粒子视觉 | Surface FX | 可选 | FACT |
| Sand Mud Ice | `res://assets/Traps/Sand Mud Ice/Mud Particle.png` | 16 x 16 | 16 x 16 | 1 | 泥粒子视觉 | Surface FX | 可选 | FACT |
| Sand Mud Ice | `res://assets/Traps/Sand Mud Ice/Ice Particle.png` | 16 x 16 | 16 x 16 | 1 | 冰粒子视觉 | Surface FX | 可选 | FACT |
| Saw | `res://assets/Traps/Saw/Off.png` | 38 x 38 | 38 x 38 | 1 | Off 视觉 | Saw | 可选 | FACT |
| Saw | `res://assets/Traps/Saw/On (38x38).png` | 304 x 38 | 38 x 38 | 8 | On 视觉 | Saw | 可选 | FACT |
| Saw | `res://assets/Traps/Saw/Chain.png` | 8 x 8 | 8 x 8 | 1 | 链条视觉 | Saw | 可选 | FACT |
| Spike Head | `res://assets/Traps/Spike Head/Idle.png` | 54 x 52 | 54 x 52 | 1 | 待机视觉 | SpikeHead | 可选 | FACT |
| Spike Head | `res://assets/Traps/Spike Head/Blink (54x52).png` | 216 x 52 | 54 x 52 | 4 | Blink 视觉 | SpikeHead | 可选 | FACT |
| Spike Head | `res://assets/Traps/Spike Head/Top Hit (54x52).png` | 216 x 52 | 54 x 52 | 4 | Top Hit 视觉 | SpikeHead | 可选 | FACT |
| Spike Head | `res://assets/Traps/Spike Head/Bottom Hit (54x52).png` | 216 x 52 | 54 x 52 | 4 | Bottom Hit 视觉 | SpikeHead | 可选 | FACT |
| Spike Head | `res://assets/Traps/Spike Head/Left Hit (54x52).png` | 216 x 52 | 54 x 52 | 4 | Left Hit 视觉 | SpikeHead | 可选 | FACT |
| Spike Head | `res://assets/Traps/Spike Head/Right Hit (54x52).png` | 216 x 52 | 54 x 52 | 4 | Right Hit 视觉 | SpikeHead | 可选 | FACT |
| Spiked Ball | `res://assets/Traps/Spiked Ball/Spiked Ball.png` | 28 x 28 | 28 x 28 | 1 | 刺球视觉 | SpikedBall | 可选 | FACT |
| Spiked Ball | `res://assets/Traps/Spiked Ball/Chain.png` | 8 x 8 | 8 x 8 | 1 | 链条视觉 | SpikedBall | 可选 | FACT |
| Spikes | `res://assets/Traps/Spikes/Idle.png` | 16 x 16 | 16 x 16 | 1 | 静态尖刺视觉 | Spike | 必选 | FACT |
| Trampoline | `res://assets/Traps/Trampoline/Idle.png` | 28 x 28 | 28 x 28 | 1 | 待机视觉 | Trampoline | 可选 | FACT |
| Trampoline | `res://assets/Traps/Trampoline/Jump (28x28).png` | 224 x 28 | 28 x 28 | 8 | Jump 视觉 | Trampoline | 可选 | FACT |

## Menu

### Buttons

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---:|---|---|---|---|
| `res://assets/Menu/Buttons/Play.png` | 21 x 22 | 21 x 22 | 1 | Play 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Settings.png` | 21 x 22 | 21 x 22 | 1 | Settings 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Volume.png` | 21 x 22 | 21 x 22 | 1 | Volume 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Levels.png` | 21 x 22 | 21 x 22 | 1 | Levels 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Achievements.png` | 21 x 22 | 21 x 22 | 1 | Achievements 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Leaderboard.png` | 21 x 22 | 21 x 22 | 1 | Leaderboard 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Next.png` | 21 x 22 | 21 x 22 | 1 | Next 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Previous.png` | 21 x 22 | 21 x 22 | 1 | Previous 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Restart.png` | 21 x 22 | 21 x 22 | 1 | Restart 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Back.png` | 15 x 16 | 15 x 16 | 1 | Back 按钮视觉 | Menu | 可选 | FACT |
| `res://assets/Menu/Buttons/Close.png` | 15 x 16 | 15 x 16 | 1 | Close 按钮视觉 | Menu | 可选 | FACT |

### Levels

完整路径集合为 `res://assets/Menu/Levels/01.png`、`02.png`、`03.png`、`04.png`、`05.png`、`06.png`、`07.png`、`08.png`、`09.png`、`10.png`、`11.png`、`12.png`、`13.png`、`14.png`、`15.png`、`16.png`、`17.png`、`18.png`、`19.png`、`20.png`、`21.png`、`22.png`、`23.png`、`24.png`、`25.png`、`26.png`、`27.png`、`28.png`、`29.png`、`30.png`、`31.png`、`32.png`、`33.png`、`34.png`、`35.png`、`36.png`、`37.png`、`38.png`、`39.png`、`40.png`、`41.png`、`42.png`、`43.png`、`44.png`、`45.png`、`46.png`、`47.png`、`48.png`、`49.png`、`50.png`。这 50 个路径均为 FACT，首版不实现关卡选择，只把它们标为可选。

| 路径模式 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---:|---|---|---|---|
| `res://assets/Menu/Levels/01.png` 到 `res://assets/Menu/Levels/50.png` | 19 x 17 | 19 x 17 | 1 | 关卡缩略图 | Level Menu | 可选 | FACT，前导零必须保留 |

### Text

| 路径 | 图片尺寸 | 单字符 | 字符布局 | 字符数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---|---:|---|---|---|---|
| `res://assets/Menu/Text/Text (Black) (8x10).png` | 80 x 50 | 8 x 10 | 10 列 x 5 行 | 50 | 黑色文字图集 | 可选 Bitmap UI | 可选 | FACT |
| `res://assets/Menu/Text/Text (White) (8x10).png` | 80 x 50 | 8 x 10 | 10 列 x 5 行 | 50 | 白色文字图集 | 可选 Bitmap UI | 可选 | FACT |

## Other

| 路径 | 图片尺寸 | 单帧 | 帧数 | 语义 | 消费节点 | 首版 | 状态 |
|---|---:|---:|---:|---|---|---|---|
| `res://assets/Other/Confetti (16x16).png` | 96 x 16 | 16 x 16 | 6 | 庆祝视觉 | Victory FX | 可选 | FACT |
| `res://assets/Other/Dust Particle.png` | 16 x 16 | 16 x 16 | 1 | 灰尘视觉 | Player FX | 可选 | FACT |
| `res://assets/Other/Shadow.png` | 16 x 16 | 16 x 16 | 1 | 阴影视觉 | Player FX | 可选 | FACT |
| `res://assets/Other/Transition.png` | 44 x 44 | 44 x 44 | 1 | 过渡视觉 | Scene FX | 可选 | FACT |

## 首版最小资源集合

| 需求 | 必选路径 |
|---|---|
| 默认角色 | `res://assets/Main Characters/Pink Man/` 下七个状态图 |
| 地形 | `res://assets/Terrain/Terrain (16x16).png` |
| 背景 | `res://assets/Background/Blue.png` |
| 水果 | `res://assets/Items/Fruits/Apple.png` |
| 收集视觉 | `res://assets/Items/Fruits/Collected.png`，可先用占位 Label 验证逻辑 |
| 起点 | `res://assets/Items/Checkpoints/Start/Start (Idle).png` |
| 检查点 | `res://assets/Items/Checkpoints/Checkpoint/Checkpoint (No Flag).png` |
| 终点 | `res://assets/Items/Checkpoints/End/End (Idle).png` |
| 静态危险 | `res://assets/Traps/Spikes/Idle.png` |

## 导入与切片验收

- [ ] Godot FileSystem 可以逐项打开首版最小资源集合。
- [ ] Pink Man 的七张图均按 `32 x 32` 切片，帧数分别为 11、12、1、1、6、7、5。
- [ ] Apple 按 `32 x 32` 切片为 17 帧。
- [ ] `Collected.png` 按 `32 x 32` 切片为 6 帧。
- [ ] Terrain 图集按 `16 x 16` 创建 Atlas source，图片尺寸是 `352 x 176`。
- [ ] `Sand Mud Ice (16x6).png` 不按未经验证的固定帧数动画导入；切片布局必须先在 Godot 中 VERIFY。
- [ ] 所有 Checkpoint 路径保留原始空格和括号，尤其是 `Checkpoint (Flag Idle)(64x64).png`。
- [ ] `Grey`、`Desappearing` 和关卡缩略图前导零未被改写。
- [ ] 没有因为 PNG 文件存在就把对应功能误标为已实现。
