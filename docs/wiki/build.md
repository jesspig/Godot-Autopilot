---
type: 构建部署指南
title: 构建体系
description: 构建/部署/打包流程、CMake 模块职责、产物清单与环境变量
tags:
  - 构建
  - CMake
  - 部署
timestamp: "2026-11-17T02:19:35+08:00"
resource:
  - CMakeLists.txt
  - CMakePresets.json
  - devtools/
  - cmake/
  - third_party/
---

# 构建体系（build）

> 审计日期：2026-11-17（Game Jam v2；构建、部署、打包命令与产物以本页「命令速查表」为准）

## 命令速查表

| 命令 | 行为 |
|---|---|
| `bun run gda-dev-tools build` | Debug：配置 + 构建（不部署） |
| `bun run gda-dev-tools build --release` | Release 配置 + 构建（不部署、不清理） |
| `bun run gda-dev-tools deploy` | 部署 Debug 产物到 `--project <dir>` 指定的 Godot 工程（可重复）；不传 `--project` 时递归发现 `examples/` 下的比赛最终项目，跳过 intake 与 e2e 沙盒，未发现则报错退出 |
| `bun run gda-dev-tools deploy --release` | 先清理目标工程，再部署 Release 产物 |
| `bun run gda-dev-tools package` | 仅打包本地 Debug 构建产物（默认 `build/debug`，不触发构建，产物缺失则报错退出）；包内含 6 个 C# 桥 `.cs`、**不含 `.pdb`** |
| `bun run gda-dev-tools package --preset release` | 打包本地 Release 构建产物（`build/release`）；包内含 6 个 C# 桥 `.cs`、**不含 `.pdb`** |
| `bun run gda-dev-tools package --libs-dir <dir>` | 从 `<dir>` 递归收集三平台库合并后打包（CI Release 用该路径；缺失任一平台库即报错退出）；包内含 6 个 C# 桥 `.cs`、**不含 `.pdb`** |
| `bun run gda-dev-tools package-check <zip>` | 校验发布包完整性：ZIP 中央目录偏移合法、三平台库齐全、6 个 C# 桥源文件齐全、`.gdextension` 的 `[libraries]` 条目一一对应、包内不含 `.pdb`；失败退出 1。仅适用于三平台发布包，`package --preset` 产出的单平台包会因缺另两平台库而失败 |
| `bun run gda-dev-tools build --debug` | 显式 Debug（默认即为 Debug） |
| `bun run gda-dev-tools clean` | 清理 `--project <dir>` 指定工程（不传则递归发现 `examples/` 下的比赛最终项目，跳过 intake 与 e2e 沙盒）的 `.godot` 与 `addons/godot-autopilot` |
| `bun run gda-dev-tools test` | 跑测试（全量默认门 = 无过滤 ctest：单元 + 边界 + 集成 + 守卫 + CLI；`--single <用例名>` 单跑，`--preset` 切预设） |
| `bun run gda-dev-tools jam` | Game Jam 抽题：生成/刷新 `examples/<theme_id>/project/` intake；`jam create` 创建比赛最终项目，`jam e2e` 创建/刷新/重置固定沙盒 `examples/e2e`，`jam list [--projects]` 查看题库/比赛项目，`jam index` 刷新比赛索引。详见 [gamejam.md](gamejam.md) |
| `bun run gda-dev-tools tui` | 交互式构建控制台（`@clack/prompts`：方向键选择、确认框与路径补全；覆盖构建/部署/打包/清理/测试/状态查看；部署与清理输入 Godot 工程路径；非 TTY 提示并退出 2） |
| `cmake --preset debug && cmake --build --preset debug` | 手动构建（不部署）。预设名不带 `-portable` 后缀即**固定 Ninja**，须本机 PATH 有 ninja；无 ninja 时改用 `debug-portable` / `release-portable`，或直接用 `bun run gda-dev-tools build` 由脚本自动选路（见「预设」小节） |

约束：`build`/`deploy` 各自的 `--release` 与 `--debug` 互斥，同时指定报错退出（`devtools/main.ts` 参数解析）；`--project` 可重复，路径必须包含 `project.godot`；`package` 的 `--preset` 与 `--libs-dir` 互斥，`--libs-dir` 缺任一平台库即报错退出。

## Game Jam 生成器（比赛开发与 e2e 沙盒）

抽题服务两类流程：比赛开发用 `jam` + `jam create` 创建并保留项目；端到端测试用 `jam e2e` 管理固定沙盒 `examples/e2e`。程序化测试不依赖抽题或 `examples/` 生成物。

- 数据、抽样、创意约束、intake 与比赛项目、固定 e2e 沙盒、提交要求（以 `GAMEJAM.md` 为准）、本地收藏与自动发现规则，统一见 [gamejam.md](gamejam.md)。
- 命令入口：`bun run gda-dev-tools jam`（抽题）、`jam create`（创建比赛最终项目）、`jam e2e [--reset]`（管理固定沙盒）、`jam list [--projects]`、`jam index`。
- `deploy`/`clean` 未传 `--project` 时递归发现 `examples/` 下的比赛最终项目，跳过 intake 与 e2e 沙盒；e2e 始终显式传 `--project examples/e2e`。

## 版本号单一来源（根 `VERSION` 文件）

版本号只在根目录 `VERSION` 文件维护一处，其余全部派生：

| 消费方 | 方式 |
|---|---|
| `CMakeLists.txt` | `project()` 前 `file(READ ...)` 读入并 `string(STRIP)`；取 `major.minor.patch` 数字前缀作为 `project(... VERSION ...)` 实参，完整字符串（可带 `-dev` 等预发布后缀）保留为 `GDA_VERSION` |
| C++（MCP `server_info`、`system_status.version`） | `configure_file(src/core/version.hpp.in → <build>/generated/version.hpp @ONLY)` 用完整 `GDA_VERSION` 生成字符串宏；`server_context.cpp` / `register_all.cpp` 包含 `<version.hpp>` 引用 |
| `devtools/lib/config.ts` | `ADDON_VERSION = fs.readFileSync(path.join(PROJECT_ROOT, "VERSION"), "utf8").trim()`，用于打包产物名 |

升版流程：递增规则见 [`AGENTS.md`](../../AGENTS.md)「版本号规则」（固定 `0.y.x`，默认只增 `x`；只有用户明确要求才增 `y`）；同步根 `VERSION`、`cli/src/version.ts` 与 `cli/assets/skills.json`，随后重新 configure。发布前去掉 `-dev` 后缀需用户明确许可。

开发期 `VERSION` 可带 `-dev` 后缀（当前 `0.3.4-dev`），`GDA_VERSION`、CLI `CLI_VERSION` 与 `gda status` 原样显示该后缀，用于区分开发版与正式版。`.github/workflows/release.yml` 的 validate 会校验 tag 与 `VERSION` 完全一致，带后缀无法发布。

### 版本与 LTO 的单一事实源与 CI 断言

版本与 LTO 各只有一处决策点，CI 侧由断言兜底（均在 `.github/workflows/release.yml`）：

