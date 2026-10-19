# gda CLI and the Standalone Binary

Companion reference of the godot-autopilot-tools volume: how to reach the server from a shell when the native MCP channel is unavailable — through `bunx`, through a globally installed `gda`, or through a self-contained binary that needs no JavaScript runtime at all.

## When this channel applies

Enter here when either holds:

- **No MCP client is configured.** The user installed the plugin in Godot and the project contains `addons/godot-autopilot`, but the harness has no GDA MCP server wired up, so no GDA tools appear in your tool list. This is the common case for a first contact: the plugin is running, the agent simply has no native channel.
- **The client lost its tools.** The tool list went empty or calls fail right after an editor restart and reconnecting did not help.

If the native MCP tools are present and working, use them instead — this reference is a fallback, not a preference.

## Which carrier to use

Probe the machine before choosing: `bun --version`, then `node --version`.

| Precondition | Carrier | Cost |
|---|---|---|
| Bun 1.1 or newer | `bunx` the pinned tag | No install; instant once cached |
| Bun, used repeatedly | `bun install -g`, then plain `gda` | One install, zero download afterwards |
| Node 18 or newer, no Bun | The bundled script bridge (references/script-access.md) | Offline, no download |
| Neither Bun nor Node | Standalone binary from the Release page | One download per version |

All carriers speak the same MCP protocol, accept the same subcommands and share the same exit codes. Nothing in the calling protocol changes when you switch carriers.

## Commands

Calling family — meta tools are called directly, every domain tool goes through `call_tool`:

| Command | Meaning |
|---|---|
| `ping` | Health check; safest first call |
| `status` | Server status and project info |
| `categories` | List all tool categories |
| `search <query>` | Search tools by keyword (optionally `--category` / `--tags`) |
| `describe <tool>` | Full schema and side-effect marker for one tool |
| `call <tool>` | Execute one tool; arguments via `--args '<json>'` or `--args-file <path>` |

Lifecycle family — these act on editor processes on the local machine:

| Command | Meaning |
|---|---|
| `list` | List registered instances |
| `start` | Launch an editor and wait until its server answers |
| `stop <id>` | Stop an instance gracefully; `--force` to kill |
| `logs <id>` | Tail the instance log |

## Global options

`--port` (default 9527), `--id`, `--project`, `--pretty`, `--raw`, `--quiet`, `--timeout`, `--category`, `--tags`, `--args`, `--args-file`, `--save-images`, `--godot`, `--headless`, `--play`, `--no-import`, `--wait`, `--force`, `--tail`, `--help`, `--version`.

`--id`, `--port` and `--project` are mutually exclusive. Without any of them the chain is: `GODOT_AUTOPILOT_PORT` environment variable, then the current directory's registered instance, then the project's server-ready trace and `config.json`, then the default 9527.

## Engine resolution for `start`

`start` needs the Godot executable. It resolves `--godot` first, then a PATH lookup (`godot.exe`/`godot4.exe` on Windows, `godot`/`godot4` elsewhere). If neither hits, it fails with `cannot locate the Godot executable (pass --godot or put godot on PATH)`.

The `GODOT_PATH` environment variable is **not** read, and neither is a `.env` in the project: that variable exists only for the L2 engine test runner. When the user's engine is not on PATH, pass `--godot` explicitly with the absolute path they gave you.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Tool failure (the payload carries an error) |
| 2 | Transport or environment failure (connection refused, timeout, HTTP error, missing engine) |
| 3 | Usage error (unknown command, missing argument, bad option) |

stdout carries the payload only; diagnostics go to stderr prefixed with `[gda]`.

## Standalone binary

Release assets are named `gda-<version>-<platform>` where platform is one of `windows-x64` (`.zip`, contains `gda.exe`), `linux-x64`, `linux-x64-musl`, `macos-arm64`, `macos-x64` (`.tar.gz`, contains `gda`). Each release also ships `sha256sums.txt`.

Download the asset that matches the user's OS and CPU from the release page, verify it against the checksum file, then run it exactly like `gda`:

```
sha256sum -c sha256sums.txt --ignore-missing
chmod +x gda
./gda ping
```

Platform notes:

- **macOS** — binaries are ad-hoc signed. If the first launch is blocked by quarantine, clear the attribute with `xattr -d com.apple.quarantine ./gda` (or have the user allow it in System Settings). Do not claim the binary is notarized; it is not.
- **Linux** — the plain `linux-x64` asset needs glibc; use `linux-x64-musl` on Alpine or musl-based images.
- **Windows** — unzip and run `gda.exe`. An unsigned binary may trigger a SmartScreen prompt; that is expected.

Prefer `bunx` over the binary whenever Bun exists: it is versioned with the plugin and needs no download.

## Hard rules

- **Never hand-write HTTP requests** against the server, with curl or otherwise. Every call goes through the MCP tools, `gda`, or the script bridge.
- **The server binds loopback only and has no authentication.** Never forward its port, never bind it to a public interface, never proxy it.
- **Verify a downloaded binary before executing it.** No checksum match, no run.
- **Report what you actually observed.** Exit code 2 means the channel is down, not that the tool returned nothing; do not fabricate results.
