# 资源清单（逐文件枚举）

枚举以磁盘目录为准。`.import` 为引擎导入文件，不计入资源数，仅备注存在。

## art/：12 个资源文件

### 玩家贴图（4 张 PNG）

| 文件 | 用途 |
|---|---|
| `art/playerGrey_walk1.png` | 玩家 `right` 第 1 帧；复用作 `Trail` 粒子贴图 |
| `art/playerGrey_walk2.png` | 玩家 `right` 第 2 帧 |
| `art/playerGrey_up1.png` | 玩家 `up` 第 1 帧 |
| `art/playerGrey_up2.png` | 玩家 `up` 第 2 帧 |

### 敌人贴图（6 张 PNG）

| 文件 | 用途 |
|---|---|
| `art/enemyFlyingAlt_1.png` | 敌人 `fly` 第 1 帧 |
| `art/enemyFlyingAlt_2.png` | 敌人 `fly` 第 2 帧 |
| `art/enemySwimming_1.png` | 敌人 `swim` 第 1 帧 |
| `art/enemySwimming_2.png` | 敌人 `swim` 第 2 帧 |
| `art/enemyWalking_1.png` | 敌人 `walk` 第 1 帧 |
| `art/enemyWalking_2.png` | 敌人 `walk` 第 2 帧 |

### 音频（2 个）

| 文件 | 用途 |
|---|---|
| `art/House In a Forest Loop.ogg` | 背景音乐（`Main/Music`） |
| `art/gameover.wav` | 死亡音效（`Main/DeathSound`） |

每个 PNG/音频旁均有同名 `.import` 文件（共 12 个）。

## fonts/：3 个文件

| 文件 | 说明 |
|---|---|
| `fonts/Xolonium-Regular.ttf` | HUD 唯一字体（分数/消息/按钮均 60pt），另有 `Xolonium-Regular.ttf.import` |
| `fonts/FONTLOG.txt` | 字体日志（内容未核实，未打开） |
| `fonts/LICENSE.txt` | Xolonium 授权文本（README 称 SIL OFL 1.1） |

## 其他

| 文件 | 说明 |
|---|---|
| `icon.webp`（+ `icon.webp.import`） | 工程图标（`project.godot` `config/icon` 指向它） |
| `screenshots/dodge.png` | 官方截图（观感参照） |
| `screenshots/.gdignore` | 截图目录忽略标记 |
| `README.md` | 官方英文说明（含授权与 Asset Store 链接） |
| `LICENSE` | 仓库内授权文件（内容未核实，未打开） |

## 引用完整性

- `player.tscn` 引用 4 张玩家 PNG；`mob.tscn` 引用 6 张敌人 PNG；`main.tscn` 引用 2 个音频；`hud.tscn` 引用字体 TTF。以上引用路径均在磁盘存在。
- 未发现悬空引用（以 tscn 文本核对为准）。