- **版本**：唯一事实源为根 `VERSION` 文件（上表）。validate 作业校验 tag 与 `VERSION` 一致（`v` 前缀可选；`workflow_dispatch` 的 tag 输入经步骤级 `env: TAG_NAME` 间接取用，不直接内插进 `run:` 脚本，消解注入面）；cli 作业 `bun build --compile` 出二进制后立即执行 `gda --version`，把输出第二段与 `VERSION` 比对，不一致报 `::error` 并中止发布——防止二进制内嵌版本（`cli/src/version.ts` 的 `CLI_VERSION`，由 `version_sync_guard` 与 `VERSION` 对齐）漂移。
- **LTO**：唯一决策点为 `cmake/Lto.cmake`（优先级 Clang ThinLTO > MSVC LTCG > GCC IPO；非 Release 静默跳过，见「cmake/ 模块职责」表）。CI 与本地同走 `bun run gda-dev-tools build --release`，构建口径一致；release 作业的 Release 构建把日志 `tee` 到 `build-release.log` 并 `grep -E '\[gda\] LTO: (thin|LTCG|full)'`，未命中报 `::error` 并失败——防止 Release 构建静默退化为无 LTO。Debug 侧不加 skipped 断言（非 Release 静默 return 无日志可断）。
- **workflow 权限**：两 workflow 顶层声明 `permissions: contents: read`（最小权限基线），仅 release 的 package 作业按发布需要作业级覆盖为 `contents: write`。

## 构建流水线（devtools/）

### 1. 清理（`deploy --release` 顺带，或 `clean` 子命令）

`cleanTargets(projects)`（`devtools/commands/clean.ts`）按目标工程逐个删除两类目录，**不触碰 `build/`**：

- `<工程>/.godot` — Godot 引擎缓存
- `<工程>/addons/godot-autopilot/` — 旧部署产物

目标由 `resolveProjects()`（`devtools/lib/config.ts`）解析：`--project` 传入的路径优先，未传入时递归发现 `examples/` 下含 `project.godot` 的最终项目（跳过 `jam.json.kind=intake` 的 intake）；两者都没有则报错退出。清理刻意与部署同口径，否则会留下目标工程已是 Release 库的幽灵态。

### 2. 配置 `configure(preset)`（`devtools/lib/cmake.ts`）

先由 [`devtools/lib/config.ts:resolvePreset()`](../../devtools/lib/config.ts) 把逻辑预设名 `debug|release` 解析为实际预设名（按 ninja 可用性在 `<preset>` 与 `<preset>-portable` 间二选一，两条路径共用 `build/<preset>/`），随后：

1. **生成器切换检测（configure 前主动执行）**：[`devtools/lib/cmake.ts`](../../devtools/lib/cmake.ts) 的 `configure()` 调 [`devtools/lib/config.ts:cachedGenerator()`](../../devtools/lib/config.ts) 读 `build/<preset>/CMakeCache.txt` 的 `CMAKE_GENERATOR:INTERNAL=` 条目——**类型在首个冒号之后**，故先按 `:` 拆出值再按 `=` 取值，否则解析不到；再与「本次期望是否 Ninja」比对，不一致则打印 `[AUTO-CLEAN] Generator changed: <旧> -> <Ninja|platform default>` 并清空该构建目录后重配。读取逻辑放在 `config.ts` 是因为 `artifactDir()` 需按同一份「实际生成器」判定产物落点（见「devtools/ 模块职责」），`cmake.ts` 不再自带私有副本。
2. `cmake --preset <解析后的预设名>`；输出 tee 到 `build/<preset>/configure.log`（控制台仍实时输出同一份原始字节，日志只额外写入命令行与 `[exit N]` 元数据行）；
3. 首次失败 → 先保留现场、输出诊断，再决定是否重试：
   - **现场**：把 `CMakeCache.txt`、`CMakeFiles/CMakeConfigureLog.yaml`、`configure.log` 复制到 `build/<preset>.failed-<ts>/`（时间戳 `yyyyMMdd-HHmmss`，同秒冲突追加 `-1`/`-2`），并写 `diagnostics.log`；现场目录创建或 diagnostics.log 写失败时按 E-04 打印 stderr 且保持退出码非 0；
   - **诊断段**：打印 `[DIAGNOSE] CMake configure failed`，含 `cmake --version`、`resolvePreset()` 结果、缓存中的 `CMAKE_C_COMPILER`/`CMAKE_RC_COMPILER`（存在时）、`TMP`/`TEMP` 与 `os.tmpdir()`、临时文件写探针结果，以及建议（无 ninja 时改用 portable 预设 / 显式 `-DCMAKE_RC_COMPILER=<rc.exe|llvm-rc.exe>` / TMP-TEMP 指向可写目录）；
- **Windows 临时目录兜底**：构建子进程（cmake / ninja / llvm-rc / 编译器）的 `TMP`/`TEMP`/`TMPDIR` 固定指向 `build/tmp`（`devtools/lib/cmake.ts` 的 `subprocessEnv()`）。当环境禁止向用户临时目录写文件时（典型症状：`llvm-rc` 报 `Unable to create temp file: permission denied`，configure 失败），固定到工程内目录可规避；删除 `build/` 即可一并清理。
   - **重试收窄**：仅当 `cachedGenerator()` 与本次期望**仍不一致**时才 `purge()`（删除该目录内除 `_deps/` 外的全部文件与目录，目录递归删除、文件 unlink）并重试一次；生成器未变时打印 `[AUTO-CLEAN] Skipped: configure failure reason unchanged (原因未变，跳过无意义重试).` 并直接以非 0 退出（不再对同一原因重复重试）；
4. 依赖源码在 `third_party/` 子模块（不在 `build/` 下），因此清空 `build/<preset>/` 不会丢依赖；子模块二进制目录 `build/<preset>/_sub/` 会被一并删除并在下一次 configure 重建。脚本仍跳过名为 `_deps` 的条目（FetchContent 时代的残留目录名，保留无害）。

子进程环境：`subprocessEnv()` 把 `dirname(process.execPath)` 前置到 `PATH`；Windows 下把 `TMP`/`TEMP`/`TMPDIR` 指向 `build/tmp`。skill 嵌入已改由 Bun 执行：[`cmake/skill_gen.cmake`](../../cmake/skill_gen.cmake) 以 `find_program(GDA_BUN_EXECUTABLE NAMES bun)` 探测 Bun，Bun 不在 PATH 或 `bun --version` 失败时 configure 直接 `FATAL_ERROR` 并提示安装；CI 的 Bun 由 `oven-sh/setup-bun@v2` 提供（见「CI 与 Release」）。

### 3. 构建 `buildPreset(preset)`

`cmake --build --preset <解析后的预设名>`（`resolvePreset` 的返回值），输出 tee 到 `build/<preset>/build.log`（口径同 §2 的 configure.log）。实际生成器由该预设决定：有 ninja 时为 Ninja，否则为平台默认生成器（并行度计算对两者一致生效）。`-portable` 预设带 `configuration` 字段（见「预设」小节），CMake 自动翻译为 `--config Debug|Release`，多配置生成器（Visual Studio / Xcode / Ninja Multi-Config）据此把产物落进配置名子目录。

### 4. 部署 `deployArtifacts(preset, projects)`（`devtools/commands/deploy.ts`）

逐个工程循环：`--project` 未指定时递归发现 `examples/` 下含 `project.godot` 的最终项目并跳过 intake。库源目录取 [`devtools/lib/config.ts:artifactDir(preset)`](../../devtools/lib/config.ts) 而非 `BUILD_DIR / preset` —— 多配置生成器的产物多一层配置名子目录（见「多配置生成器的产物路径」小节）：

