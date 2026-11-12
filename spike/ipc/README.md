# GDA IPC Spike

T-02 的最小实验，用于确认 Bun `node:net` 与 C++ 服务端在 Windows 命名管道、POSIX AF_UNIX、loopback TCP 上的互通性。本目录不参与生产构建。

## 结构

- `server.cpp`：C++17 回显服务端，支持 `pipe`（Windows）、`unix`（POSIX）、`tcp`。
- `client.ts`：Bun 客户端，发送 4 字节大端长度前缀 + JSON/二进制帧，校验回显。
- `run-spike.ps1`：Windows 构建与运行脚本。
- `run-linux.sh`：Linux/WSL 构建与运行脚本。
- `CMakeLists.txt`：仅用于本 spike 的独立构建。

## 帧格式

- 4 字节大端无符号长度 + UTF-8/二进制负载。
- 单帧上限 4 MiB（`4194304` 字节）。
- 服务端按连接串行读取与回显，连接之间并发。

## Windows

```powershell
cd spike/ipc
./run-spike.ps1 -Mode all
```

产物：`client-pipe.json`、`client-tcp.json`、`server-pipe.log`、`server-tcp.log`。

## Linux

```bash
cd spike/ipc
bash run-linux.sh
```

产物：`client-unix.json`、`client-tcp.json`、`server-unix.log`、`server-tcp.log`。

## 结果摘要

| 平台 | 后端 | 4 MiB 往返 | 10 并发 64 KiB | 结论 |
| --- | --- | --- | --- | --- |
| Windows 11 x64 | 命名管道 | 32 ms | 通过 | 首选 |
| Windows 11 x64 | loopback TCP | 14 ms | 通过 | 回退 |
| Linux (WSL2) x64 | AF_UNIX | 28 ms | 通过 | 首选 |
| Linux (WSL2) x64 | loopback TCP | 16 ms | 通过 | 回退 |
| macOS arm64/x64 | AF_UNIX | 未在本机实测 | 未在本机实测 | 代码路径同 Linux，待 CI 矩阵验证 |

- Bun 版本：Windows 1.4.2；Linux 1.4.2。
- 所有用例均校验回显字节完全一致；客户端在 `socket closed` 时退出非零。
- 4 MiB 帧在命名管道与 UDS 上均未分片失败；TCP 同样通过。
- 断连由服务端 `CLOSED` 日志确认；客户端关闭后服务端释放连接。
- 详细日志与机器可读结果见 `.handoff/evidence/T-02/`。
