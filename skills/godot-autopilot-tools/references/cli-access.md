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
| Bun 1.1 or newer | `bunx` a ref that actually ships the CLI (see the caveat below) | No install; instant once cached |
| Bun, used repeatedly | `bun install -g`, then plain `gda` | One install, zero download afterwards |
| Node 18 or newer, no Bun | The bundled script bridge (references/script-access.md) | Offline, no download |
| Neither Bun nor Node | Standalone binary from the Release page | One download per version |

All carriers speak the same MCP protocol, accept the same subcommands and share the same exit codes. Nothing in the calling protocol changes when you switch carriers.

**Caveat — the pinned tag does not ship the CLI.** The release tag this repository pins was cut before the `gda` CLI existed: that tag's tree has no `cli/` directory and no root `package.json`, so `bunx "github:jesspig/Godot-Autopilot#0.2.7"` cannot resolve a `gda` binary and fails on every attempt. Do not retry it. Use `bunx "github:jesspig/Godot-Autopilot#develop" <cmd>` instead — that branch does ship the CLI, but it is an unreleased moving target rather than a pinned version, so pin your own copy with `bun install -g` if you need a stable one. The versioned alternative is the standalone binary below, once a release actually ships the `gda` assets: check the release page for the asset and its `sha256sums.txt` before downloading, and use the script bridge in the meantime.

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
| `stop <id>` | Stop an instance gracefully; `--force` to kill. The instance can also be addressed with `--id`, `--port` or `--project` instead of the positional id |
| `logs <id>` | Tail the instance log |

## Global options

`--port` (default 9527), `--id`, `--project`, `--pretty`, `--raw`, `--timeout`, `--category`, `--tags`, `--args`, `--args-file`, `--save-images`, `--godot`, `--headless`, `--play`, `--no-import`, `--wait`, `--force`, `--tail`, `--help`, `--version`.

`--quiet` was removed and is no longer recognized. Passing it now fails with exit code 3 and the message "unknown option: --quiet", and it is gone from `gda --help`. An older script or wrapper that still passes `--quiet` will start failing — drop the flag rather than working around it. Diagnostics already go to stderr on their own, so the flag bought nothing.

`--id`, `--port` and `--project` are mutually exclusive, and `stop` accepts a positional instance id as a fourth way to address an instance; passing a positional id together with any of the three options is a usage error. Without any of them the chain is: `GODOT_AUTOPILOT_PORT` environment variable, then the current directory's registered instance, then the project's server-ready trace and `config.json`, then the default 9527.

Every project path in that chain is normalized with `path.resolve` semantics, so the same relative `--project` (for example `../x`) now resolves to the same instance under `ping`, `stop` and `list` alike. Before, the three commands disagreed on the same input and could address different instances.

## Engine resolution for `start`

`start` needs the Godot executable. It resolves `--godot` first, then a PATH lookup (`godot.exe`/`godot4.exe` on Windows, `godot`/`godot4` elsewhere). If neither hits, it fails with `cannot locate the Godot executable (pass --godot or put godot on PATH)`.

The `GODOT_PATH` environment variable is **not** read, and neither is a `.env` in the project: that variable exists only for the L2 engine test runner. When the user's engine is not on PATH, pass `--godot` explicitly with the absolute path they gave you.

## Session and lifecycle behaviour

- **A failed start is rolled back.** When the readiness probe fails, `start` terminates the editor process it launched and removes the pending log file, then reports the failure. A failed start therefore leaves no orphaned Godot process behind and no half-written log to clean up by hand.
- **A restarted server is picked up again.** The handshake state is tracked per target URL, and a server restart invalidates it. When the server answers a request with the "session not initialized" rejection, the CLI re-handshakes once per URL per process and replays the request. The same CLI process therefore survives an editor restart instead of failing with an HTTP 400 or a -32600 error for the rest of its life.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Tool failure (the payload carries an error) |
| 2 | Transport or environment failure (connection refused, timeout, HTTP error, missing engine) |
| 3 | Usage error (unknown command, missing argument, bad option) |

A response that carries a top-level JSON-RPC `error` envelope is a tool failure, not a transport failure: it exits 1 and the envelope is printed as the payload. The same holds for a payload that carries an inner error. Exit 2 is reserved for the channel itself being unusable — the server is not listening, the endpoint answered with an HTTP error, or the request timed out. Do not read exit 2 as "the tool returned nothing".

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

Prefer `bunx` over the binary whenever Bun exists: it needs no download. It is not version-pinned with the plugin until a release tag that ships the CLI exists, so the `develop` ref above moves under you — install your own copy with `bun install -g` when you need a stable CLI.

## Hard rules

- **Never hand-write HTTP requests** against the server, with curl or otherwise. Every call goes through the MCP tools, `gda`, or the script bridge.
- **The server binds loopback only and has no authentication.** Never forward its port, never bind it to a public interface, never proxy it.
- **Verify a downloaded binary before executing it.** No checksum match, no run.
- **Report what you actually observed.** Exit code 2 means the channel is down, not that the tool returned nothing; do not fabricate results.