1. 创建部署目录，删除 Godot 热重载遗留的备份库（glob `~*`）；
2. 生成 `godot-autopilot.gdextension`（见下节）；
3. 复制平台库（缺失仅 WARN 不失败）；Windows 额外复制 `godot-autopilot.pdb`（存在时）——Debug 构建由编译器/链接器产出外部 PDB（MSVC Debug=`ProgramDatabase`；clang-cl Debug=`Embedded` CodeView 由链接器汇总为外部 PDB），Release 保持 `Embedded` 不产出外部符号；
4. `validateAddonIntegrity()`（`devtools/commands/deploy.ts`）：解析 gdextension 中当前平台（`windows`/`linux`/`macos` 前缀）的 `[libraries]` 条目，逐个校验 `res://` 对应磁盘文件存在；缺失即报错并以退出码 1 结束（防止宿主工程导出失败）。

### 5. 打包 `packageAddon(libsDir, preset)`（`devtools/commands/package.ts`）

产出 `dist/godot-autopilot-<version>.zip`（`ADDON_VERSION` 读自根目录 `VERSION` 文件——版本号单一来源，与 `project()` 版本一致）。实现要点：

- **临时暂存目录收集**：从构建产物目录（或 `--libs-dir`）收集内容到 `fs.mkdtempSync(os.tmpdir()/gda-package-)` 下建 `addons/godot-autopilot/` 暂存树，再由 `writeZip()`（`devtools/lib/zip.ts`）打包；不依赖也不改写任何已部署工程，zip 内的插件目录前缀固定为 `addons/godot-autopilot`（下游按此路径安装，不可变更），临时目录在 `finally` 中清理。
- **收集内容三件套**（`collectPackageFiles`）：① 平台库（`collectPlatformLibs` 递归取首个匹配）；② C# 桥源码（`collectCSharpBridge`，`BRIDGE_DIR` 下的 6 个 `.cs`，缺失即报错退出）；③ `godot-autopilot.gdextension`（`generateGdextension`）。**`.pdb` 不在收集范围，不随包分发。**
- **两条分支的差异**（`packageAddon`）：

| 分支 | 平台库来源 | 必需平台库 | 失败行为 |
|---|---|---|---|
| 传 `--libs-dir <dir>` | `<dir>` 递归收集三平台库 | 三平台齐全（`PLATFORM_LIBS` 全部值） | 缺任一平台库即报错退出（退出码 1） |
| 不传 `--libs-dir` | 本地构建产物目录 `artifactDir(preset)`（默认 `build/debug`，`--preset release` 时 `build/release`） | 仅当前平台的库（按 `hostPlatform()` 取 `PLATFORM_LIBS`） | 构建目录不存在 → 报错提示先执行对应 `build`；平台不在 `PLATFORM_LIBS` → 报错「Unsupported platform」 |

  两条路径产出同构内容：平台库 + C# 桥源码 + gdextension 描述文件，均不含 `.pdb`。
- **打包完整性自检**：`bun run gda-dev-tools package-check <zip>`（`devtools/commands/package-check.ts`）独立校验（逐条核对：ZIP 中央目录偏移与 EOCD 一致、三平台库齐全、C# 桥 6 个源文件齐全、`.gdextension` 的 `[libraries]` 条目逐一有对应文件、且包内不含 `.pdb`）；全通过打印校验结果并退出 0，缺项报错退出 1，参数错误退出 2。Release 作业在打包后、发布前跑这一步，失败即中止发布。

## CI 与 Release

`.github/workflows/ci.yml` — develop push/PR 触发：三平台 matrix（ubuntu-latest / macos-latest / windows-2022）Debug 编译 + 测试 `ctest --preset debug -E "^gda_runner_|^pool_" --output-on-failure`（历史排除模式随 L2 引擎轨删除已无匹配对象，实际等价无过滤全跑；引擎轨删除前它用于排除无 Godot 即失败的 L2 用例与池 fixture——10-29 `e0d687e` 首次 CI 失败即排除模式漏掉 `^pool_` 所致）；ninja 由 `seanmiddleditch/gha-setup-ninja@v5` 装入 PATH，因此 `resolvePreset` 恒选 `debug` 而非 `debug-portable`（ctest 步骤直接写死 `ctest --preset debug`、不经过 `resolvePreset`，依赖这一前提成立）；sccache + `third_party/` 子模块缓存加速（`actions/checkout` 带 `submodules: true`；`actions/cache` 的 path 为 `third_party/godot-cpp`、`third_party/mcp-cpp-sdk`、`third_party/googletest`，key `submodules-debug-${{ runner.os }}-${{ steps.submodules.outputs.shas }}`，shas 取 `git ls-files -s third_party` 的 gitlink sha，子模块升级即换 key）。build job 不安装 Python（历史 setup 步骤已移除）：开发链只依赖 Bun + CMake + 编译器，skill 嵌入由 `oven-sh/setup-bun@v2` 提供的 Bun 执行 `tools/embed_skills.ts`；`release.yml` 的 build job 同步为 Bun setup。另有 `cli-bins` 作业：三平台各自 `bun build --compile` 出可执行文件并跑 `--version` / `list` 冒烟，**不发布**（发布走 `release.yml` 的 `cli` 作业）。

`.github/workflows/release.yml` — tag 双格式触发（`v*.*.*` / `[0-9]*.*.*`，另支持 `workflow_dispatch` 手动指定 tag）：

| job | 内容 |
|---|---|
| validate | 校验 tag 与根 `VERSION` 一致（`v` 前缀可选：`v0.3.4` / `0.3.4` ↔ `0.3.4`，版本号以根 `VERSION` 为准，当前开发版 0.3.4-dev；发布前去掉 `-dev`），不一致 fail |
| build | 同 CI 环境（Ninja/sccache/msvc-dev-cmd，ninja 由 `seanmiddleditch/gha-setup-ninja@v5` 提供，故 `resolvePreset` 恒选 `release`；checkout 带 `submodules: true`，缓存同 CI 改为 `key: submodules-release-${{ runner.os }}-${{ steps.submodules.outputs.shas }}`），Release 构建后按精确文件名上传各平台库 artifact（天然排除 pdb） |
| cli | 5 个目标的 `bun build --compile` 独立二进制（`windows-x64` / `linux-x64` / `linux-x64-musl` / `macos-arm64` / `macos-x64`），macOS 产物 ad-hoc 签名（`codesign --force --sign -`），各自打 zip/tar.gz 后上传 artifact；二进制名带根 `VERSION` |
| package | 下载全部 artifact → `bun run gda-dev-tools package --libs-dir dist` 合并 → **跑 `bun run gda-dev-tools package-check dist/godot-autopilot-*.zip` 自检（失败即中止发布）** → 重命名为 `addons.zip` → 收集 `cli-dist/` 下二进制并生成 `sha256sums.txt` → softprops/action-gh-release 一并发布（`addons.zip` + `gda-<version>-<platform>.*` + 校验和）。校验和覆盖 `addons.zip` 与全部 CLI 二进制 |

Release 资产下载对 tag 的 `v` 前缀与无前缀两种形式依次尝试，校验文件固定为 `sha256sums.txt`；`gda install plugin` 与 `scripts/install.*` 共用该口径。

