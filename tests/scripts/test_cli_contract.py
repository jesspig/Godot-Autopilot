#!/usr/bin/env python3
"""Dual-carrier CLI mock MCP conformance tests.

Carrier 1 (legacy, behavior frozen): skills/godot-autopilot-tools/scripts/gda.mjs
driven by node, plus a core subset driven by bun when bun is available.
Carrier 2 (new CLI): cli/bin/gda.ts driven by bun, pointed at the same
in-process mock MCP server via --port / GODOT_AUTOPILOT_PORT.

Spins up an in-process mock MCP server (http.server on 127.0.0.1 with a
random port) that mimics the real server's wire behavior, then drives both
CLIs and asserts stdout JSON, exit codes, wire shapes, and side effects.

SKIP semantics: no node -> whole script prints SKIP and exits 0; no bun ->
the bun subsets (legacy core subset and new CLI group) print SKIP and the
script continues without failing.

Stdlib only. Never writes inside the repo (image drops go to tempfile).
Each CLI invocation has a timeout; the mock server runs on a daemon thread
and is shut down at the end.
"""

import base64
import http.server
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEGACY_SCRIPT = ROOT / "skills" / "godot-autopilot-tools" / "scripts" / "gda.mjs"
CLI_SCRIPT = ROOT / "cli" / "bin" / "gda.ts"

CALL_TIMEOUT = 30

PNG_B64 = (
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQG"
    "AhKmMIQAAAABJRU5ErkJggg=="
)

SEARCH_RESULTS = [{"name": "mock_tool", "score": 0.99}]


class MockHandler(http.server.BaseHTTPRequestHandler):
    initialized = False
    lock = threading.Lock()
    last_search_args = None
    last_direct_name = None
    last_wrapped_params = None

    def log_message(self, *args):
        pass

    def _send(self, status, content_type, body: bytes):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_json(self, status, obj):
        self._send(status, "application/json", json.dumps(obj).encode())

    def _send_sse(self, envelope, multiline=False):
        if multiline:
            head = '{"jsonrpc": "2.0", "id": %s,' % json.dumps(envelope.get("id"))
            tail = '"result": %s}' % json.dumps(envelope["result"])
            frame = f"event: message\ndata: {head}\ndata: {tail}\n\n"
        else:
            frame = f"event: message\ndata: {json.dumps(envelope)}\n\n"
        self._send(200, "text/event-stream", frame.encode())

    def _ok(self, req_id, business):
        self._send_sse(
            {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {"content": [{"type": "text", "text": json.dumps(business)}]},
            }
        )

    def do_GET(self):
        self._send_json(405, {"error": {"code": -32600, "message": "GET not supported"}})

    def do_POST(self):
        if self.path != "/mcp":
            self._send_json(404, {"error": {"code": -32601, "message": "not found"}})
            return
        try:
            length = int(self.headers.get("Content-Length", 0))
        except ValueError:
            length = 0
        try:
            body = json.loads(self.rfile.read(length) or b"{}")
        except (ValueError, OSError):
            self._send_json(400, {"error": {"code": -32700, "message": "parse error"}})
            return
        if not isinstance(body, dict):
            self._send_json(400, {"error": {"code": -32600, "message": "invalid request"}})
            return
        if body.get("method") == "notifications/initialized":
            with type(self).lock:
                type(self).initialized = True
            self._send_json(200, {})
            return
        if body.get("method") != "tools/call":
            self._send_json(
                400, {"jsonrpc": "2.0", "id": body.get("id"), "error": {"code": -32600, "message": "bad method"}}
            )
            return
        with type(self).lock:
            ready = type(self).initialized
        if not ready:
            self._send_json(
                400,
                {
                    "jsonrpc": "2.0",
                    "id": body.get("id"),
                    "error": {"code": -32600, "message": "Server not initialized"},
                },
            )
            return
        params = body.get("params", {}) if isinstance(body.get("params"), dict) else {}
        name = params.get("name")
        arguments = params.get("arguments", {}) if isinstance(params.get("arguments"), dict) else {}
        req_id = body.get("id")
        if name == "call_tool" and isinstance(arguments, dict):
            type(self).last_wrapped_params = params
            self._route_wrapped(req_id, arguments.get("name"), arguments.get("arguments", {}))
        else:
            type(self).last_direct_name = name
            self._route_direct(req_id, name, arguments if isinstance(arguments, dict) else {})

    def _route_direct(self, req_id, name, arguments):
        if name == "ping":
            self._ok(req_id, {"pong": True})
        elif name == "list_categories":
            self._ok(req_id, {"categories": ["mock-cat"]})
        elif name == "search_tools":
            with type(self).lock:
                type(self).last_search_args = arguments
            self._send_sse(
                {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {
                        "content": [
                            {"type": "text", "text": json.dumps({"result": {"results": SEARCH_RESULTS}})}
                        ]
                    },
                },
                multiline=True,
            )
        elif name == "get_tool_detail":
            tool = arguments.get("name", "mock_tool")
            self._ok(
                req_id,
                {"name": tool, "description": "mock tool", "inputSchema": {"type": "object", "properties": {}}},
            )
        elif name == "mock_503":
            self._send_json(503, {"error": {"code": -32603, "message": "editor busy"}})
        else:
            self._ok(req_id, {"error": {"code": -32602, "message": f"unknown tool: {name}"}})

    def _route_wrapped(self, req_id, inner, inner_args):
        if inner == "system_status":
            self._ok(req_id, {"ok": True, "engine": "mock", "status": "ready"})
        elif inner == "mock_echo":
            self._ok(req_id, {"echo": inner_args})
        elif inner == "mock_image":
            self._send_sse(
                {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {"content": [{"type": "image", "data": PNG_B64, "mimeType": "image/png"}]},
                }
            )
        elif inner == "mock_503":
            self._send_json(503, {"error": {"code": -32603, "message": "editor busy"}})
        else:
            self._ok(req_id, {"error": {"code": -32602, "message": f"unknown tool: {inner}"}})


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Context:
    def __init__(self, runtime, script, port, empty_dir):
        self.runtime = runtime
        self.script = script
        self.port = port
        self.empty_dir = empty_dir

    def run(self, *args, env_port=None, timeout=CALL_TIMEOUT):
        env = dict(os.environ)
        if env_port is None:
            env.pop("GODOT_AUTOPILOT_PORT", None)
        else:
            env["GODOT_AUTOPILOT_PORT"] = str(env_port)
        cmd = [self.runtime, str(self.script), *args]
        # node/bun 管道输出为 UTF-8；显式指定避免 Windows locale(GBK) 解码崩溃
        return subprocess.run(
            cmd, capture_output=True, encoding="utf-8", errors="replace", timeout=timeout, env=env
        )

    def run_port(self, *args, **kwargs):
        return self.run(*args, "--port", str(self.port), "--project-dir", self.empty_dir, **kwargs)


