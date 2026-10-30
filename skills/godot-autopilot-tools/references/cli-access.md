# The GDA CLI

Companion reference of the godot-autopilot-tools volume: how to reach the server from a shell when the native MCP channel is unavailable — through `bunx`, through a globally installed `gda`, or through a self-contained binary that needs no JavaScript runtime at all.

The GDA CLI is the only scripted calling channel. Every call goes through the harness MCP tools or this command; nothing else is sanctioned.

## When this channel applies

Enter here when either holds:

- **No MCP client is configured.** The user installed the plugin in Godot and the project contains `addons/godot-autopilot`, but the harness has no GDA MCP server wired up, so no GDA tools appear in your tool list. This is the common case for a first contact: the plugin is running, the agent simply has no native channel.
- **The client lost its tools.** The tool list went empty or calls fail right after an editor restart and reconnecting did not help.

If the native MCP tools are present and working, use them instead — this reference is a fallback, not a preference.

## Which distribution to use

Probe the machine before choosing: `bun --version`, then `node --version`.

| Precondition | Distribution | Cost |
|---|---|---|
| Bun 1.1 or newer | `bunx` a ref that actually ships the CLI (see the caveat below) | No install; instant once cached |
| Bun, used repeatedly | `bun install -g`, then plain `gda` | One install, zero download afterwards |
| Neither Bun nor Node | Standalone binary from the Release page | One download per version |

All three distributions are the same program: they speak the same MCP protocol, accept the same subcommands and share the same exit codes. Nothing in the calling protocol changes when you switch.

**Caveat — the pinned ref resolves only once its tag is published.** As of `0.3.0` the repository tree does ship the CLI: the root `package.json` exposes the `gda` binary as `cli/bin/gda.ts`, and `cli/` is part of the tree, so `bunx "github:jesspig/Godot-Autopilot#0.3.0"` resolves the `gda` binary as soon as the `0.3.0` tag exists on the remote. The older caveat — that the pinned tag was cut before the CLI existed, carried no `cli/` directory and no root `package.json`, and could never resolve — no longer applies to the current pin. What still applies is the publishing step: until the `0.3.0` tag and its release are out, that ref is not fetchable, so use `bunx "github:jesspig/Godot-Autopilot#develop" <cmd>` in the meantime (that branch does ship the CLI, but it is an unreleased moving target rather than a pinned version, so pin your own copy with `bun install -g` if you need a stable one). The versioned alternative is the standalone binary below: each release publishes the `gda` assets for five targets next to `addons.zip` with a `sha256sums.txt`, so check the release page for the asset and verify the checksum before downloading.

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

Lifecycle and control family — these manage editor processes or the local control plane:

| Command | Meaning |
|---|---|
| `list` | List instances from both registries, merged: CLI-started entries (`source: "cli"`) plus live plugin-index instances (`source: "plugin"`, deduplicated by pid/port), each with `live`; add `--probe <port>` to append a `source: "probe"` entry for one explicitly probed port (no port scanning) |
| `start` | Launch an editor and wait until its server answers |
| `stop <id>` | Stop an instance. A headless instance has no window to receive a close request, so it is force-stopped directly and reports `method: "force"` — that is expected, not a fallback. A windowed instance is stopped gracefully; `--force` kills it. The instance can also be addressed with `--id`, `--port` or `--project` instead of the positional id |
| `restart <id>` | Stop the instance and start it again on the same port, headless mode and Godot executable. Headless instances (and any instance with `--force`) are force-stopped; windowed instances stop gracefully. Same addressing as `stop`. The stop phase waits a fixed 10 seconds and ignores `--wait`; if a windowed process is still alive, no replacement is started and the registration is kept |
| `serve` | Start the loopback control-plane proxy (default `127.0.0.1:9700`) that routes to one or several instances; see the control-plane section below |
| `logs <id>` | Tail the instance log |