macOS runner 为 ARM64，preset 设 `CMAKE_OSX_ARCHITECTURES=x86_64;arm64` 编译 universal 双架构（仅 Apple 平台生效），gdextension 对应条目为 `macos.{debug,release}.universal`。

## 产物清单

| 产物 | 位置 | 说明 |
|---|---|---|
| `godot-autopilot.dll` | `build/{debug,release}/`（多配置生成器再套一层 `Debug`/`Release`）→ 部署目录 | Windows 平台库 |
| `libgodot-autopilot.so` | 同上 | Linux |
| `libgodot-autopilot.dylib` | 同上 | macOS |
| `godot-autopilot.pdb` | 同上（Windows，存在时复制） | 调试符号；Debug 构建的外部 PDB（MSVC `/Zi`+`ProgramDatabase`；clang-cl `Embedded` CodeView 经链接器汇总），Release 保持 `Embedded` 无外部符号；**仅部署目录，不进发布包** |
| `godot-autopilot.gdextension` | 部署目录（每次部署重新生成） | 入口 `entry_symbol = "GDExtensionEntryPoint"`、`compatibility_minimum = "4.7"`、`[libraries]` 6 条平台路径（windows/linux 为 `x86_64`，macos 为 `universal`） |
| 6 个 C# 桥 `.cs` | `csharp_bridge/`（`BRIDGE_DIR`）→ 部署目录与发布包内 | `BridgeCall.cs`/`BridgeConvert.cs`/`BridgeEvents.cs`/`BridgeGetSet.cs`/`GdaCSharpBridge.cs`/`GdaCSharpRuntime.cs`（`BRIDGE_SOURCES`）；缺任一即报错退出 |
| `dist/godot-autopilot-<version>.zip` | `dist/` | `--package` 产物（版本号取自根 `VERSION` 文件）；包内为「三平台库 + 6 个 C# 桥 `.cs` + `.gdextension`」，不含 `.pdb` |

**构建目录布局随生成器而变**：单配置生成器（Ninja / Unix Makefiles）直接落 `build/debug/godot-autopilot.dll`；多配置生成器（Visual Studio / Xcode / Ninja Multi-Config）**再套一层以配置命名的子目录**，实测（Windows，`CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026`，`CMAKE_CONFIGURATION_TYPES=Debug;Release;MinSizeRel;RelWithDebInfo`）：

| 产物 | 单配置生成器 | 多配置生成器 |
|---|---|---|
| 平台库 | `build/debug/godot-autopilot.dll` | `build/debug/Debug/godot-autopilot.dll`（同目录另有 `.exp`/`.lib`/`.pdb`） |
| gtest 测试可执行（单元 + 集成） | `build/debug/tests/gda_unit_tests.exe` / `gda_integration_tests.exe` | `build/debug/tests/Debug/gda_unit_tests.exe` / `gda_integration_tests.exe` |

程序侧一律用 [`devtools/lib/config.ts:artifactDir(preset)`](../../devtools/lib/config.ts) 取平台库源目录，不写死 `BUILD_DIR / preset`（见「devtools/ 模块职责」）。

部署目录：目标 Godot 工程的 `addons/godot-autopilot/`（`--project` 指定或递归发现 `examples/` 下的最终项目；不入库、不参与打包）；CI/Release 直接使用 `build/release/` 下的产物，不经过部署步骤。`devtools/` 与 CI/Release 均使用仓库实际目录大小写，大小写敏感平台不会产生分叉目录。

## CMake 目标

`add_library(godot-autopilot SHARED ...)` 共 **115 个 .cpp**（源清单由 `cmake/sources/*.cmake` 聚合，见下）：

| 目录 | 数量 | 目录 | 数量 |
|---|---:|---|---:|
| `src/main.cpp` | 1 | `src/tools/` | 59 |
| `src/core/` | 31（含 editor_readiness / editor_coords / log_persist / trace_recorder / sanitize_policy / monitor / monitor_env / perf_sampler / gda_home / project_store / instance_registry / storage_paths / legacy_store / port_resolver / api_token / http_transport / http_route_table / http_stream_registry / godot_transport / mcp_http_shim / ipc_endpoint / ipc_framing / ipc_transport） | `src/util/` | 7（含 scene_verify；skill_gen + skill_content_generated 构建期嵌入薄胶水） |
| `src/resources/` | 3 | `src/ui/` | 1 |
| `src/prompts/` | 9 | `src/runtime/` | 4（含 09-18 新增 game_bridge_verify） |

（分项求和 1 + 59 + 31 + 7 + 3 + 1 + 9 + 4 = 115，与磁盘逐行核对一致；磁盘 `src/**/*.cpp` 共 115 个，全部已登记。）

- **源清单聚合机制（10-21 引入）**：`add_library` 的源清单不再直接写在根 `CMakeLists.txt` 里，而是拆到 `cmake/sources/` 下的若干清单文件，由根 `CMakeLists.txt` 依次 `include` 后拼成目标源列表——`gda_core.cmake`（基础业务源）、`api_core.cmake` / `api_contract.cmake` / `api_storage.cmake` / `api_transport.cmake` / `api_cli.cmake`（各并行分支独占维护的新增编译单元；0.3.2 移除 REST 面后多数清单已清空，文件保留以维持登记位）；`tests/CMakeLists.txt` 侧对应 `gda_unit_sources.cmake` 与 `api_unit_*.cmake`。目的是让并行开发同一插件的多个分支各自登记新增 `.cpp`，避免在同一文件里产生归并冲突。`cmake/sources/api_probe.cmake` 另定义一个可运行的传输探针目标 `gda_mcp_transport_probe`（**不注册为 ctest 用例**，由验收阶段按需执行）。

- **skill 内容嵌入头**：技能正文存于仓库根 `skills/` 标准目录（1 张入口卡 + 9 篇 references，共 10 个 .md，**不进 add_library**），`cmake/skill_gen.cmake` 在构建期经 `tools/embed_skills.ts`（Bun 执行）生成 `build/<preset>/generated/skill_content_embedded.h`（gitignore 覆盖）；`add_dependencies(godot-autopilot gda_skill_embed_header)` 保证生成先于编译；
- 私有头文件目录：`${CMAKE_SOURCE_DIR}/src`；
- 链接库（PRIVATE）：`godot-cpp`、`mcp-server`、`mcp-http`（后两者来自 mcp-cpp-sdk）；
- MSVC（含 clang-cl）额外 `target_link_options "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>"`（`CMakeLists.txt:94`）—— 强制导出 godot-cpp 全部符号，防止 GDExtension 入口符号被链接器裁剪；
- **Unity 构建已接线生效**：`set_target_properties(godot-autopilot PROPERTIES UNITY_BUILD ${GDA_UNITY_ENABLED} UNITY_BUILD_BATCH_SIZE ${GDA_UNITY_BATCH})`（`CMakeLists.txt`，add_library 之后）——实测 Debug batch=8（16 核）；此前仅 BuildOptimization.cmake 计算参数、未挂到 target；
- `option(GDA_ENABLE_TESTS ... OFF)`，开启后 `add_subdirectory(tests)`（详见 [tests.md](./tests.md)）。

## cmake/ 模块职责

按根 `CMakeLists.txt:52-58` 的 include 顺序：

