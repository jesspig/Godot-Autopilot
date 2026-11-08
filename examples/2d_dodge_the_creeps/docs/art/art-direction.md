# 美术方向实录

## 风格

Kenney “Abstract Platformer” 几何扁平风：高饱和角色 + 深青灰底（`ColorRect` 0.22/0.37/0.38），配 Xolonium 大字号 HUD（60pt），竖屏街机感。角色为 0.5 缩放、敌人 0.75 缩放的像素风小精灵，配白色渐隐移动拖尾（`Trail` 粒子）。

## 角色呈现

- 玩家：灰色人体 `playerGrey_*`，横向 `right` / 纵向 `up` 两套两帧动画，5 fps；左右按 `flip_h` 镜像，下移整体旋转 PI。
- 敌人：飞 / 游 / 走三套两帧动画（3 / 4 / 4 fps），每只出生随机选一套，全程不变。

## 场景呈现

- 背景为纯色 `ColorRect` 全屏，无视差、无贴图。
- HUD 文字全部 Xolonium 60pt：顶部分数、中央消息、底部 Start 按钮。
- 参考观感以 `screenshots/dodge.png` 为准。

## 音频方向

- 背景乐：`art/House In a Forest Loop.ogg`，开局播、结束停，可循环（Loop 命名，进编辑器核对循环设置——未核实导入属性）。
- 死亡音：`art/gameover.wav`，`game_over()` 时单次播放。

## 授权（转录自 README.md）

- 图片出自 kenney.nl “Abstract Platformer”（2016），CC0 1.0。
- 背景乐 © 2012 HorrorPen，CC-BY 3.0。
- 字体 Xolonium © 2011–2016 Severin Meyer，SIL OFL 1.1（见 `fonts/LICENSE.txt`）。