REST family — these do not speak MCP; they call the plugin's plain-HTTP face on the same port:

| Command | Meaning |
|---|---|
| `api list [query]` | List the registered tools through the REST face; `--query`, `--category` and `--tags` filter the result |
| `api describe <tool>` | One tool's schema and side-effect marker, through the REST face |
| `api call <tool>` | Call one tool through the REST face; `--args` or `--args-file` carry the argument object, which is the request body itself |
| `endpoints` | Show the discovered MCP and REST endpoints, the port they came from, and whether the port is alive |

The REST commands need a bearer token; the CLI reads it from the plugin's instance registry and attaches it. When no instance record is found the command fails with a transport error naming the port and its source, and points at the registry path. `--probe` picks the liveness check: `light` (a TCP connect, the default) or `strict` (TCP plus one HTTP request). The endpoint contract and the token sources are in references/rest-api.md.

Follow the same discovery order as the native channel: `ping`, then `search` or `categories`, then `describe`, then `call`. Calling with an unknown name fails with a message telling you to search — never guess names.

## Global options

`--port` (default 9527; 9700 for `serve`), `--id`, `--project`, `--pretty`, `--raw`, `--timeout`, `--category`, `--tags`, `--args`, `--args-file`, `--save-images`, `--godot`, `--headless`, `--play`, `--no-import`, `--wait`, `--force`, `--tail`, `--bind`, `--no-auth`, `--query`, `--probe`, `--help`, `--version`.

`--bind` and `--no-auth` apply to `serve` only; `--bind` takes a loopback address and `--no-auth` turns the control-plane token check off. `--query` applies to the `api list` subcommand only. `--probe` is overloaded: `light`/`strict` liveness level for `api`, `endpoints` and `start`, or a port number (1-65535) for `list` to probe exactly that port; passing it to another command is a usage error.

`--quiet` was removed and is no longer recognized. Passing it now fails with exit code 3 and the message "unknown option: --quiet", and it is gone from `gda --help`. An older wrapper that still passes `--quiet` will start failing — drop the flag rather than working around it. Diagnostics already go to stderr on their own, so the flag bought nothing.

`--id`, `--port` and `--project` are mutually exclusive, and `stop` and `restart` accept a positional instance id as a fourth way to address an instance; passing a positional id together with any of the three options is a usage error. Every project path in that chain is normalized with `path.resolve` semantics, so the same relative `--project` (for example `../x`) now resolves to the same instance under `ping`, `stop`, `restart` and `list` alike. Before, those commands disagreed on the same input and could address different instances.

## Port and target resolution

Never assume the default port. The plugin may have moved off 9527 when that port was already taken: an editor that finds 9527 occupied picks the next free port in a fixed range instead. The target is resolved in seven steps, and each step records where the port came from:

| Priority | Source | What to read |
|---|---|---|
| 1 | `--port` | The port you pass |
| 2 | `--id` | The instance id in the registry; the instance must still be alive |
| 3 | `GODOT_AUTOPILOT_PORT` environment variable | The port it holds |
| 4 | CLI registry | A live instance registered for `--project`, or for the current directory |
| 5 | Plugin instance registry | The plugin's own registry of running instances, matched by project path - this is where the REST token comes from too |
| 6 | Project server-ready trace and config.json | The port recorded by the project under the Godot user data folder |
| 7 | Fallback | 9527 |

`gda endpoints` prints the resolved port together with the step it came from, both endpoint views and the liveness result - that is the quickest way to find out where a port came from. The trace and the config file live under the Godot user data folder of the project. Pass `--port` explicitly whenever you already know the editor was configured with a custom port.

## Passing arguments

Domain calls carry the domain tool name plus its arguments object. Give the arguments inline with `--args`, or from a JSON file with `--args-file`:

```
gda call <tool> --args '{"type": "Node2D", "name": "Player", "parent_path": "Root/Actors"}'
gda call <tool> --args-file args.json
```