| 文件 | 职责 | 关键变量 |
|---|---|---|
| `Platform.cmake` | 架构与 CI 探测 | `GDA_ARCH`（`x86_64`/`x86`，按指针宽度）、`GDA_IS_CI`（进程环境存在 `CI` 即 ON） |
| `BuildOptimization.cmake` | 硬件感知并行度（核心规则） | `GDA_COMPILE_JOBS`、`GDA_LINK_JOBS`（CACHE 优先、其次 ENV）、`GDA_UNITY_BUILD`（ON 且核数 >1 时启用）、`GDA_UNITY_BATCH_SIZE`（0=auto）、`GDA_MAX_COMPILE_MEM_MB`（默认 1500）、`GDA_MAX_LINK_MEM_MB`（默认 4000）、`GDA_UNITY_MEM_MB`（默认 500）；`CMAKE_BUILD_PARALLEL_LEVEL` 对**所有**生成器生效；`gda_compile_pool`/`gda_link_pool` 仅在 `if(CMAKE_GENERATOR MATCHES "Ninja")` 时注册（`:84-91`），非 Ninja 只打印 `[gda] Ninja jobs: N (non-Ninja generator)`，其余并行度与 Unity 计算照常 |
| `CompilerOptions.cmake` | 按编译器分发 flags | Clang/clang-cl 与 MSVC：`/utf-8 /bigobj /W4 /EHsc` + `_CRT_SECURE_NO_WARNINGS`、`_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`、`_WIN32_WINNT=0x0A00`；**MSVC 分支（`elseif(MSVC)`，即真正的 cl.exe）额外 `/Zc:lambda`**，clang-cl 走的是第一个 `if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")` 分支、不加该开关（见下「MSVC lambda 捕获差异」小节）；**调试信息格式按配置**：MSVC 分支 `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT` = Debug `ProgramDatabase`（`/Zi`+外部 PDB）/其余 `Embedded`（`/Z7`）；clang-cl（Windows）分支 Debug 设 `Embedded` CodeView，由链接器 `/debug /pdb:<TARGET_PDB>` 汇总为外部 `godot-autopilot.pdb`；两种情况下 Release 都不新增外部符号；GCC/Clang 非 Windows 非 CI 追加 `-march=native`；`find_package(Threads REQUIRED)`；`CMAKE_POSITION_INDEPENDENT_CODE ON` |
| `Cache.cmake` | 编译缓存自动探测 | 优先 sccache（支持 MSVC），回退 ccache（仅 GCC/Clang）；命中则设 `CMAKE_C/CXX_COMPILER_LAUNCHER` |
| `Dependencies.cmake` | `third_party/` 子模块依赖（纯 `add_subdirectory`，无 FetchContent） | `GDA_THIRD_PARTY_DIR`（`${CMAKE_SOURCE_DIR}/third_party`）、`GDA_SUBMODULE_BIN_DIR`（`${CMAKE_BINARY_DIR}/_sub`）、`gda_require_submodule(<name>)`（子模块缺失即 `FATAL_ERROR` 并提示 `git submodule update --init --recursive`，故意不做网络回退）、`MCP_WITH_OPENSSL "OFF"`（CACHE STRING ... FORCE，仅 `add_subdirectory(mcp-cpp-sdk)` 前设置；SDK 0.3.8 起三态开关 AUTO/ON/OFF，GDA 仅监听 127.0.0.1 明文 HTTP 无 TLS 需求，故显式 OFF）、`GODOTCPP_API_VERSION "4.7"`（CACHE/FORCE）；`godot-cpp` @ `10.0.0-stable`（commit `507ed9d`）、`mcp-cpp-sdk` @ `0.3.11`（commit `93e18ef`） |
| `Lto.cmake` | 仅 Release 的链接优化 | 优先级 Clang ThinLTO（`-flto=thin`）> MSVC LTCG（`CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`）> GCC IPO（`CheckIPOSupported`）；非 Release 直接跳过 |
| `skill_gen.cmake` | skill 标准目录构建期嵌入（09-08 新增） | `GDA_BUN_EXECUTABLE`（`find_program(NAMES bun)`；未命中或 `bun --version` 失败即 `FATAL_ERROR` 并提示安装 Bun 1.4+）+ configure 期 `--version` 可用性自检；`add_custom_command` 执行 `"${GDA_BUN_EXECUTABLE}" tools/embed_skills.ts` 生成 `<build>/generated/skill_content_embedded.h` + `add_custom_target(gda_skill_embed_header)`；依赖 `skills/*/[SKILL.md|references/*.md|scripts/*.mjs]`（CONFIGURE_DEPENDS） |

Bun 探测为何用 `find_program`：Bun 是单文件可执行，官方安装器与 CI 的 `oven-sh/setup-bun@v2` 都会把 `bun` 放入 PATH，`find_program(NAMES bun)` 即可命中（Windows 自动匹配 `bun.exe`）。configure 期额外执行 `bun --version` 自检，避免 PATH 中是损坏的存根；Bun 缺失时 `FATAL_ERROR` 给出安装地址，不再回退 Python。

### 依赖获取（`third_party/` 子模块）

三个依赖由根 `.gitmodules` 管理为 git submodule，版本由 gitlink 锁定的 commit 唯一确定，不随上游 tag 移动（tag 仅作人类可读标识）：

| 子模块 | 上游仓库 | tag | commit |
|---|---|---|---|
| `third_party/godot-cpp` | `godotengine/godot-cpp` | `10.0.0-stable` | `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5` |
| `third_party/mcp-cpp-sdk` | `jesspig/modelcontextprotocol-cpp-sdk` | `0.3.11` | `93e18ef8bf9766754a5320b167389e094c28cca7` |
| `third_party/googletest` | `google/googletest` | `v1.15.2` | `b514bdc898e2951020cbdca1304b75f5950d1f59` |

- **检出方式**：`git clone --recurse-submodules <repo>`，或事后 `git submodule update --init --recursive`；子模块源码在 `build/` 之外，随仓库一同流转，不受清理 `build/` 影响。
- **子项目二进制目录**：`${CMAKE_BINARY_DIR}/_sub/<name>`（即 `build/<preset>/_sub/godot-cpp` 等），删除后可由 configure 重建。
- **缺失即失败**：`gda_require_submodule` 直接 `FATAL_ERROR` 并提示 `git submodule update --init --recursive`，**故意不做网络回退**——回退会让构建行为不确定，内网环境下会在 configure 中途抛出难以定位的 git 报错；因此 configure 阶段零网络访问。
- googletest 由 `tests/CMakeLists.txt` 在 `GDA_ENABLE_TESTS=ON` 时经同一 `gda_require_submodule` + `add_subdirectory` 引入（见 [tests.md](./tests.md)）。

### 并行度计算规则（`BuildOptimization.cmake`）

- **编译作业数** = `min(总内存/1500MB, 核数-2)`，下限 1；同时写入 `CMAKE_BUILD_PARALLEL_LEVEL`；
- **链接作业数** = `min(总内存/4000MB, 2)`，下限 1（链接内存密集，刻意收紧）；
- 作业池命名 `gda_compile_pool` / `gda_link_pool`，加 `gda_` 前缀是**刻意规避**与 mcp-cpp-sdk 自带 `compile_pool`/`link_pool` 的全局同名冲突（`BuildOptimization.cmake:83` 注释）；
- **Unity batch** = `min(总内存/500MB, (核数+1)/2)`，下限 2。

