---
name: godot-autopilot-setup
description: "Install and connect Godot Autopilot (GDA) from zero: detect the OS, Godot project and existing gda CLI, install the CLI and the editor plugin, install the English or Chinese skill books, start the editor, verify with gda status --json, and generate an MCP client configuration from mcp_url. Use when GDA is not installed yet, when a project has no addons/godot-autopilot, or when the CLI cannot find an editor instance."
---

# Godot Autopilot Setup

This volume takes a machine from nothing to a working GDA connection. It is the
first book to read on a new machine or a new project. Once setup is done, read
the `godot-autopilot` volume for the daily workflow and the
`godot-autopilot-tools` volume for the calling protocol.

The `gda` CLI is the first-class interface. It discovers the plugin's random
port, speaks MCP to it, and reports the only URL an MCP client needs:
`mcp_url` from `gda status --json`. Never hardcode a port and never hand-write
an MCP endpoint.

## 1. Inspect the machine before installing

Collect these facts first; every later step branches on them:

```bash
gda --version
godot --version
test -f project.godot && echo "project ok"
```

On PowerShell use `Get-Command gda`, `Get-Command godot` and
`Test-Path project.godot`. Record the operating system and CPU architecture
(Windows x64, macOS arm64/x64, Linux x64/glibc, Linux x64/musl). The `gda`
release ships one standalone binary per target, so the architecture decides the
asset name.

If `gda --version` works, skip to step 3. If Godot is not on `PATH`, find the
editor executable and pass it explicitly with `--godot` when starting.

## 2. Install the CLI

Use the standalone `gda` binary from the release page; it needs no JavaScript
runtime. Download the asset matching the OS and architecture, verify it against
the release's `sha256sums.txt`, then put it on `PATH`:

```bash
sha256sum -c sha256sums.txt --ignore-missing
chmod +x gda
mv gda ~/.local/bin/gda
gda --version
```

On macOS, clear the quarantine attribute only after the checksum matches:
`xattr -d com.apple.quarantine ./gda`. On Windows, unblock the file from the
file properties dialog if SmartScreen marked it.

A source checkout can run the CLI directly with Bun:

```bash
bun install
bun cli/bin/gda.ts --version
```

Do not pin a Git ref or a package version in project scripts. The CLI and the
plugin are updated together with `gda update all`; a version pinned in one
place drifts from the other.

## 3. Install the editor plugin

Run from the Godot project root (the directory containing `project.godot`):

```bash
gda install plugin --project .
```

The installer writes `addons/godot-autopilot`, stops a running editor for that
project, and restarts it when one was running. Use `--source cache` when the
machine is offline and a cached plugin archive exists. The builtin source does
not embed the plugin archive; if it is unavailable, use `online` or `cache`.

Verify the install:

```bash
test -f addons/godot-autopilot/godot-autopilot.gdextension && echo "plugin ok"
```

## 4. Start the editor and verify the connection

```bash
gda start --project .
gda list
gda status --json
gda ping
```

`gda start` launches the editor and waits for the plugin's instance record.
`gda list` shows the instance id, project, port, transport and liveness.
`gda status --json` is the authoritative connection record. The fields that
matter for a client are:

```json
{
  "ok": true,
  "project": "/path/to/project",
  "port": 21437,
  "port_source": "persisted",
  "mcp_url": "http://127.0.0.1:21437/mcp",
  "transport": "ipc",
  "ipc_kind": "unix",
  "plugin_version": "<plugin version>",
  "toolset_hash": "1a2b3c4d"
}
```

The port is chosen and persisted by the plugin, not by the CLI. Never copy a
port from documentation or from another project. Read `mcp_url` from this
output every time; it stays correct after an editor restart.

`gda ping` confirms the MCP handshake. If it fails, run `gda list` first: no
live instance means the editor is not open, the plugin is disabled, or the
project path is wrong.

## 5. Install the skill books

Skills are separate from the plugin and can be updated independently:

```bash
gda install skills
gda install skills --zh
gda install skills --offline
gda skills list
```

The default target is `~/.agents/skills`. Pass `--path <dir>` to install into a
project-local directory instead. `--zh` selects the Chinese books; `--offline`
never touches the network and uses the cached or builtin archive. Switching an
existing install between languages needs `--force`, because the book names are
the same and the contents differ. `gda skills list` prints the installed books
with their file counts, and `gda skills show <name>` prints one book without a
running editor.

## 6. Generate the MCP client configuration (optional)

Use GDA through the CLI whenever a shell is available. When the client should
also see native MCP tools, generate its configuration from the live status
record instead of editing a template by hand:

1. Run `gda status --json` and read `mcp_url`.
2. Put that URL into the client's MCP server entry. A streamable HTTP entry
   looks like this:

```json
{
  "mcpServers": {
    "godot-autopilot": {
      "url": "http://127.0.0.1:21437/mcp"
    }
  }
}
```

3. Restart or reconnect the client and confirm that the GDA tools appear.
4. If the tools disappear after an editor restart, re-read `mcp_url` and
   reconnect; the port may have changed, but the discovery command always has
   the current value.

The server listens on loopback only. Never forward, proxy, or publish the port.
There is no command that installs an MCP configuration for a client; the agent
generates it from `mcp_url`, because only the agent knows which client is in
use.

## 7. First end-to-end check

```bash
gda search "scene"
gda describe create_scene_node
gda call ping
gda call system_status
```

`gda search` queries the live instance and needs a running editor.
`gda describe` shows the input schema and `side_effect` marker before the first
write. `gda call` executes against the live editor and prints the result
JSON. Read the `new_errors_since_last_call` watermark in every response; zero
means no new engine errors were recorded.

## Troubleshooting

| Symptom | Check | Action |
|---|---|---|
| `gda` not found | `gda --version` | Install the standalone binary and add its directory to `PATH`. |
| No instance | `gda list --all` | Open the project, or run `gda start --project .`. |
| Plugin missing | `addons/godot-autopilot` exists | Run `gda install plugin --project .`. |
| Plugin disabled | Editor Plugins dialog | Enable Godot Autopilot and restart the editor. |
| Connection refused | `gda status --json` | Re-read `mcp_url`; do not reuse an old port. |
| Skills missing | `gda skills list` | Run `gda install skills` or `--zh`; use `--offline` when disconnected. |
| Confirmation error | Response `code` | Review the target, then retry with `--confirm`. |
| Offline update | `gda cache list` | Use `--source cache`; the builtin source only carries skills. |

## Safety rules

- The MCP endpoint is loopback-only and has no authentication. Keep it local.
- High-risk calls (`destructive`, `process`, `code_execute`, `game_runtime`)
  return `confirmation_required`; pass `--confirm` only after reviewing the
  tool description and the target path.
- Prefer a read-back over assuming a write landed. `ok` means the call was
  delivered, not that the effect matches the intent.

## See also

- `godot-autopilot` - the daily entry point: connection, routing, observation.
- `godot-autopilot-tools` - CLI command reference, discovery, confirm, errors.
- `godot-autopilot-scene-system` - scenes, nodes, properties, signals.
- `godot-autopilot-resources` - resources, imports, rename and move.
- `godot-autopilot-scripting` - GDScript creation and execution channels.
- `godot-autopilot-runtime` - running and debugging the game.
- `godot-autopilot-servers` - RenderingServer, PhysicsServer, NavigationServer.
- `godot-autopilot-content` - tilemap, animation, audio, UI theming.
- `godot-autopilot-csharp` - C# projects, build loop, reflection bridge.
