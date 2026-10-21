# The REST API

Companion reference of the godot-autopilot-tools volume: the plain-HTTP face of the plugin, for clients that cannot speak the MCP protocol.

The plugin listens on **one** port and serves two faces on it. The MCP face is the primary one and the one this volume's ladder prefers. The REST face is the alternative for a client that can only issue ordinary HTTP requests — no MCP handshake, no session, no JSON-RPC framing.

This is an **interface reference**, not a new scripted channel. The only scripted calling channel remains the `gda` CLI (see references/cli-access.md). The two snippets below show the shape of a REST call; when a shell is available, prefer `gda api`.

## When this face applies

Enter here when either holds:

- **The client cannot do an MCP handshake.** It can send HTTP requests with a bearer token but has no MCP client, no session handling and no JSON-RPC framing. Some HTTP-first agent harnesses and simple integrations land here.
- **You need a plain probe.** `GET /api/health` answers without a token and returns only liveness, the service version, the bound port and the process id.

If the harness lists native GDA tools, use them. If a shell is available, use the CLI. This face is for the remaining case.

## Endpoints

The path prefix is `/api`. All of them need `Authorization: Bearer <token>` **except** the health probe.

| Method | Path | What it does |
|---|---|---|
| GET | `/api` | Service name, version and the endpoint list |
| GET | `/api/tools` | The registered tools; filter with `query`, `category`, `tags`; page with `limit` and `offset` |
| GET | `/api/tools/<tool>` | One tool's description, category, tags, side-effect marker and input schema |
| POST | `/api/tools/<tool>` | Call one tool — **the request body is the argument object itself** |
| POST | `/api/batch` | Call several tools in order |
| GET | `/api/health` | Liveness probe — **no token required** |
| GET | `/api/openapi.json` | The OpenAPI document for the face |

The tool name in the path may be percent-encoded; only letters, digits, underscore, hyphen and dot are accepted.

## Calling a tool

The body **is** the argument object. There is no `name` / `arguments` wrapper on this face, unlike the MCP `call_tool` meta tool.

```json
{"type": "Node2D", "name": "Player", "parent_path": "Root/Actors"}
```

An empty body means "no arguments". A body that is not a JSON object is rejected with 400. Extra keys that the schema does not declare are ignored rather than rejected; a missing required key or a wrong type is an error.

Batch bodies carry an ordered operation list:

```json
{
  "operations": [
    {"tool": "create_scene_node", "args": {"type": "Node2D", "name": "Player"}},
    {"tool": "property_set", "args": {"path": "Root/Player", "property": "position", "value": {"x": 1, "y": 2}}}
  ],
  "stop_on_error": true
}
```

A batch always answers 200. Per-operation outcomes live in the `results` array: each entry carries the tool name, a status, and either the payload or an error. When `stop_on_error` is true the first failure stops the run and the remaining operations are not executed; the response then says how many ran and how many were skipped.

## Getting the token

The token is a 32-byte random value in hexadecimal (64 lowercase hex characters). Three ways to get it, in order of precedence:

