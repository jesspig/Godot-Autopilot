# Install Godot Autopilot

> **For AI agents:** read the raw file, not the rendered GitHub page:
> `https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/INSTALL.md`.
> Follow the agent flow below before running any command.

Godot Autopilot (GDA) has two installable parts:

- **Editor plugin** — a GDExtension that runs inside the Godot editor and exposes the MCP server. It is installed per Godot project.
- **`gda` CLI** — discovers the plugin's random port, speaks MCP, installs the plugin and skill books, and starts or stops editors. It is installed once per machine.
- **Skill books** — optional Agent Skills that teach an AI client the GDA calling protocol. They can be installed globally or per project.

GDA does **not** install Godot itself. A Godot 4.7+ editor must already be available.

---

## 中文速览

- 推荐路径：在 Godot 工程根目录执行一键脚本。脚本会安装 `gda` CLI、识别 `project.godot` 并安装插件；技能书按你的选择装到全局或项目内。
- 全局技能目录：`~/.agents/skills`（Windows：`%USERPROFILE%\.agents\skills`）。
- 项目级技能目录：`<project>/.agents/skills`。
- Agent 在安装前必须问你：技能书装全局、装项目内，还是跳过。
- 一键脚本未识别到 Godot 工程时只安装 CLI，不安装插件；此时传入 `--project <工程目录>`。
- 安装后用 `gda status --json` 读取 `mcp_url`，再用 `gda ping` 验证。
- 不要用 `--headless` 验证：无窗口编辑器模式下插件会禁用 UI 与服务器。
- 详细的手动安装、源码构建、客户端配置与排错见下文英文正文。

---

## Requirements

| Item | Requirement |
|---|---|
| Godot | 4.7 or newer, graphical editor session |
| OS | Windows x64, Linux x64 (glibc or musl), macOS arm64 or x64 |
| Network | GitHub Releases access for the one-click and manual paths |
| Shell | PowerShell 5.1+ on Windows; bash on macOS/Linux |
| Build from source | Git, CMake 3.28+, C++17 compiler, Bun 1.4+, optional Ninja |

The plugin and the CLI should come from the same release. Unless you deliberately need a mixed version, install both from the same tag.

---

## For AI agents — read this first

You are installing GDA into a Godot project on this machine.

### Step 1 — Find the target project

1. If the current working directory contains `project.godot`, use the current directory.
2. Otherwise, walk up from the current directory and use the nearest ancestor containing `project.godot`.
3. Otherwise, use an absolute path explicitly supplied by the user.
4. Otherwise, **stop and ask** for the absolute path. Do not guess, do not scan sibling directories, and do not create a new project.

Resolve the path and confirm the file is a regular file, not a directory.

### Step 2 — Find Godot

Run `godot --version` or `godot4 --version`. If neither is on `PATH`, ask the user for the editor executable and pass it later with `--godot`. Do not install Godot yourself.

### Step 3 — Ask about skill books

Ask the user exactly one scope question before installing:

> Install the GDA skill books globally for this user (`~/.agents/skills`, shared by all Godot projects), only inside this project (`<project>/.agents/skills`), or skip them?

Do not choose silently. If the session is non-interactive and the user did not specify a scope, skip the skill books and say so in the final report.

- Global: pass `--skills` to the one-click script, or run `gda install skills --force`.
- Project: pass `--skills-project`, or run `gda install skills --path "<project>/.agents/skills" --force`.
- Skip: pass neither flag.

Add `--zh` if the user wants Chinese skill books.

### Step 4 — Install

Use the one-click script by default:

- macOS/Linux: `scripts/install.sh`
- Windows: `scripts/install.ps1`

Pass the project path explicitly even if the script could auto-detect it. This makes the result deterministic.

If the one-click script fails because of network or checksum, fall back to **Option B — Manual install from GitHub Releases**. Releases before 0.3.3 are not supported for plugin install; upgrade or use **Option C**. If the user asked to build from source or needs a custom build, use **Option C**.

### Step 5 — Verify before reporting

