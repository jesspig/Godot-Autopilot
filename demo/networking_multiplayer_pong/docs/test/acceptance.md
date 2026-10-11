# 验收标准汇总

顶层场景全过（截图为证）即通过；分项数值与行为定义见 [../gdd/mechanics.md](../gdd/mechanics.md)。

## 顶层场景

### A. 大厅可用

- [ ] 主场景打开为大厅：标题 `Multiplayer Pong`、地址默认 `127.0.0.1`、Host/Join 可点（编辑器截图）。
- [ ] Host 显示 `Waiting for player...` + 转发提示 + 公网 IP 链接（运行截图）。
- [ ] Join 非法 IP 显示 `IP address is invalid.` 且不建 peer（运行截图）。
- [ ] 连接失败显示 `Couldn't connect.` 且按钮恢复可用（有条件：需无 server 时 Join，运行截图）。

### B. 双实例对战（本机回环）

- [ ] B Join `127.0.0.1` 后两端同时进入球场、大厅隐藏（双端运行截图）。
- [ ] A 只动左拍、B 只动右拍，对方拍不受本端输入影响（行为记录 + 截图）。
- [ ] 球上下边反弹、过拍加速（行为记录；加速数值以 mechanics 为准）。
- [ ] 左出界右分 +1、右出界左分 +1，两端比分一致（双端截图对比）。

### C. 胜负与退出

- [ ] 先到 10 分：同侧 `The Winner!` + `Exit Game` 显示、球停住（运行截图）。
- [ ] 按 Exit Game：Pong 回收、大厅重现、Host/Join 恢复可用（运行截图）。
- [ ] 再 Host 一局可正常开局（闭环记录）。

### D. 断线

- [ ] 任一端关闭/掉线，另一端回大厅并显示原因（`Client disconnected.` / `Server disconnected.`，运行截图）。
- [ ] 回大厅后按钮可用，可再开一局（闭环记录）。

### E. 视觉验收

- [ ] 大厅布局与文案同 [../gdd/ui-hud.md](../gdd/ui-hud.md)（编辑器 + 运行截图）。
- [ ] 球场摆位同 [../gdd/level-design.md](../gdd/level-design.md)：背景铺满、双拍染色、比分 `0`（编辑器截图）。
- [ ] 官方截图 `screenshots/pong_multiplayer.png` 与实测画面风格一致（目视对比，记录结论）。

## 单机未核实项（不计入通过/失败）

- 跨机器联机、端口转发、`Find your public IP address` 外链跳转、高延迟手感。

## 分项索引

| 主题 | 去处 |
|---|---|
| 端口/胜分/速度/权限/记分方向 | [../gdd/mechanics.md](../gdd/mechanics.md) |
| 文案与显隐时机 | [../gdd/ui-hud.md](../gdd/ui-hud.md) |
| 坐标与验收路线 | [../gdd/level-design.md](../gdd/level-design.md) |
| 资源清单 | [../art/asset-catalog.md](../art/asset-catalog.md) |
| 配置恢复前置、RPC 表 | [../tech/tdd.md](../tech/tdd.md) |
| 环、证据、问题格式 | [./test-plan.md](./test-plan.md) |