args.json holds the arguments object, for example:

```json
{"type": "Node2D", "name": "Player", "parent_path": "Root/Actors"}
```

`--args` and `--args-file` apply to the `call` command only; passing them to another command is a usage error. An unreadable or malformed arguments file is a usage error too, not a transport error.

## Engine resolution for `start`

`start` needs the Godot executable. It resolves `--godot` first, then a PATH lookup (`godot.exe`/`godot4.exe` on Windows, `godot`/`godot4` elsewhere). If neither hits, it fails with `cannot locate the Godot executable (pass --godot or put godot on PATH)`.

The `GODOT_PATH` environment variable is **not** read, and neither is a `.env` in the project: that variable exists only for the L2 engine test runner. When the user's engine is not on PATH, pass `--godot` explicitly with the absolute path they gave you.

## Session and lifecycle behaviour

- **A failed start is rolled back.** When the readiness probe fails, `start` terminates the editor process it launched and removes the pending log file, then reports the failure. A failed start therefore leaves no orphaned Godot process behind and no half-written log to clean up by hand.
- **A restarted server is picked up again.** The handshake state is tracked per target URL, and a server restart invalidates it. When the server answers a request with the "session not initialized" rejection, the CLI re-handshakes once per URL per process and replays the request. The same CLI process therefore survives an editor restart instead of failing with an HTTP 400 or a -32600 error for the rest of its life.

## Control plane (`gda serve`)

`gda serve` starts a loopback-only proxy that gives one entry point to several editor instances. It is a pure proxy: it does not add tools, does not change any semantics and does not start or stop editors.

- **Listen**: `127.0.0.1:9700` by default. `--port` changes the port; `--bind` accepts loopback addresses only (`127.0.0.1`, `::1`, `localhost`) and rejects anything else; `--timeout` sets the upstream wait (default 60 seconds).
- **Routes**: `GET /control/health` needs no token. `GET /control/instances` lists live instances (never their tokens) and needs a token. `/i/<ref>/api/...` and `/i/<ref>/mcp` forward to the chosen instance. `<ref>` accepts three forms, matched in this order: the plugin's `instance_id` (for example `gda-daa757f3-9672-1792697320437`), the CLI id `p<pid>` (for example `p9672`, exactly what `gda list` prints, matched by process id), then a numeric port (for example `9530`). Plain `/api/...` and `/mcp` forward to the single live instance; with several live instances they return 409 `instance_ambiguous` with `candidates`, and with none they return 503 `no_active_instance`.
- **Auth**: send `Authorization: Bearer <token>`. The token is generated on first start at `<home>/.godot-autopilot/control.token` (64 hex characters, mode `0600`) and reused afterwards. `--no-auth` disables the check with a warning — every local process can then drive the editor.
- **Forwarding**: the proxy drops the client's `Authorization` header, attaches the target instance's own token, and streams the response through unchanged (SSE included), adding `X-GDA-Instance` with the instance that served the call.
- **Not for process control**: there are no HTTP start/stop endpoints by design; use `gda start` / `stop` / `restart` for that.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Tool failure (the payload carries an error) |
| 2 | Transport or environment failure (connection refused, timeout, HTTP error, missing engine) |
| 3 | Usage error (unknown command, missing argument, bad option) |

A response that carries a top-level JSON-RPC `error` envelope is a tool failure, not a transport failure: it exits 1 and the envelope is printed as the payload. The same holds for a payload that carries an inner error. Exit 2 is reserved for the channel itself being unusable — the server is not listening, the endpoint answered with an HTTP error, or the request timed out. Do not read exit 2 as "the tool returned nothing".

## Output contract

- stdout carries the payload only, as compact JSON; `--pretty` indents it by two spaces.
- `--raw` prints the unprocessed response envelope instead, with image data replaced by a byte-count placeholder.
- Diagnostics go to stderr, prefixed with `[gda]`.
- A tool failure is exit code 1 with the business payload on stdout: read the error object inside the payload, not the transport message.

