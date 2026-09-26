# 美术方向与呈现方式

本文描述视觉与听觉呈现。逐文件清单见 [asset-catalog.md](./asset-catalog.md)。

## 整体风格

- 低多边形卡通 3D：角色与小怪为 Blender 建模的圆润体块（橙色玩家、蓝紫色小怪，材质见下），竞技场为灰色地面 + 红色装饰柱 + 程序化天空。
- 角色动画为程序化浮动：`Player.tscn` 与 `Mob.tscn` 内各有一段 1.2 秒循环 `float` 动画（位置上下浮动 + 俯仰摇摆），`AnimationPlayer` 自动播放；玩家移动时 `speed_scale = 4`，静止为 1；小怪按随机速度比例缩放。
- 跳跃弧线另有代码倾斜：`rotation.x = PI / 6 * velocity.y / jump_impulse`（以代码为准）。

## 材质（`.tres` 实测）

- `art/body.tres`（玩家体）：橙色（约 0.906, 0.354, 0），`roughness = 0.5`，`cull_mode = 2`，自发光启用（`emission_enabled`）。
- `art/mob_body.tres`（小怪体）：深蓝紫（约 0.059, 0.267, 0.490），`roughness = 0.43`，`cull_mode = 2`。
- `art/eye.tres`、`art/mob_eye.tres`、`art/pupil.tres`：眼部材质（具体色值以文件为准，此处不逐项复述）。
- `.blend` 与 `.glb` 并存：`mob.blend`/`mob.glb`、`player.blend`/`player.glb` 同目录共存；`.blend` 是否为 `.glb` 的导出源未核实，文档仅记录共存事实。

## 字体与 UI

- 字体：`fonts/Montserrat-Medium.ttf`（SIL OFL，见 `fonts/LICENSE.txt`），经 `ui_theme.tres` 设为主题默认字体。
- HUD 为黑色 32 号文字（分数左上）+ 半透明黑全屏重试遮罩（白字居中）。

## 音乐

- `art/House In a Forest Loop.ogg`（HorrorPen，CC-BY 3.0，见仓库 `README.md` 许可节），由 `MusicPlayer.tscn`（`AudioStreamPlayer`，`autoplay = true`）播放，源库配置为 autoload（工作树模板现状见 [../tech/tdd.md](../tech/tdd.md)）。循环点与音量未核实（tscn 无循环/音量覆写）。
