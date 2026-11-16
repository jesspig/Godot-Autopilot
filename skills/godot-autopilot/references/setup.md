# Setup and Connection

Use this reference when GDA is not installed, the project has no
`addons/godot-autopilot`, or the CLI cannot find an editor instance. The `gda`
CLI is the first-class interface; the plugin owns the random port.

## Inspect the machine

Collect three facts before installing:

```bash
gda --version
godot --version
test -f project.godot && echo "project ok"
```

Record the OS and CPU architecture. Releases ship one standalone `gda` binary
per target, so the architecture selects the asset. If `gda --version` works, skip
the CLI install. If Godot is not on `PATH`, locate the editor and pass `--godot`
when starting.

## Install the CLI

Use the standalone binary from the release page and verify it against
`sha256sums.txt` before putting it on `PATH`. On macOS, clear the quarantine
attribute only after the checksum matches. On Windows, unblock the file if
SmartScreen marked it.

A source checkout can run the CLI directly:

```bash
bun install
bun cli/bin/gda.ts --version
```

Do not pin a Git ref or package version in project scripts. Update the CLI and
plugin together with `gda update all`; a version pinned in one place drifts from
the other.

## Install the editor plugin

Run from the directory containing `project.godot`:

```bash
gda install plugin --project .
```

The installer writes `addons/godot-autopilot`, stops a running editor for that
project, and restarts it when one was running. With no network, use
`--source cache` if a cached archive exists; the builtin source does not embed
the plugin archive. Verify the result:

```bash
test -f addons/godot-autopilot/godot-autopilot.gdextension && echo "plugin ok"
```

## Start and verify the connection

```bash
gda start --project .
gda list
gda status --json
gda ping
```

`gda start` waits for the plugin's instance record. `gda list` shows the instance
id, project, port, transport, and liveness. `gda status --json` is the
authoritative connection record:

- `mcp_url` is the only endpoint an MCP client needs.
- `transport` and `ipc_kind` describe the local channel.
- `port_source` explains why the port was chosen.
- `plugin_version` identifies the running plugin.

The plugin chooses and persists the port. Never copy a port from documentation
or from another project, and never reuse one across editor restarts. `gda ping`
confirms the MCP handshake; if it fails, run `gda list` first. No live instance
means the editor is closed, the plugin is disabled, or the project path is wrong.

## Install the skill cards

Skills are independent of the plugin and update separately:

```bash
gda install skills
gda install skills --zh
gda install skills --offline
gda install skills --path "<project>/.agents/skills"
gda install skills --prune
```

The default target is `~/.agents/skills`. `--zh` selects Chinese content;
`--offline` uses the cache or builtin archive. Switching an existing install
between languages needs `--force` because the names are the same and the content
differs. `--prune` removes retired skill directories from the target; without
it, the command reports `stale` entries and deletes nothing. `gda skills list`
and `gda skills show <name>` work without a running editor.

## MCP client configuration

When a client should see native MCP tools, generate its entry from `mcp_url`:

```json
{
  "mcpServers": {
    "godot-autopilot": {
      "url": "<mcp_url>"
    }
  }
}
```

Re-read `mcp_url` after every editor restart and reconnect. The server listens on
loopback only; never forward, proxy, or publish the port.

## First end-to-end check

```bash
gda search scene
gda describe create_scene_node
gda call ping
gda call system_status
gda call get_editor_state --args '{"action": "info"}'
```

`gda search` needs a live editor. `gda describe` shows the schema and
`side_effect` marker before the first write. Read
`new_errors_since_last_call` in every response; zero means no new engine errors
were recorded.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| `gda` not found | Binary is not on `PATH` | Install the standalone binary and reopen the shell |
| No instance | Editor closed, plugin disabled, or wrong project | `gda list --all`, then open the project or `gda start --project .` |
| Plugin missing | No `addons/godot-autopilot` | `gda install plugin --project .` |
| Connection refused | Stale port or restarted editor | Re-read `mcp_url`; never reuse the old port |
| Skills missing | Not installed or wrong language | `gda skills list`, then `gda install skills` with `--zh` if needed |
| Retired books remain | Old install without cleanup | `gda install skills --prune` |