## Image rules

Screenshot tools (`capture_editor_viewport`, `capture_game_viewport`, `capture_display_screen`) return image payloads. By default the CLI reports image metadata only — mime type and byte count — and writes nothing to disk. Pass `--save-images DIR` to decode the payloads into DIR under self-generated file names (gda_<timestamp>_<index>.png). Inspect the saved files the same way you would inspect image content on the native channel before trusting an edit.

## Standalone binary

The release workflow builds the `gda` binary for five platforms and attaches the assets plus a checksum file to the GitHub release. The binaries are not in every release: the release cut before the `cli` job joined the workflow still carries only the addon package, so check the release page for the asset before you promise a download to the user.

Once a release ships them, assets are named `gda-<version>-<platform>` where platform is one of `windows-x64` (`.zip`, contains `gda.exe`), `linux-x64`, `linux-x64-musl`, `macos-arm64`, `macos-x64` (`.tar.gz`, contains `gda`). Each such release also ships `sha256sums.txt`.

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

Prefer `bunx` over the binary whenever Bun exists: it needs no download. It is pinned to the plugin version only once that version's tag is published, so the `develop` ref above moves under you — install your own copy with `bun install -g` when you need a stable CLI.

## Troubleshooting

| Symptom | Meaning | Fix |
|---|---|---|
| Connection refused | Editor closed, plugin disabled, or wrong port | Open the editor with the plugin enabled; run `gda endpoints` to see which port was resolved and where it came from; pass `--port` explicitly |
| Timeout (exit 2) | Work unit too large for the wait budget | Split into smaller calls; `--timeout` only extends the client wait, server caps still apply |
| Unknown tool name (exit 1) | Guessed or stale tool name | Discover exact names with `search` and confirm with `describe`; never invent names |
| Authorization denied (exit 1) | The tool's capability is not authorized | Enable it via the `GODOT_AUTOPILOT_ALLOW` environment variable plus engine restart, or the matching MCP Config dock checkbox effective on the next call; write-class tools need `write` |
| No API token available (exit 2) | The plugin's instance registry has no record for that port | Run `gda endpoints` to find the port and its source; the token lives in the plugin instance registry, not in the CLI registry |
| Empty or missing image output | Metadata-only default | Re-run with `--save-images DIR` and inspect the files on disk |
| bunx cannot resolve the CLI | The pinned tag is not published yet (the tree does ship the CLI; the ref only resolves once the tag exists) | Use the `#develop` ref or a global install; see the caveat above |

Tool-level failures (bad arguments, missing nodes) are not transport errors — they arrive as exit code 1 with an error key in the business payload. Handle them with the same error protocol as the native channel: read the watermark, diagnose from real output, confirm the fix by driving the watermark back to 0.

## Hard rules

- **Never hand-write HTTP requests** against the MCP endpoint from any tool. Every MCP call goes through the MCP tools or `gda`. The plugin's REST face has its own reference (references/rest-api.md) and is the one sanctioned place to speak plain HTTP directly.
- **The server binds loopback only.** Never forward its port, never bind it to a public interface, never expose it through a proxy. `gda serve` is the only sanctioned local proxy and is itself loopback-only; it is not a way to reach the server from another machine. The MCP face has no authentication, so the loopback binding is the only thing protecting it; the REST face asks for a bearer token, but that token is a local credential and does not make the port safe to expose.
- **Verify a downloaded binary before executing it.** No checksum match, no run.
- **Report what you actually observed.** Exit code 2 means the channel is down, not that the tool returned nothing; do not fabricate results.

## See also

- godot-autopilot-tools — channel ladder, discovery protocol, error protocol and limits
- references/rest-api.md — the plain-HTTP face: endpoints, token sources and status codes
- references/manual-fallback.md — what to do when no CLI is available either