Run the verification commands in [Verify the installation](#verify-the-installation). Do not report success based only on an installer exit code.

### Rules

- Never use `sudo`, an elevated PowerShell, or administrator rights.
- Never expose the GDA port beyond loopback; never proxy or forward it.
- Do not pass `--confirm` to high-risk tools during installation (`destructive`, `process`, `code_execute`, `game_runtime`).
- Do not hand-write HTTP requests to the MCP port.
- Do not use `--headless` for installation verification. The plugin disables the server when the editor has no drawable window.
- Modify only `addons/godot-autopilot`, the selected skill directory, and the MCP client configuration. Do not rewrite project files.
- If a step fails, report the command, exit code, and the relevant output. Do not silently skip a failed check.
- If the user asked for a specific release, pin `--version <version>`; otherwise use the latest release.

### Final report

Include:

- Which install path was used (one-click, manual release, or source build).
- Installed CLI version and path.
- Project path and plugin path.
- Skill scope and skill directory, or “skipped”.
- `mcp_url` from `gda status --json`.
- Every command that failed, with its exit code.
- Any remaining risk (for example, the release predates 0.3.3 and cannot install the plugin).

---

## Option A — One-click script (recommended)

### macOS / Linux

Run from the Godot project root:

```bash
curl -fsSL https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.sh \
  | bash -s -- --project "$PWD" --skills
```

Use `--skills-project` instead of `--skills` for project-only skill books. Add `--zh` for Chinese books.

### Windows PowerShell

```powershell
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.ps1))) `
  -Project (Get-Location).Path -Skills
```

Use `-SkillsProject` instead of `-Skills` for project-only skill books. Add `-Zh` for Chinese books.

If you prefer to review the script before running it:

```powershell
$script = Join-Path $env:TEMP "gda-install.ps1"
irm https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.ps1 -OutFile $script
notepad $script
& $script -Project (Get-Location).Path -Skills
```

### What the script does

1. Resolves the latest GitHub Release, or uses `--version` when supplied.
2. Downloads the `gda` binary for the current OS and CPU.
3. Verifies it against the release's `sha256sums.txt`.
4. Installs it to `~/.godot-autopilot/bin` and adds that directory to the user `PATH` unless `--no-path` is set.
5. Detects a Godot project from `--project`, `GDA_PROJECT`, the current directory, or the nearest parent directory.
6. Installs the plugin into `<project>/addons/godot-autopilot` when a project was found and `--no-plugin` was not set.
7. Installs skill books globally or per project when requested.

The plugin step is project-scoped. If no project is found, the script installs only the CLI and prints an instruction to re-run with `--project`.

### Flags

| Flag | Meaning |
|---|---|
| `--version <version>` | Install a specific release; accepts `0.3.3` or `v0.3.3` |
| `--prefix <dir>` | CLI install directory; default `~/.godot-autopilot/bin` |
| `--archive <path>` | Use a local release asset or a directory containing it |
| `--project <dir>` | Godot project directory; must contain `project.godot` |
| `--skills` | Install skill books globally to `~/.agents/skills` |
| `--skills-project` | Install skill books only into `<project>/.agents/skills` |
| `--zh` | Use Chinese skill books; requires `--skills` or `--skills-project` |
| `--no-plugin` | Install the CLI only; skip the plugin even if a project is found |
| `--no-path` | Do not modify the user `PATH` |
| `--skip-checksum` | Allow missing checksum metadata; checksum mismatches still fail |
| `--dry-run` | Print the planned actions without installing |

PowerShell uses the same names with `-Version`, `-Prefix`, `-Archive`, `-Project`, `-Skills`, `-SkillsProject`, `-Zh`, `-NoPlugin`, `-NoPath`, `-SkipChecksum`, and `-DryRun`.

### Version compatibility

- The plugin path requires **0.3.3 or newer**. That release includes `addons.zip` in `sha256sums.txt` and writes a standard ZIP that Windows `Expand-Archive`, bsdtar, and `unzip` can open.
- On 0.3.2 and older, `sha256sums.txt` only covers the CLI binaries and the `addons.zip` archive was produced by the old packager. Those releases are not supported for plugin install; upgrade to 0.3.3+ or build from source.
- The script tries both `v<version>` and `<version>` tag forms, so releases tagged either way work.

---

## Option B — Manual install from GitHub Releases

Use this path when the one-click script is unavailable, when you need a specific version, or when the CLI cannot download the plugin archive.

### B1 — Install the `gda` CLI

Open the latest release:

`https://github.com/jesspig/Godot-Autopilot/releases/latest`

Download the asset for this machine:

| OS / CPU | Asset |
|---|---|
| Windows x64 | `gda-<version>-windows-x64.zip` |
| Linux x64, glibc | `gda-<version>-linux-x64.tar.gz` |
| Linux x64, musl | `gda-<version>-linux-x64-musl.tar.gz` |
| macOS Apple Silicon | `gda-<version>-macos-arm64.tar.gz` |
| macOS Intel | `gda-<version>-macos-x64.tar.gz` |

Also download `sha256sums.txt` from the same release.

Verify the archive:

```bash
# Linux
grep "gda-<version>-linux-x64.tar.gz" sha256sums.txt | sha256sum -c -

# macOS
grep "gda-<version>-macos-arm64.tar.gz" sha256sums.txt | shasum -a 256 -c -
```

```powershell
# Windows PowerShell
$asset = "gda-<version>-windows-x64.zip"
$line = (Select-String -Path sha256sums.txt -Pattern ([regex]::Escape($asset)) | Select-Object -First 1)
if (-not $line) { throw "$asset is not listed in sha256sums.txt" }
$expected = $line.Line.Split()[0]
$actual = (Get-FileHash $asset -Algorithm SHA256).Hash.ToLower()
if ($expected -ne $actual) { throw "checksum mismatch: $asset" }
```

Extract and install:

```bash
# macOS / Linux
tar -xzf gda-<version>-<platform>.tar.gz
chmod +x gda
mkdir -p ~/.godot-autopilot/bin
mv gda ~/.godot-autopilot/bin/gda
export PATH="$HOME/.godot-autopilot/bin:$PATH"
gda --version
```

```powershell
# Windows PowerShell
Expand-Archive -Path gda-<version>-windows-x64.zip -DestinationPath $env:TEMP\gda-cli -Force
New-Item -ItemType Directory -Force -Path "$HOME\.godot-autopilot\bin" | Out-Null
Move-Item -Force "$env:TEMP\gda-cli\gda.exe" "$HOME\.godot-autopilot\bin\gda.exe"
$env:Path = "$HOME\.godot-autopilot\bin;$env:Path"
gda --version
```

macOS: clear the quarantine attribute only after the checksum matches:

```bash
xattr -d com.apple.quarantine ~/.godot-autopilot/bin/gda
```

Windows: if SmartScreen marked the downloaded ZIP, right-click the ZIP, choose Properties, and unblock it before extracting.

### B2 — Install the editor plugin

Recommended, using the CLI:

```bash
gda install plugin --project "<project>"
```

This writes `<project>/addons/godot-autopilot`, stops a running editor for that project, and restarts it when one was running.

To install the plugin without `gda install plugin`, download `addons.zip` from a 0.3.3+ release and extract it manually:

```bash
# macOS / Linux
curl -fL -o addons.zip https://github.com/jesspig/Godot-Autopilot/releases/latest/download/addons.zip
unzip -o addons.zip -d "<project>"
```

```powershell
# Windows PowerShell
Invoke-WebRequest -Uri "https://github.com/jesspig/Godot-Autopilot/releases/latest/download/addons.zip" -OutFile addons.zip -UseBasicParsing
Expand-Archive -Path addons.zip -DestinationPath "<project>" -Force
```

The ZIP roots at `addons/godot-autopilot`, so extract it at the project root. Verify:

```text
<project>/addons/godot-autopilot/godot-autopilot.gdextension
<project>/addons/godot-autopilot/godot-autopilot.dll       # Windows
<project>/addons/godot-autopilot/libgodot-autopilot.so     # Linux
<project>/addons/godot-autopilot/libgodot-autopilot.dylib  # macOS
```

macOS may quarantine the downloaded ZIP or library. If Godot cannot load the plugin, clear it after verifying:

```bash
xattr -dr com.apple.quarantine "<project>/addons/godot-autopilot"
```

### B3 — Install skill books

Choose one scope:

```bash
# Global, shared by all projects
gda install skills --force

# Project-only
gda install skills --path "<project>/.agents/skills" --force

# Chinese
gda install skills --force --zh
gda install skills --path "<project>/.agents/skills" --force --zh
```

Do not install both scopes unless the user explicitly wants both; some clients may discover duplicate skills.

---

## Option C — Build from source and install

Use this path for contributing, custom builds, or when release binaries are unavailable.

### C1 — Prerequisites

- Git with submodule support
- CMake 3.28+
- A C++17 compiler (Visual Studio 2022, clang, or GCC)
- Bun 1.4+
- Optional: Ninja. Without Ninja, the devtools automatically use the `*-portable` CMake presets.
- Godot 4.7+ for verification

### C2 — Clone and build

```bash
git clone --recurse-submodules https://github.com/jesspig/Godot-Autopilot.git
cd Godot-Autopilot
bun install
bun run gda-dev-tools build --release
bun run gda-dev-tools deploy --release --demos 2d_dodge_the_creeps
bun run gda-dev-tools package
```

If the repository was cloned without submodules:

```bash
git submodule update --init --recursive
```

The package step produces `dist/godot-autopilot-<version>.zip` for the host platform. Extract it into the target project:

```bash
unzip -o dist/godot-autopilot-<version>.zip -d "<project>"
```

```powershell
Expand-Archive -Path dist\godot-autopilot-<version>.zip -DestinationPath "<project>" -Force
```

Alternatively, copy the deployed addon directory:

```bash
# macOS / Linux
mkdir -p "<project>/addons"
cp -R examples/2d_dodge_the_creeps/addons/godot-autopilot "<project>/addons/"
```

```powershell
# Windows PowerShell
New-Item -ItemType Directory -Force -Path "<project>\addons" | Out-Null
Copy-Item -Recurse -Force "examples\2d_dodge_the_creeps\addons\godot-autopilot" "<project>\addons\"
```

Verify the same files listed in B2.

### C3 — Install the CLI from the checkout

The CLI runs directly from TypeScript with Bun:

```bash
bun cli/bin/gda.ts --version
bun cli/bin/gda.ts install skills --force
```

Or build a standalone binary:

```bash
bun build cli/bin/gda.ts --compile --minify --outfile dist/gda
./dist/gda --version
```

On Windows the compiled file is `dist\gda.exe`; run `dist\gda.exe --version`.

### C4 — Verify the source build

```bash
bun run typecheck
bun test cli/tests
bun run gda-dev-tools test
```

A source build contains only the host platform library. Use the release pipeline when you need Windows, Linux, and macOS libraries in one package.

---

## Connect your MCP client

1. Start a **graphical** Godot editor session:
   ```bash
   gda start --project "<project>"
   ```
   Or open the project in the Godot editor yourself.
2. Read the connection record:
   ```bash
   gda status --json
   ```
   The only URL a client needs is `mcp_url`, for example `http://127.0.0.1:21437/mcp`.
3. Add that URL to the client configuration. Examples:
   - Claude Code (`.mcp.json`):
     ```json
     {"mcpServers":{"godot-autopilot":{"type":"http","url":"http://127.0.0.1:21437/mcp"}}}
     ```
   - Cursor (`.cursor/mcp.json`):
     ```json
     {"mcpServers":{"godot-autopilot":{"url":"http://127.0.0.1:21437/mcp"}}}
     ```
   - OpenCode (`opencode.json`):
     ```json
     {"mcp":{"godot-autopilot":{"type":"remote","url":"http://127.0.0.1:21437/mcp","enabled":true}}}
     ```
   - Codex (`.codex/config.toml`):
     ```toml
     [mcp_servers.godot-autopilot]
     url = "http://127.0.0.1:21437/mcp"
     ```
   - Other clients: use the same `mcp_url`.
4. Restart or reconnect the client and confirm that the GDA tools appear.

The port is chosen and persisted by the plugin, not by the CLI. Never reuse a port from documentation or from another project; read `mcp_url` again after every editor restart.

Do not use `--headless` for this check. The plugin disables the UI and server when the editor has no drawable window.

---

## Verify the installation

Run these in order:

```bash
gda --version
gda list
gda status --json
gda ping
gda search "scene"
gda call ping
```

Expected:

- `gda --version` prints the installed version.
- `gda list` shows a live entry for the project, or tells you no instance is running.
- `gda status --json` contains `"ok": true` and a non-empty `mcp_url`.
- `gda ping` succeeds.
- `gda search "scene"` returns candidates from the live instance.
- `gda call ping` returns a successful MCP response.

If `gda status --json` fails, start the graphical editor first and retry. If it still fails, check that the plugin directory exists and that the editor loaded the plugin.

---

## Upgrade

The installer is idempotent. Re-run the same one-click command to replace the CLI and plugin:

```bash
curl -fsSL https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.sh \
  | bash -s -- --project "$PWD" --skills
```

- The CLI binary is replaced in `~/.godot-autopilot/bin`.
- The plugin is replaced in `<project>/addons/godot-autopilot`; the installer keeps a timestamped backup.
- Skill books are overwritten because the script passes `--force`.
- Re-read `mcp_url` after the upgrade. The plugin persists a port per project, but the value is authoritative only after the editor restarts.

For manual upgrades, replace the CLI binary and re-run `gda install plugin --project "<project>"`.

---

## Uninstall

1. Stop the editor for the project.
2. Remove the plugin:
   ```text
   <project>/addons/godot-autopilot
   ```
3. Remove the CLI and its cache:
   ```text
   ~/.godot-autopilot
   ```
4. Remove skill books for the scope you installed:
   ```text
   ~/.agents/skills/godot-autopilot*
   <project>/.agents/skills/godot-autopilot*
   ```
5. Remove the MCP client entry that points at the old `mcp_url`.
6. Remove `~/.godot-autopilot/bin` from the user `PATH` if you added it.

---

## Troubleshooting

| Symptom | Check | Fix |
|---|---|---|
| `gda: command not found` | `gda --version` | Use the full path `~/.godot-autopilot/bin/gda`, or add that directory to `PATH` and open a new shell. |
| Download fails with 404 | Release tag and asset name | Use the one-click script or `releases/latest`; the script tries both `v<version>` and `<version>` tags. |
| `checksum mismatch` | Downloaded archive and `sha256sums.txt` | Delete the cached archive and retry. Do not bypass the mismatch. |
| `addons.zip is not listed in sha256sums.txt` | Release version | Releases 0.3.2 and older are not supported for plugin install; upgrade to 0.3.3+ or build from source. |
| `no running Godot Autopilot instance` | Editor is open and graphical | Open the project in a graphical editor, or run `gda start --project "<project>"` without `--headless`. |
| Plugin missing | `addons/godot-autopilot/godot-autopilot.gdextension` | Re-run `gda install plugin --project "<project>"`, or extract `addons.zip` manually. |
| GDExtension load error | Godot output panel and the library file name | Confirm the library matches the OS/CPU, clear the macOS quarantine attribute, and re-extract `addons.zip`. |
| `godot: command not found` | `godot --version` | Pass the editor path with `gda start --godot <path> --project "<project>"`. |
| macOS refuses to run `gda` | Quarantine attribute | Verify the checksum first, then `xattr -d com.apple.quarantine ~/.godot-autopilot/bin/gda`. |
| Windows SmartScreen warning | Zone identifier on the ZIP | Unblock the ZIP before extracting. |
| Port conflict or stale URL | `gda status --json` | Re-read `mcp_url`; do not hardcode a port. |
| Skills install fails because files exist | Existing skill books | Re-run with `--force`, or choose a different `--path`. |
| Headless editor has no server | Editor has no drawable window | Use a graphical editor session; `GDA_FORCE_HEADLESS` and headless mode disable the plugin UI and server. |

---

## Security

- The MCP server binds loopback only. Never expose, proxy, or forward the port.
- High-risk tools (`destructive`, `process`, `code_execute`, `game_runtime`) require `confirm=true`; there is no global allow list.
- Do not pass `--confirm` during installation. Review the tool description and target before confirming a high-risk call.
- Verify downloaded archives against `sha256sums.txt` before installing.
- Prefer the official GitHub Releases. If you use a mirror, set `GDA_GITHUB_BASE` only to a source you trust.
- The CLI and plugin are installed in user directories; do not run the installers as root or administrator.
- The plugin does not install Godot or modify project files other than `addons/godot-autopilot`.