### 预设（`CMakePresets.json`）

version 8，**6 个 configure 预设**（2 个 hidden 基座 + 4 个可直接 `--preset` 使用），build / test 预设各 4 个：

| configure 预设 | `inherits` | `generator` | `binaryDir` | 说明 |
|---|---|---|---|---|
| `base-debug` | — | 不声明 | `build/debug` | `hidden: true`，仅供继承，**不可直接 `--preset` 使用** |
| `debug` | `base-debug` | `"Ninja"` | 继承（同一目录） | 检测到 ninja 时使用 |
| `debug-portable` | `base-debug` | 省略 | 继承（同一目录） | 无 ninja 时的回退 |
| `base-release` | — | 不声明 | `build/release` | `hidden: true`，仅供继承 |
| `release` | `base-release` | `"Ninja"` | 继承（同一目录） | 检测到 ninja 时使用 |
| `release-portable` | `base-release` | 省略 | 继承（同一目录） | 无 ninja 时的回退 |

- **两个基座承载全部共性**，经 `inherits` 复用不重复声明：`CMAKE_BUILD_TYPE` = Debug / Release、`CMAKE_EXPORT_COMPILE_COMMANDS=ON`、`CMAKE_OSX_ARCHITECTURES=x86_64;arm64`（仅 Apple 平台生效，产出 universal 双架构库）、`GDA_ENABLE_TESTS=ON`。
- **两条路径共用同一 `binaryDir`**（`build/debug` / `build/release`），不是分开目录；切换生成器时由 [`devtools/lib/cmake.ts`](../../devtools/lib/cmake.ts) 的 AUTO-CLEAN 重建缓存（见下）。
- **省略 `generator` 的依据**：CMake 官方文档中 `generator` 字段 "If omitted, CMake will fall back to regular generator discovery procedure" —— 无 ninja 的机器因此仍可 configure（Windows → Visual Studio，Linux/macOS → Unix Makefiles）。
- **选路入口只有一处**：`devtools/lib/config.ts:resolvePreset()` 用 `Bun.which("ninja")` 探测，命中返回 `<preset>`，否则返回 `<preset>-portable`。动机：Windows 上 ninja 通常只存在于 VS 开发环境（DevShell），普通终端探测不到。
- **命令入口**：`bun run gda-dev-tools build [--release|--debug]` / `deploy [--project <dir>]... [--release|--debug]` / `package [--libs-dir <dir> | --preset debug|release]` / `clean [--project <dir>]...` / `test [--preset debug|release]` / `jam [--theme <id|theme>] [--json]` / `jam create --theme <id|theme> --name <name> [--slug <slug>] [--out <dir>] [--json]` / `jam e2e [--reset] [--theme <id|theme>] [--json]` / `jam list [--projects] [--json]` / `jam index [--json]` / `tui`；`test --preset` 与 `package --preset` 的可选值只有 `debug` / `release` 两个，实际 preset 名由 `devtools/commands/test.ts` 内部调 `resolvePreset` 解析；`devtools/tui.ts` 同理。手动 `cmake --preset` / `ctest --preset` 绕过脚本，必须自己按 ninja 可用性补 `-portable` 后缀。
- **build / test 预设**：`debug`、`debug-portable`、`release`、`release-portable` 四个，各自 `configurePreset` 关联同名 configure 预设。**`-portable` 的 build 与 test 预设额外声明 `configuration`**（`debug-portable` = `Debug`、`release-portable` = `Release`），Ninja 的 `debug`/`release` 不声明——多配置生成器下 ctest 缺 `-C` 会漏注册测试（见下「ctest 在多配置生成器下需要 `-C`」小节）。
- 根 [`CMakeLists.txt`](../../CMakeLists.txt) 在 `project()` 之前自动探测编译器（Windows 找 `clang-cl` 并加 `--target=x86_64-pc-windows-msvc`，非 Windows 找 `clang++-19/18/17/16` 回退 `clang++`；都找不到就交给系统默认），并在 `find_program(ninja)` 命中但生成器非 Ninja 时给出 `WARNING [gda] Ninja available (...) but generator is '...'. Use -G Ninja for faster builds.`。实测在无 DevShell 的普通 PowerShell 中，CMake 能自行发现 Visual Studio 18 并使用 MSVC，无需 vcvars 环境。

#### 生成器切换（已知坑）

`-portable` 预设省略 `generator` 时，CMake **会沿用 cache 里已有的生成器而非重新发现**。因此在装过 ninja 的 DevShell 里构建过一次、再切到探测不到 ninja 的普通终端时，`build/<preset>/CMakeCache.txt` 会留下不可用的 ninja 路径，后续构建必然失败。故 `configure()` 在 configure 前主动比对并清空目录，而不是等 configure 自己报错——机制与实现见上文「构建流水线 → 配置」。`BuildOptimization.cmake` 的并行度与 Unity 计算对所有生成器一致生效。

#### MSVC lambda 捕获差异（已知坑）

上一批让「无 ninja 机器能 configure」之后，MSVC 路径的**首次全量编译**才暴露出第二个问题：

- **现象**：`src/tools/base/runtime_ops.cpp(891,33): error C3493` 与 `tests/unit/tool_catalog_test.cpp(93,27): error C3493`。
- **根因**：MSVC **拒绝 lambda 经 `[&]` 隐式引用捕获局部 `const`/`constexpr` 变量**，clang（clang-cl）接受同一写法。典型形态是函数内 `constexpr size_t N = 256;` 配合 `std::thread([&s]{ ... s.size() > N ... })`；经最小复现逐条定位，属 MSVC 实现开关而非源码违规 —— 源码本身合法，只在两个编译器间行为不一致。
- **修法**：[`cmake/CompilerOptions.cmake:48`](../../cmake/CompilerOptions.cmake) 的 MSVC 分支（`elseif(MSVC)`，即真正的 cl.exe；clang-cl 命中的是前面的 `if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")` 分支，不受影响）追加 `/Zc:lambda`。这是 MSVC 的 lambda 标准符合性开关，**CMake 默认不启用**。加上后**零源码改动**即可编译，并顺带拉齐了 clang 与 MSVC 的行为差异。

#### 多配置生成器的产物路径（已知坑）

- **现象**：在 Visual Studio 生成器下构建成功，但 `deploy` 报 `[WARN] godot-autopilot.dll not found in <dir>`，库根本没复制出去。
- **根因**：Visual Studio / Xcode / Ninja Multi-Config 等**多配置生成器**会把产物再放进一层**以配置命名的子目录**。实测落点：`build/debug/Debug/godot-autopilot.dll`、`build/debug/tests/Debug/gda_unit_tests.exe`；单配置生成器（Ninja / Unix Makefiles）则直接落 `build/debug/godot-autopilot.dll`。对照表见「产物清单」。
- **修法**：[`devtools/lib/config.ts`](../../devtools/lib/config.ts) 新增 `cachedGenerator(target)`（读 `CMakeCache.txt` 的 `CMAKE_GENERATOR:INTERNAL=`，**类型在首个冒号之后**，须先按 `:` 拆再按 `=` 取值）与 `artifactDir(preset)`；后者按前缀表 `("Visual Studio", "Xcode", "Ninja Multi-Config")` 判定，命中则返回 `build/<preset>/Debug|Release`，否则返回 `build/<preset>`。**判定依据是构建目录里实际记录的生成器，而非预设名** —— 手动用 `--preset debug-portable` 而 PATH 恰有 ninja 时，CMake 发现的仍是单配置生成器，产物就在 `build/debug` 根下。消费方 `devtools/commands/deploy.ts:deployArtifacts()`（`binaryDir = artifactDir(preset)`）不再写死 `BUILD_DIR / preset`。
- **去重**：读取生成器的逻辑原先是 `devtools/lib/cmake.ts` 的私有副本，现已删除、改为从 `config.ts` 导入 `cachedGenerator` —— 全仓库只有一份实现，`artifactDir()` 与 `configure()` 共用。