def check(name, cond, detail=""):
    if not cond:
        raise AssertionError(f"{name}: {detail}")


def run_legacy_cases(ctx, bun, port, empty_dir):
    """Carrier 1: gda.mjs via node plus the frozen bun core subset."""
    passed = 0

    MockHandler.initialized = False
    r = ctx.run_port("ping")
    check("lazy-handshake/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    check("lazy-handshake/retry", "notifications/initialized" in r.stderr, r.stderr[-500:])
    check("lazy-handshake/pong", "pong" in r.stdout, r.stdout[-500:])
    passed += 1

    r = ctx.run_port("ping")
    check("ping/exit", r.returncode == 0, r.stderr[-500:])
    check("ping/body", json.loads(r.stdout).get("pong") is True, r.stdout[-500:])
    check("ping/no-retry", "not needed" in r.stderr, r.stderr[-500:])
    passed += 1

    r = ctx.run_port("status")
    check("status/exit", r.returncode == 0, r.stderr[-500:])
    body = json.loads(r.stdout)
    check("status/body", body.get("ok") is True and body.get("engine") == "mock", r.stdout[-500:])
    passed += 1

    r = ctx.run_port("categories")
    check("categories/exit", r.returncode == 0, r.stderr[-500:])
    check("categories/body", "mock-cat" in r.stdout, r.stdout[-500:])
    passed += 1

    r = ctx.run_port("search", "mock")
    check("search/exit", r.returncode == 0, r.stderr[-500:])
    check("search/multiline-projection", json.loads(r.stdout) == SEARCH_RESULTS, r.stdout[-500:])
    passed += 1

    r = ctx.run_port("search", "mock", "--category", "mock-cat", "--tags", "a,b")
    check("search-filter/exit", r.returncode == 0, r.stderr[-500:])
    check("search-filter/body", json.loads(r.stdout) == SEARCH_RESULTS, r.stdout[-500:])
    seen = MockHandler.last_search_args
    check("search-filter/category", (seen or {}).get("category") == "mock-cat", repr(seen))
    check("search-filter/tags", (seen or {}).get("tags") == ["a", "b"], repr(seen))
    passed += 1

    r = ctx.run_port("describe", "mock_tool")
    check("describe/exit", r.returncode == 0, r.stderr[-500:])
    body = json.loads(r.stdout)
    check("describe/body", body.get("name") == "mock_tool" and "inputSchema" in body, r.stdout[-500:])
    passed += 1

    payload = {"x": 1, "s": "hi"}
    r1 = ctx.run_port("call", "mock_echo", "--args", json.dumps(payload))
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(payload, f)
        args_file = f.name
    try:
        r2 = ctx.run_port("call", "mock_echo", "--args-file", args_file)
    finally:
        os.unlink(args_file)
    check("call-args/exit", r1.returncode == 0 and r2.returncode == 0, f"{r1.returncode}/{r2.returncode}")
    check(
        "call-args/equivalent",
        json.loads(r1.stdout) == json.loads(r2.stdout) == {"echo": payload},
        f"{r1.stdout[-300:]} vs {r2.stdout[-300:]}",
    )
    passed += 1

    r = ctx.run_port("call", "no_such_tool_xyz")
    check("tool-error/exit1", r.returncode == 1, f"exit={r.returncode}")
    passed += 1

    r = ctx.run_port("call", "mock_503")
    check("http503/exit2", r.returncode == 2, f"exit={r.returncode} stderr={r.stderr[-300:]}")
    passed += 1

    closed = free_port()
    r = ctx.run("--port", str(closed), "--project-dir", empty_dir, "ping")
    check("refused/exit2", r.returncode == 2, f"exit={r.returncode}")
    passed += 1

    for uargs in (["boguscmd"], ["ping", "--category", "x"], ["describe"]):
        r = ctx.run_port(*uargs)
        check(f"usage/exit3:{uargs}", r.returncode == 3, f"exit={r.returncode} {r.stderr[-300:]}")
    passed += 1

    with tempfile.TemporaryDirectory() as img_dir:
        r = ctx.run_port("call", "mock_image", "--save-images", img_dir)
        check("image/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
        check("image/no-base64", PNG_B64[:40] not in r.stdout, r.stdout[-300:])
        body = json.loads(r.stdout)
        check("image/report", "images" in body and body["images"][0]["saved"] is True, r.stdout[-500:])
        pngs = list(Path(img_dir).glob("*.png"))
        check("image/file", len(pngs) == 1, os.listdir(img_dir))
        check("image/magic", pngs[0].read_bytes()[:8] == base64.b64decode(PNG_B64)[:8], "bad PNG header")
    passed += 1

    r = ctx.run("--port", str(port), "--project-dir", empty_dir, "ping", env_port=closed)
    check("port-flag/exit", r.returncode == 0, r.stderr[-500:])
    check("port-flag/wins", "pong" in r.stdout, r.stdout[-300:])
    passed += 1

    r = ctx.run("--project-dir", empty_dir, "ping", env_port=port)
    check("port-env/exit", r.returncode == 0, r.stderr[-500:])
    check("port-env/used", "pong" in r.stdout, r.stdout[-300:])
    passed += 1

    if bun is not None:
        bctx = Context(bun, LEGACY_SCRIPT, port, empty_dir)
        r = bctx.run_port("ping")
        check("bun/ping", r.returncode == 0 and "pong" in r.stdout, f"{r.returncode} {r.stderr[-300:]}")
        r = bctx.run_port("call", "mock_echo", "--args", '{"x":1}')
        check("bun/call", r.returncode == 0 and json.loads(r.stdout) == {"echo": {"x": 1}}, r.stdout[-300:])
        r = bctx.run_port("call", "no_such_tool_xyz")
        check("bun/tool-error", r.returncode == 1, f"exit={r.returncode}")
        passed += 1
        print("bun subset: ping + call + tool-error exit codes OK")
    else:
        print("SKIP bun subset: bun not found on PATH")
    return passed


def run_cli_cases(bctx, port):
    """Carrier 2: cli/bin/gda.ts via bun against the same mock server."""
    passed = 0

    def cli_run(*args, env_port=None):
        # 新 CLI 的 --port/--id/--project 三选一，故只附 --port，不再附 --project-dir
        return bctx.run(*args, "--port", str(port), env_port=env_port)

    MockHandler.initialized = False
    MockHandler.last_search_args = None
    MockHandler.last_direct_name = None
    MockHandler.last_wrapped_params = None

    r = cli_run("ping")
    check("cli-ping/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    check("cli-ping/body", json.loads(r.stdout) == {"pong": True}, r.stdout[-500:])
    check("cli-ping/compact", "\n" not in r.stdout.strip(), repr(r.stdout[-200:]))
    check("cli-ping/direct", MockHandler.last_direct_name == "ping", repr(MockHandler.last_direct_name))
    passed += 1

    r = cli_run("status")
    check("cli-status/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    check(
        "cli-status/body",
        json.loads(r.stdout) == {"ok": True, "engine": "mock", "status": "ready"},
        r.stdout[-500:],
    )
    check(
        "cli-status/wrapped",
        MockHandler.last_wrapped_params
        == {"name": "call_tool", "arguments": {"name": "system_status", "arguments": {}}},
        repr(MockHandler.last_wrapped_params),
    )
    passed += 1

    r = cli_run("categories")
    check("cli-categories/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    check("cli-categories/body", json.loads(r.stdout) == {"categories": ["mock-cat"]}, r.stdout[-500:])
    check("cli-categories/direct", MockHandler.last_direct_name == "list_categories", repr(MockHandler.last_direct_name))
    passed += 1

    r = cli_run("search", "q")
    check("cli-search/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    check("cli-search/projection", json.loads(r.stdout) == SEARCH_RESULTS, r.stdout[-500:])
    check("cli-search/direct", MockHandler.last_direct_name == "search_tools", repr(MockHandler.last_direct_name))
    passed += 1

    r = cli_run("describe", "mock_tool")
    check("cli-describe/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    body = json.loads(r.stdout)
    check("cli-describe/body", body.get("name") == "mock_tool" and "inputSchema" in body, r.stdout[-500:])
    check("cli-describe/direct", MockHandler.last_direct_name == "get_tool_detail", repr(MockHandler.last_direct_name))
    passed += 1

    r = cli_run("call", "mock_echo", "--args", '{"k":1}')
    check("cli-call-wrapped/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    check("cli-call-wrapped/body", json.loads(r.stdout) == {"echo": {"k": 1}}, r.stdout[-500:])
    check(
        "cli-call-wrapped/params",
        MockHandler.last_wrapped_params
        == {"name": "call_tool", "arguments": {"name": "mock_echo", "arguments": {"k": 1}}},
        repr(MockHandler.last_wrapped_params),
    )
    passed += 1

    r = cli_run("call", "ping")
    check("cli-call-direct/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    check("cli-call-direct/body", json.loads(r.stdout) == {"pong": True}, r.stdout[-500:])
    check("cli-call-direct/direct", MockHandler.last_direct_name == "ping", repr(MockHandler.last_direct_name))
    passed += 1

    r = cli_run("call", "no_such_tool_xyz")
    check("cli-tool-error/exit1", r.returncode == 1, f"exit={r.returncode}")
    passed += 1

    closed = free_port()
    r = bctx.run("--port", str(closed), "ping")
    check("cli-refused/exit2", r.returncode == 2, f"exit={r.returncode} stderr={r.stderr[-300:]}")
    passed += 1

    for uargs in (["describe"], ["call"], ["boguscmd"]):
        r = bctx.run(*uargs)
        check(f"cli-usage/exit3:{uargs}", r.returncode == 3, f"exit={r.returncode} {r.stderr[-300:]}")
    passed += 1

    r = cli_run("ping", "--pretty")
    check("cli-pretty/exit", r.returncode == 0, f"exit={r.returncode} stderr={r.stderr[-500:]}")
    check("cli-pretty/multiline", len(r.stdout.strip().splitlines()) > 1, repr(r.stdout[-200:]))
    check("cli-pretty/indent", '  "pong": true' in r.stdout, repr(r.stdout[-200:]))
    check("cli-pretty/body", json.loads(r.stdout) == {"pong": True}, r.stdout[-500:])
    passed += 1
    return passed


def main():
    node = shutil.which("node")
    if node is None:
        print("SKIP: node not found on PATH, mock MCP conformance tests skipped")
        return 0
    if not LEGACY_SCRIPT.is_file():
        print(f"FAIL: gda.mjs not found at {LEGACY_SCRIPT}")
        return 1
    bun = shutil.which("bun")

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), MockHandler)
    port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    passed = 0
    try:
        with tempfile.TemporaryDirectory() as empty_dir:
            passed += run_legacy_cases(Context(node, LEGACY_SCRIPT, port, empty_dir), bun, port, empty_dir)

            if bun is None:
                print("SKIP bun cli subset")
            else:
                if not CLI_SCRIPT.is_file():
                    print(f"FAIL: gda.ts not found at {CLI_SCRIPT}")
                    return 1
                cli_passed = run_cli_cases(Context(bun, CLI_SCRIPT, port, empty_dir), port)
                passed += cli_passed
                print(f"bun cli subset: {cli_passed} cases passed")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=10)

    print(f"cli-contract mock conformance: {passed} cases passed (node={node}, bun={bun})")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as e:
        print(f"FAIL: {e}")
        sys.exit(1)
    except subprocess.TimeoutExpired as e:
        print(f"FAIL: CLI call timed out: {e}")
        sys.exit(1)