1. **The `gda` CLI does it for you.** `gda api list`, `gda api describe <tool>` and `gda api call <tool>` read the token from the plugin's instance registry and attach the header. This is the recommended path.
2. **Read it from the instance registry.** The plugin writes one record per running instance into `instances.json` under the GDA home directory (the `.godot-autopilot` folder in the user's home directory; set `GODOT_AUTOPILOT_HOME` to move it). Each record's **api_token** field holds the bearer token for that instance, and the same record names the port the instance actually bound.
3. **Read the token file.** The plugin also writes the token into the per-project storage folder as a file named **api_token**. On POSIX the file is tightened to owner-only permissions; on Windows it inherits the ACL of the GDA home directory.

If the editor was started with the `GODOT_AUTOPILOT_API_TOKEN` environment variable set, that value is the token and no file is written.

Never print the token into a log, a commit, or a chat transcript. Treat it like a local credential.

## Status codes

| Code | Meaning | What to do |
|---|---|---|
| 200 | Success — including a batch whose individual operations failed | Read the payload |
| 400 | Bad request: malformed body, missing `operations`, bad `limit` or `offset`, bad tool arguments | Fix the request; do not retry unchanged |
| 401 | No token, wrong header shape, or a token that does not match | Read the token again from the instance registry |
| 403 | The tool's capability is not authorized | Enable the capability; see the authorization note below |
| 404 | Unknown route or unknown tool name | Discover the tool name first; never guess |
| 405 | The route does not accept this method | Use the method from the endpoint table |
| 409 | Conflicting request | Retry later |
| 413 | The request body is too large | Split the payload |
| 422 | The tool itself failed | Read the error text; the call reached the tool |
| 429 | Rate limited | Retry in a moment |
| 500 | Internal error | Check the plugin log |
| 503 | The editor is exporting, or the main-thread queue is unavailable | Wait for the export to finish, or retry later for the queue |
| 504 | The editor main thread did not finish inside its budget | **Do not retry blindly** — see below |

A failure body carries an `error` string and a `code` string. Branch on the code, not on the wording.

Two retries deserve care. The main-thread timeout code is marked as cancelled and as **not retryable**: the call was abandoned, but the side effect may still land in the editor afterwards. Read the editor state before deciding to send it again. The queue-unavailable code **is** retryable — that one means the channel itself was momentarily busy.

## Python, minimal

Standard library only; no third-party package needed.

```python
import json
import os
import urllib.request

BASE = "http://127.0.0.1:9527/api"
TOKEN = os.environ["GDA_API_TOKEN"]


def call(method, path, payload=None):
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        BASE + path,
        data=body,
        method=method,
        headers={
            "Authorization": f"Bearer {TOKEN}",
            "Content-Type": "application/json",
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


print(call("GET", "/health"))  # the health probe needs no token
print(call("GET", "/tools?query=scene"))
print(call("POST", "/tools/create_scene_node", {"type": "Node2D", "name": "Player"}))
```

## JavaScript, minimal

`fetch` is built in on Node 18+ and on Bun; no package needed.

```javascript
const BASE = "http://127.0.0.1:9527/api";
const TOKEN = process.env.GDA_API_TOKEN;

async function call(method, path, payload) {
  const response = await fetch(BASE + path, {
    method,
    headers: {
      Authorization: `Bearer ${TOKEN}`,
      "Content-Type": "application/json",
    },
    body: payload === undefined ? undefined : JSON.stringify(payload),
  });
  return await response.json();
}

console.log(await call("GET", "/health"));
console.log(await call("GET", "/tools?query=scene"));
console.log(await call("POST", "/tools/create_scene_node", { type: "Node2D", name: "Player" }));
```

## Authorization on this face

A valid token proves who is calling; it does not grant anything. The capability gate still applies, and it is checked on every call:

- **Read** tools — those whose side effect is `none` — are allowed by default.
- **Write** tools — `writes_file`, `writes_config`, `shows_alert`, `modifies_window` — are **denied by default**. They need the `write` capability in the allow list.
- **Danger** tools — `process`, `code_execute`, `game_runtime`, `destructive` — are denied by default and need their own capability.

Enable a capability with the `GODOT_AUTOPILOT_ALLOW` environment variable (a comma-separated list, or `all`) and an engine restart, or with the matching checkbox in the plugin's MCP Config dock ("Allow code_execute", "Allow game_runtime", "Allow user tools", "Allow write", "Allow destructive"), which saves to the plugin config and takes effect on the next call with no restart. Un-ticking a box removes only that one capability. A denied call answers 403 and names the capability it needs — a write-class call answers 403 authorization_required unless `write` is allowed. This face cannot widen the allow list per request — there is no field in the body that does that.

## Hard rules

- **Loopback only.** The server binds `127.0.0.1`. Never forward the port, never proxy it, never bind it to a public interface.
- **The token is a local credential.** It only works together with the loopback binding. Do not forward it, do not log it, do not commit it.
- **Discover before calling.** Read the tool list first; the tool names in this reference are examples, and the real set is whatever the tool list returns.
- **A timeout is not a retry signal.** Read the editor state before repeating a call that ended in a main-thread timeout.

## See also

- godot-autopilot-tools — channel ladder, discovery protocol, error protocol and limits
- references/cli-access.md — the `gda` CLI, including the REST subcommands and the port resolution chain