#### ctest 在多配置生成器下需要 `-C`（已知坑）

- **现象**：只跑 `ctest --preset debug-portable` 时**只注册了 40 个测试**，且三个非 gtest 用例直接报 `Test not available without configuration. (Missing "-C <config>"?)`。
- **根因**：多配置生成器下 `gtest_discover_tests`（[`tests/CMakeLists.txt:91`](../../tests/CMakeLists.txt)）生成的**发现文件名带配置名**。实测 `build/debug/tests/` 下为 `gda_unit_tests_e3b0c442_Debug_tests.cmake`（另有 `_include.cmake` / `_discovery.cmake` 以及 Release / RelWithDebInfo / MinSizeRel 对应文件）；ctest 不带 `-C` 时不会 include 对应配置的发现文件，于是 gtest 用例整体缺席。
- **对照实测**：`ctest --test-dir build/debug -C Debug -N` → **928** 个（871 个 单元测试 gtest + 47 个 L2 runner + 10 个守卫/脚本，2026-10-28 实测；现行数值见 [tests.md](./tests.md)「数值核算总表」）；缺 `-C` 的 `ctest --test-dir build/debug -N` 实测同为 928 —— build/debug 为 Ninja 单配置目录，`-C` 不影响枚举。
- **修法**：在 [`CMakePresets.json`](../../CMakePresets.json) 的 build/test 预设上加 `configuration` 字段（`debug-portable` = `Debug`、`release-portable` = `Release`；Ninja 的两个预设不加）。CMake 会自动把它翻译成 `--config` / `-C`，因此 **`devtools/commands/test.ts` 零改动**，`bun run gda-dev-tools test` 在两类生成器下行为一致。
- **注意**：这是「配置名」维度，与预设名正交 —— 手动执行 `ctest --preset debug-portable` 已自动带上 `-C Debug`；若绕过 preset 直接 `ctest --test-dir build/debug`，则须自己补 `-C Debug`。

## devtools/ 模块职责（Bun/TS）

| 模块 | 职责 |
|---|---|
| [`config.ts`](../../devtools/lib/config.ts) | 路径与集合常量（`PROJECT_ROOT`/`BUILD_DIR`/`EXAMPLES_DIR`/`PLATFORM_LIBS`）、**`BRIDGE_DIR`**（`PROJECT_ROOT/csharp_bridge`）、**`BRIDGE_SOURCES`**（6 个 C# 桥源文件名的必需清单，缺失即打包失败）、`ADDON_VERSION`（读根 `VERSION`，版本号单一来源）、**`resolvePreset()`**（按 `Bun.which("ninja")` 在 `<preset>` 与 `<preset>-portable` 间选路，生成器双路径的**唯一决策点**）、**`cachedGenerator(target)`**（读 `CMakeCache.txt` 的 `CMAKE_GENERATOR:INTERNAL=`，无缓存返回 `null`，生成器读取的唯一实现）、**`artifactDir(preset)`**（按实际生成器判定多配置生成器的配置名子目录，返回 `build/<preset>/Debug\|Release` 或 `build/<preset>`）、`discoverExampleProjects()`（经 `devtools/lib/jam_projects.ts` 递归扫描 `examples/`，跳过 intake、e2e 沙盒与工具目录）、`resolveProjects()`（`--project` 路径优先，未传入时用 `discoverExampleProjects()`；校验 `project.godot` 并去重） |
| [`jam_projects.ts`](../../devtools/lib/jam_projects.ts) | Game Jam 项目元数据与发现：读取 `jam.json`、判定 `kind=intake`/`kind=e2e`、递归扫描 `examples/` 并跳过 intake、e2e 沙盒与 `.git`/`.godot`/`addons`/`build`/`node_modules` |
| [`cmake.ts`](../../devtools/lib/cmake.ts) | `configure()` / `buildPreset()`：preset 解析 → 从 `config` 导入的 `cachedGenerator()` 比对 cache 生成器、切换时 `purge()` 后重配 → `cmake --preset` → 首次失败先保留现场（`build/<preset>.failed-<ts>/`）并输出 `[DIAGNOSE]` 诊断段 → 仅生成器仍不一致时 `purge()` 重试一次，否则直接失败；`run()` 统一打印命令行并把输出 tee 到 `build/<preset>/{configure,build}.log`，子进程无法启动时打印 `[ERROR]` 并按失败返回；`subprocessEnv()` 把 Bun 目录前置到子进程 PATH，Windows 下重定向临时目录 |
| [`test.ts`](../../devtools/commands/test.ts) | ctest 薄封装（`runTests`：默认全量门 / `--single` 按 `-R` 单跑）；preset 名经 `resolvePreset` 解析，故 `test --preset` 的可选值仍只有 `debug`/`release` 两个 |
| [`jam.ts`](../../devtools/commands/jam.ts) | Game Jam 抽题与项目创建：读 schema v2 题库并校验；按比赛最终项目判定已用主题；每次抽题独立采样 3–5 条关键词与灵感；生成/刷新 `examples/<theme_id>/project/` intake；`jam create` 复制 intake、按 `<slug>-<hash8>` 命名、写入 `jam.json`；`jam e2e` 创建/刷新/重置 `examples/e2e`（`kind=e2e`，拒绝非 e2e 目录）；作品说明文档按 `GAMEJAM.md` 提交要求处理；支持 `list`/`projects`/`index`/`--json`；单测见 `cli/tests/jam.test.ts` |
| [`deploy.ts`](../../devtools/commands/deploy.ts) | 把 `artifactDir(preset)` 指向的产物（**不是** `BUILD_DIR / preset`，多配置生成器下多一层配置名子目录）复制到 `resolveProjects()` 解析出的目标工程，生成 `.gdextension`，`validateAddonIntegrity()` 逐条校验 res:// 路径存在（库缺失时报错并以退出码 1 结束） |
| [`package.ts`](../../devtools/commands/package.ts) | 打包本地构建产物（默认 `build/debug`，`--preset release` 用 `build/release`，`--libs-dir` 用外部目录）；`collectPlatformLibs()` 递归收集三平台库、`collectCSharpBridge()` 收集 6 个 C# 桥 `.cs`、`collectPackageFiles()` 汇总「平台库 + C# 桥 + gdextension」到**临时暂存目录**（不含 `.pdb`），再由 `writeZip()` 出 `dist/godot-autopilot-<version>.zip` |
| [`package-check.ts`](../../devtools/commands/package-check.ts) | 打包完整性自检：解压 zip 后核对三平台库齐全、`BRIDGE_SOURCES` 6 个 C# 桥源文件齐全、`.gdextension` 的 `[libraries]` 条目逐一有对应文件、包内不含 `.pdb`；通过退出 0，缺项退出 1，参数错误退出 2 |
| [`clean.ts`](../../devtools/commands/clean.ts) | 按 `resolveProjects()` 解析出的目标工程清 `.godot/` 与 `addons/godot-autopilot/`，**不碰 `build/`** |
| [`tui.ts`](../../devtools/tui.ts) | 交互式构建控制台（`@clack/prompts`）；覆盖构建/部署/打包/清理/测试/状态表；部署与清理输入 Godot 工程路径，构建类型与测试预设的可选值仍只有 Debug/Release，走同一 `resolvePreset`；非 TTY 提示并退出 2 |
| [`main.ts`](../../devtools/main.ts) | 参数解析与子命令分发（`build` / `deploy` / `clean` / `package` / `package-check` / `test` / `jam` / `tui`） |

