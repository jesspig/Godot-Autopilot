# Direct HTTP Access to the Godot Autopilot MCP Server

Companion reference of the godot-autopilot skill: how to talk to the server with plain HTTP requests (curl or PowerShell) when the normal MCP client channel is broken. The server itself is almost always fine.

## When to use this

- Your MCP client lost its tools after an editor restart: the tool list is empty, calls fail, or reconnecting did not help.
- The server starts automatically with the editor plugin, so it is typically still online - the break is usually in the client harness, not the server.
- You can bypass the harness by POSTing JSON-RPC straight to the server endpoint.

## Step 0 - Discover the actual port

Never assume the default 9527. The port is configurable and any of these sources can override it. Check in this order:

| Order | Where to look | What to read |
|---|---|---|
| 1 | MCP client config files in the project root: .mcp.json, .cursor/mcp.json, .codex/config.toml, opencode.json and similar | The URL the panel's Generate button wrote, of the form http://127.0.0.1:\<port\>/mcp - this is the actual address |
| 2 | Plugin config in the Godot user data folder (Windows): %APPDATA%\Godot\app_userdata\<ProjectName>\godot_autopilot\config.json | The "port" key; \<ProjectName\> is the config/name value from project.godot |
| 3 | Environment variable | GODOT_AUTOPILOT_PORT |
| 4 | Fallback | 9527 |

Whichever source you use, the endpoint path is always /mcp on 127.0.0.1.

## Protocol - two mandatory steps

All requests are POSTs to http://127.0.0.1:\<port\>/mcp with these headers:

- Content-Type: application/json
- Accept: application/json, text/event-stream
- No Authorization header and no session id header - the server is stateless.
- The Host must be localhost (127.0.0.1); the server rejects other Host values.

Step 1, one-time handshake: send a JSON-RPC notification (no id):

```json
{"jsonrpc":"2.0","method":"notifications/initialized"}
```

The server answers 202 with an empty object. Skipping this step is the most common failure: every later tools/call returns 400 "Server not initialized". Repeat step 1 once per editor session if the editor was restarted meanwhile; sending it again is harmless.

Step 2: call a tool. The JSON-RPC `id` is required and must not be null. Domain tools are proxied through `call_tool`:

```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"call_tool","arguments":{"name":"ping","arguments":{}}}}
```

## curl example (bash)

```bash
URL="http://127.0.0.1:9527/mcp"  # discover the real port first!
# Step 1: one-time handshake (required, returns 202)
curl -s -X POST "$URL" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","method":"notifications/initialized"}'
# Step 2: call a domain tool through call_tool
curl -s -X POST "$URL" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"call_tool","arguments":{"name":"ping","arguments":{}}}}'
```

`ping` is the safest first call. Creating a node works the same way - only the inner arguments change (omit `parent_path` only while the edited scene has no root):

```bash
curl -s -X POST "$URL" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"call_tool","arguments":{"name":"create_scene_node","arguments":{"type":"Node2D","name":"Player"}}}}'
```

## PowerShell example

```powershell
$Url = "http://127.0.0.1:9527/mcp"   # discover the real port first!
$accept = "application/json, text/event-stream"
# Step 1: one-time handshake (required, returns 202)
Invoke-WebRequest -Method Post -Uri $Url -ContentType "application/json" -Headers @{ Accept = $accept } -Body '{"jsonrpc":"2.0","method":"notifications/initialized"}'
# Step 2: call a domain tool through call_tool
$resp = Invoke-WebRequest -Method Post -Uri $Url -ContentType "application/json" -Headers @{ Accept = $accept } -Body '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"call_tool","arguments":{"name":"ping","arguments":{}}}}'
# The response body is an SSE frame: keep the first "data:" line and parse it.
$line = ($resp.Content -split "`n" | Where-Object { $_ -like 'data:*' } | Select-Object -First 1)
$json = ($line.Substring(5) | ConvertFrom-Json)
$inner = ($json.result.content[0].text | ConvertFrom-Json)
$inner.result                        # -> "pong"
$inner.new_errors_since_last_call    # error watermark, see the godot-autopilot skill
```

PowerShell single-quoted strings need no JSON double-quote escaping. If your PowerShell version renders Invoke-WebRequest output differently, cross-check against the curl example above.

## Reading the response

A successful call returns HTTP 200 with an SSE frame: lines of `event: message` followed by `data: <JSON>`. To get the business payload:

1. Take the line starting with `data:` and strip the prefix (in bash: pipe through `sed -n 's/^data: *//p'`).
2. Parse the remaining JSON - the JSON-RPC envelope. The tool result lives at result.content[0].text.
3. That text field is itself a JSON string; parse it again (inner layer).
4. The business payload is the inner "result" key; an inner "error" key means the tool call failed, and an outer isError of true marks the same thing.
5. Read the inner top-level `new_errors_since_last_call` - same one-shot semantics as on the MCP channel (see the godot-autopilot skill).

Full shape of a `ping` response (whitespace added for readability):

```json
{"jsonrpc":"2.0","id":1,"result":{"content":[{"type":"text",
"text":"{\"new_errors_since_last_call\":0,\"result\":\"pong\"}"}],"isError":false}}
```

## Troubleshooting

| HTTP status | Meaning | Fix |
|---|---|---|
| 400 "Server not initialized" | Step 1 handshake missing | Send the notifications/initialized notification first |
| 400 "tool not found: ..." or "domain tool '...' not found" | Wrong tool name in tools/call or call_tool | Discover exact names with search_tools |
| 403 | Host header missing or not localhost | Keep Host as 127.0.0.1 / localhost |
| 404 | GET request used | The endpoint only accepts POST |
| 413 | Request body over 4 MiB | Shrink the payload (split into batches) |
| 503 | More than 8 concurrent requests, or server not running | Is the editor open with the plugin enabled? Reduce parallel calls |
| 504 | Processing exceeded 30 seconds | Split the work into smaller calls |

Tool-level failures (bad arguments, missing nodes) are not HTTP errors - they arrive as HTTP 200 with an "error" key in the inner JSON.

## Constraints

- Loopback only, no authentication: never expose the port to the network and never use this against a remote machine.
- At most 8 concurrent in-flight requests.
- The handshake of step 1 is per editor session; redo it after the editor restarts.

## See also

- godot-autopilot - usage overview, discovery protocol and the error watermark
