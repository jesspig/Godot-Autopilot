# 游戏设计总纲（GDD）

本文是本 demo 的系统总览与完成定义；规则数值见 [mechanics.md](./mechanics.md)，界面见 [ui-hud.md](./ui-hud.md)，球场几何见 [level-design.md](./level-design.md)。

## 系统总览

| 系统 | 载体 | 职责 |
|---|---|---|
| 大厅 | `lobby.tscn` + `logic/lobby.gd` | 建房/加入、状态提示、开局实例化、局后回收 |
| 对战 | `pong.tscn` + `logic/pong.gd` | 权限分配、记分与胜负、退出事件 |
| 球 | `ball.tscn` + `logic/ball.gd` | 本地运动仿真、分侧判出界、弹跳/重置/停止 |
| 球拍 | `paddle.tscn` + `logic/paddle.gd` | 本地输入移动、位置同步、碰撞触发弹跳 |

- 联机方式：ENet 点对点，默认端口 **8910**，单房间上限 1 个对端（即双人），压缩 `COMPRESS_RANGE_CODER`。
- 权限模型：Host 为 server，Join 方为 client；`Player1`（左拍）归 server，`Player2`（右拍）归 client；球的出界判定按"各管自己一侧"分工（server 判左侧、client 判右侧）。
- 同步方式：球运动**不同步**（两侧各自本地仿真）；球拍位置用 `unreliable` RPC 逐帧同步；得分、弹跳、重置、停止用可靠 `any_peer + call_local` RPC 同步。

## 内容范围

- 大厅 1 个：标题、地址输入（默认 `127.0.0.1`）、Host/Join 按钮、成功/失败状态各一行、端口转发提示（Host 成功后可见）+ 公网 IP 链接。
- 球场 1 个：640×400 深色底、中线、左右球拍、1 球、左右比分、两侧胜负标签（平时隐藏）、Exit Game 按钮（平时隐藏）。
- 无第二球场、无单人模式、无存档，断线即结算回大厅。

## 完成定义

1. 本机双实例可完整走通"建房 → 加入 → 对打 → 10 分胜负 → 退回大厅 → 再开一局"。
2. 所有验收项见 [../test/acceptance.md](../test/acceptance.md)；涉及画面的项以截图为证据。
3. 跨机器与公网路径在单机环境下标记为"未核实"，不计入完成定义（见 [../test/test-plan.md](../test/test-plan.md) 的"单机环境限制"节）。