## 环境变量

| 变量 | 作用 | 生效方式 |
|---|---|---|
| `GODOT_AUTOPILOT_PORT` | 覆盖端口（10-21 起为**第一优先级**：环境变量 ＞ 工程级配置偏好 ＞ 自动分配） | 运行时 `std::getenv`（`src/core/server_context.cpp`）；显式指定时绑定失败**不自动换端口**，直接启动失败 |
| `GODOT_AUTOPILOT_HOST` | 读取监听地址；默认 `127.0.0.1`，非环回地址由 `ServerContext::start()` 拒绝 | 运行时 `std::getenv`（`src/core/server_context.cpp`） |
| `GODOT_AUTOPILOT_HOME` | 覆写插件数据的家目录集中根（10-21 新增） | 插件侧：`HomeResolver::gda_home()` 的第 1 级来源（形状校验，不做存在性校验）；CLI 侧：`gdaHome()` 的第 1 级来源 |
| `GODOT_AUTOPILOT_API_TOKEN` | 覆写 REST 面的 Bearer 令牌（10-21 新增） | 插件侧：`ApiAuth::load_token()` / `issue_token()` 的第 1 级来源；设置后不再签发也不写令牌文件 |
| `GODOT_AUTOPILOT_CLI_HOME` | 覆写 CLI 的 state dir（10-21 新增） | CLI 侧：`registry.ts` 的 `stateDir()` 第 1 级来源 |
| `GODOT_AUTOPILOT_USERDATA` | 覆写 CLI 解析 Godot userdata 目录的位置 | CLI 侧：`platform.ts` 的 `userDataBase()` 第 1 级来源（仅影响 CLI 寻址链的旧路径回退） |
| `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` | 强制编译/链接并行度 | CACHE（`-D`）优先，其次进程环境变量 |
| `CI` | 存在即 `GDA_IS_CI=ON` | 隐式；关闭 `-march=native` 以保证可复现 |
| `GDA_UNITY_BUILD` / `GDA_UNITY_BATCH_SIZE` / `GDA_MAX_COMPILE_MEM_MB` / `GDA_MAX_LINK_MEM_MB` / `GDA_UNITY_MEM_MB` | Unity 与内存估算 | 仅 CMake 缓存参数（`-D`），**不支持环境变量** |

## 本地 CLI 开发（Bun）

CLI 载体（`cli/`）为 Bun + TypeScript 源码直跑，**不参与 `gda-dev-tools build`/`deploy`/`package` 构建流水线**。本地开发只需 Bun：

- 跑测试：`bun test cli/tests`（275 项：252 pass / 23 skip，秒级）
- 类型检查：`bun x tsc -p cli/tsconfig.json --noEmit`
- 手动调用：`bun cli/bin/gda.ts --help`

CI 经 ctest `cli_unit`（封装 `bun test`）与 `cli_contract`（CLI 载体契约）接入，Bun 由 CI 环境提供；bun 缺失时两测试打印 SKIP 并退出 0。详见 [modules/cli.md](./modules/cli.md)。

## 与 AGENTS.md / README 对照

逐条核对结论（一致，另有一处精度补充）：

- `bun run gda-dev-tools build` / `--release` 语义、手动 `cmake --preset` 命令 ✓（**精度补充**：手动命令绕过 `resolvePreset`，无 ninja 时须改写 `debug-portable` / `release-portable`，见「预设」小节）；
- "依赖以 `third_party/` 子模块随仓库检出" ✓（`cmake/Dependencies.cmake` 的 `gda_require_submodule` 缺依赖即 FATAL_ERROR，`.gitmodules` 锁定 commit）；
- "添加新 .cpp 时必须在 `add_library()` 中加入" ✓（Unity 构建只编译列出的文件）；**精度补充**：`skills/` 下 .md 为内容数据文件，经生成头机制（`skill_gen.cmake` + `tools/embed_skills.ts`）进入编译，不进 add_library；
- 依赖版本 `godot-cpp 10.0.0-stable`（`507ed9d`）/ `mcp-cpp-sdk 0.3.11`（`93e18ef`）、git submodule 检出 ✓（`cmake/Dependencies.cmake` + `.gitmodules`，含 `GODOTCPP_API_VERSION "4.7"` FORCE 与 `MCP_WITH_OPENSSL "OFF"` FORCE）；
- 编译器优先 Clang/clang-cl、MSVC/GCC 回退 ✓（根 CMakeLists 自动探测 + `CompilerOptions.cmake` 分发）；**精度补充**：MSVC 分支的 `/Zc:lambda` 是 clang-cl 路径不需要的符合性开关，仅为让 MSVC 接受 clang 已接受的写法而加（见「MSVC lambda 捕获差异」小节）；
- 优化自适应（sccache/ccache、LTO、Unity、生成器自适应 + Ninja 作业池）✓；**精度补充**：「生成器自适应」覆盖的不只是能否 configure，还包括多配置生成器下的产物路径（`artifactDir`）与 ctest 配置维度（preset `configuration`）；AGENTS.md 与实现一致：仅 `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` 支持 CACHE（`-D`）与进程环境变量双通道，`GDA_UNITY_BUILD`/`GDA_UNITY_BATCH_SIZE` 等内存参数只接受 `-D` CACHE；
- **文档一致性**：五个语言版本 README 的工具数口径为 badge `tools-452+` 与“Over 452 tools”（451 域工具 + `system_status`；数量随版本增长，以 `search_tools` 查到的为准），与 wiki 实测的 [overview.md](./overview.md) 口径一致——wiki 另计的 458/459（MCP 可达 / catalog-index）含 7 元工具与 `system_status`，README 未列这两个数字属“域工具 vs 全量口径”差异；README 的安装入口指向根 `INSTALL.md`，构建说明指向 [build.md](./build.md) 与 [tests/README.md](../../tests/README.md)，`--package` / `--debug` 参数以本页命令速查表为准。

## 关联页面

- [modules/core.md](./modules/core.md) — 核心模块（ServerContext、命令队列、模式检测）
- [tests.md](./tests.md) — 测试体系（程序化四层）与 `GDA_ENABLE_TESTS` 用法
- [overview.md](./overview.md) — 项目总览与工具统计
- [modules/support.md](./modules/support.md) — 支撑模块（技能书、客户端配置生成、资源与提示词）
