# 界面规格（大厅 / 对战 HUD）

本文规定各界面节点的存在性、文案与显隐时机；规则数值见 [mechanics.md](./mechanics.md)，坐标见 [level-design.md](./level-design.md)。

## 大厅（`lobby.tscn`）

根为全屏 `Control`（`Lobby`），脚本挂在 `LobbyPanel` 上（`logic/lobby.gd`）。

| 节点 | 类型 | 文案/初值 | 显隐 |
|---|---|---|---|
| `Title` | Label（32px，居中） | `Multiplayer Pong` | 常显 |
| `LobbyPanel/AddressLabel` | Label | `Address:` | 常显 |
| `LobbyPanel/Address` | LineEdit | 默认 `127.0.0.1` | 常显 |
| `LobbyPanel/HostButton` | Button | `Host` | 常显；Host/Join 进行中禁用 |
| `LobbyPanel/JoinButton` | Button | `Join` | 常显；Host/Join 进行中禁用 |
| `LobbyPanel/StatusOk` | Label（居中） | 空 | 有成功态文案时显示 |
| `LobbyPanel/StatusFail` | Label（浅红居中） | 空 | 有失败态文案时显示 |
| `LobbyPanel/PortForward` | Label | 端口 8910 UDP 转发提示三行英文 | 仅 Host 成功后可见 |
| `LobbyPanel/FindPublicIP` | LinkButton | `Find your public IP address` | 仅 Host 成功后可见，点击打开 `https://icanhazip.com/` |

状态文案时机（`logic/lobby.gd` 原样）：

| 时机 | 行 | 文案 |
|---|---|---|
| 按 Host 成功 | Ok | `Waiting for player...` |
| 按 Join（合法 IP） | Ok | `Connecting...` |
| Join 地址非法 | Fail | `IP address is invalid.` |
| 连接失败 | Fail | `Couldn't connect.` |
| Host 端口被占 | Fail | `Can't host, address in use.` |
| 对局结束/断线 | Fail | 原因串（`Client disconnected.` / `Server disconnected.` / 空） |

- 成功行与失败行互斥：写一边即清空另一边（`_set_status`）。
- 信号连接：`HostButton.pressed → _on_host_pressed`、`JoinButton.pressed → _on_join_pressed`、`FindPublicIP.pressed → _on_find_public_ip_pressed`。
- 开局后整个 `Lobby` 根隐藏（`hide()`），局后 `_end_game` 再 `show()`。

## 对战 HUD（`pong.tscn`）

| 节点 | 类型 | 文案/初值 | 显隐 |
|---|---|---|---|
| `ScoreLeft` | Label | `0` | 常显 |
| `ScoreRight` | Label | `0` | 常显 |
| `WinnerLeft` | Label | `The Winner!` | 仅左侧到 10 分时显示 |
| `WinnerRight` | Label | `The Winner!` | 仅右侧到 10 分时显示 |
| `ExitGame` | Button | `Exit Game` | 仅胜负产生后显示；`pressed → _on_exit_game_pressed` |
| `Paddle/You`（每拍各一） | Label | `You` | 权限方首次有效输入后隐藏；非权限方首帧隐藏 |

- 背景 `ColorRect`、中线 `Separator`、球拍染色（左青、右品红）见 [level-design.md](./level-design.md) 与 [../art/art-direction.md](../art/art-direction.md)。
- `Winner*` 与 `ExitGame` 平时 `visible=false`，由 `update_score` 在胜负时统一 `show()`。

## 验收提示

- 大厅截图：标题 + 地址 + 双按钮 + 空状态行同框；Host 成功后再截一张（含转发提示与链接）。
- 对战截图：双方比分 + 球 + 双拍同框；胜负后再截一张（胜负标签 + Exit Game）。
