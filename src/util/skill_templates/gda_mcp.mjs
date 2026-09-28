#!/usr/bin/env node
// gda_mcp.mjs - call the Godot Autopilot (GDA) MCP server over plain HTTP.
//
// This script pins the two-step protocol documented in tools--script-access.md
// into code so callers do not hand-craft requests: it resolves the server
// port, POSTs a JSON-RPC tools/call to http://127.0.0.1:<port>/mcp,
// performs the lazy initialized-handshake on demand, and prints the
// business payload as indented JSON.
//
// Request flow notes:
// - The server answers successful calls with HTTP 200 and a single SSE
//   frame (event: message plus data: <JSON-RPC>). Continuation lines that
//   start with "data:" are joined with "\n" before JSON parsing.
// - Error answers are plain JSON, not SSE. GET is not supported.
// - Meta tools (ping, search_tools, list_categories, get_tool_detail,
//   batch_execute, code_execute) are called directly; domain tools and
//   system_status are wrapped through call_tool.
// - Only serial requests are sent, well below the 8 in-flight limit.
// - The request body must stay under 4 MiB; a single HTTP round trip uses
//   AbortSignal.timeout (--timeout, default 60 s).
//
// Portability and safety notes:
// - Only Node/Bun globals (fetch, AbortSignal, Buffer, TextDecoder,
//   process, console) plus node:fs are used. Argument parsing is
//   hand-written. No third-party dependencies.
// - The target host is the hard-coded 127.0.0.1 literal; only the port is
//   adjustable (validated to 1-65535).
// - Proxy environment variables are intentionally ignored; fetch keeps its
//   default direct-connection behavior for loopback.
// - Response content is only JSON-parsed, never evaluated or executed.
// - The only disk writes are decoded images inside --save-images DIR with
//   self-generated file names. --args-file only reads a JSON file.

import fs from "node:fs";

const HOST = "127.0.0.1";
const DEFAULT_PORT = 9527;
const DEFAULT_TIMEOUT_SEC = 60;
const MAX_BODY_BYTES = 4 * 1024 * 1024;
const JSONRPC_METHOD = "tools/call";
const META_DIRECT_TOOLS = new Set([
  "ping",
  "search_tools",
  "list_categories",
  "get_tool_detail",
  "batch_execute",
  "code_execute",
]);

function makeEnvelope(id, name, args, direct) {
  const params = direct
    ? { name, arguments: args }
    : { name: "call_tool", arguments: { name, arguments: args } };
  return { jsonrpc: "2.0", id, method: JSONRPC_METHOD, params };
}

function usageText() {
  return [
    "Usage: gda_mcp.mjs [options] <command> [arguments]",
    "",
    "Commands:",
    "  ping                              Call the ping meta tool.",
    "  status                            Call system_status via call_tool.",
    "  categories                        Call the list_categories meta tool.",
    "  search [query] [--category C] [--tags a,b]",
    "                                    Call search_tools; prints only [{name, score}].",
    "  describe <tool>                   Call get_tool_detail for <tool>.",
    "  call <tool> [--args JSON | --args-file FILE]",
    "                                    Call a meta tool directly, or a domain tool",
    "                                    (and system_status) wrapped via call_tool.",
    "",
    "Options:",
    "  --port N            Server port (1-65535). Highest priority port source.",
    "  --project-dir DIR   Project directory used to find project.godot (default: cwd).",
    "  --timeout SEC       Per-request HTTP timeout in seconds (default: 60).",
    "  --save-images DIR   Decode image payloads into DIR as gda_<timestamp>_<n>.png.",
    "  --raw               Print the full JSON-RPC envelope (image bytes redacted).",
    "  --quiet             Suppress informational [gda] stderr lines.",
    "  --help, -h          Show this help.",
    "",
    "Port resolution (first hit wins, failures fall through silently):",
    "  1) --port  2) GODOT_AUTOPILOT_PORT  3) newest trace server_ready port",
    "  4) godot_autopilot/config.json port  5) 9527.",
    "",
    "Exit codes: 0 success, 1 tool-level failure, 2 transport failure, 3 usage error.",
    "",
    "Examples:",
    "  node gda_mcp.mjs ping",
    "  node gda_mcp.mjs search \"spawn enemy\" --category scene-system",
    "  node gda_mcp.mjs call create_scene_node --args '{\"type\":\"Node2D\",\"name\":\"Player\"}'",
  ].join("\n");
}

function usageError(message) {
  console.error(`[gda] usage error: ${message}`);
  console.error("[gda] run with --help for usage.");
  process.exit(3);
}

function transportError(message, suggestion) {
  console.error(`[gda] transport error: ${message}`);
  if (suggestion) console.error(`[gda] suggestion: ${suggestion}`);
  process.exit(2);
}

function parseArgv(argv) {
  const opts = {
    portText: undefined,
    projectDir: undefined,
    timeoutText: undefined,
    saveImages: undefined,
    raw: false,
    quiet: false,
    help: false,
    category: undefined,
    tagsText: undefined,
    argsJson: undefined,
    argsFile: undefined,
  };
  const positionals = [];
  const takeValue = (flag, value, inline) => {
    if (inline !== undefined) return inline;
    if (value === undefined || value.startsWith("-")) usageError(`${flag} requires a value.`);
    return value;
  };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--") {
      positionals.push(...argv.slice(i + 1));
      break;
    }
    const eq = a.indexOf("=");
    const flag = eq === -1 || !a.startsWith("--") ? a : a.slice(0, eq);
    const inline = eq === -1 ? undefined : a.slice(eq + 1);
    if (!a.startsWith("-")) {
      positionals.push(a);
    } else if (flag === "--port") {
      opts.portText = takeValue("--port", argv[i + 1], inline);
      if (inline === undefined) i++;
    } else if (flag === "--project-dir") {
      opts.projectDir = takeValue("--project-dir", argv[i + 1], inline);
      if (inline === undefined) i++;
    } else if (flag === "--timeout") {
      opts.timeoutText = takeValue("--timeout", argv[i + 1], inline);
      if (inline === undefined) i++;
    } else if (flag === "--save-images") {
      opts.saveImages = takeValue("--save-images", argv[i + 1], inline);
      if (inline === undefined) i++;
    } else if (flag === "--category") {
      opts.category = takeValue("--category", argv[i + 1], inline);
      if (inline === undefined) i++;
    } else if (flag === "--tags") {
      opts.tagsText = takeValue("--tags", argv[i + 1], inline);
      if (inline === undefined) i++;
    } else if (flag === "--args") {
      opts.argsJson = takeValue("--args", argv[i + 1], inline);
      if (inline === undefined) i++;
    } else if (flag === "--args-file") {
      opts.argsFile = takeValue("--args-file", argv[i + 1], inline);
      if (inline === undefined) i++;
    } else if (a === "--raw") {
      opts.raw = true;
    } else if (a === "--quiet") {
      opts.quiet = true;
    } else if (a === "--help" || a === "-h") {
      opts.help = true;
    } else {
      usageError(`unknown option: ${a}`);
    }
  }
  return { opts, positionals };
}

function sep() {
  return process.platform === "win32" ? "\\" : "/";
}

function joinPath(...parts) {
  return parts.filter((p) => p !== "").join(sep());
}

function isAbsolutePath(p) {
  if (process.platform === "win32") return /^[A-Za-z]:[\\/]/.test(p) || p.startsWith("\\\\");
  return p.startsWith("/");
}

function resolvePath(p) {
  if (isAbsolutePath(p)) return p;
  return joinPath(process.cwd(), p);
}

function validPort(value) {
  return Number.isInteger(value) && value >= 1 && value <= 65535;
}

function parsePortText(text) {
  if (!/^\d+$/.test(text.trim())) return null;
  const n = Number(text.trim());
  return validPort(n) ? n : null;
}

function readProjectName(startDir) {
  let dir = resolvePath(startDir);
  for (;;) {
    const candidate = joinPath(dir, "project.godot");
    try {
      if (fs.statSync(candidate).isFile()) {
        const text = fs.readFileSync(candidate, "utf8");
        const m = text.match(/^\s*config\/name\s*=\s*"([^"\r\n]*)"/m);
        if (m) return m[1];
        return null;
      }
    } catch {
      // Missing or unreadable file: keep walking upward.
    }
    const parent = dir.substring(0, Math.max(dir.lastIndexOf(sep()), dir.endsWith(":") ? dir.length : 0));
    if (!parent || parent === dir) return null;
    if (process.platform !== "win32" && parent === "") return null;
    dir = parent;
  }
}

function userDataBase() {
  try {
    if (process.platform === "win32") {
      const appData = process.env.APPDATA;
      if (!appData) return null;
      return joinPath(appData, "Godot", "app_userdata");
    }
    const home = process.env.HOME || process.env.USERPROFILE;
    if (!home) return null;
    if (process.platform === "darwin") {
      return joinPath(home, "Library", "Application Support", "Godot", "app_userdata");
    }
    return joinPath(home, ".local", "share", "godot", "app_userdata");
  } catch {
    return null;
  }
}

function portFromLatestTrace(godotAutopilotDir) {
  const tracesDir = joinPath(godotAutopilotDir, "traces");
  let entries;
  try {
    entries = fs.readdirSync(tracesDir);
  } catch {
    return null;
  }
  let best = null;
  for (const name of entries) {
    if (!name.startsWith("trace-") || !name.endsWith(".jsonl")) continue;
    const full = joinPath(tracesDir, name);
    try {
      const mtime = fs.statSync(full).mtimeMs;
      if (best === null || mtime > best.mtime) best = { full, mtime };
    } catch {
      // Unreadable entry: skip it.
    }
  }
  if (best === null) return null;
  try {
    const lines = fs.readFileSync(best.full, "utf8").split(/\r?\n/);
    for (let i = lines.length - 1; i >= 0; i--) {
      const line = lines[i].trim();
      if (!line.includes('"type":"server_ready"') && !line.includes('"type": "server_ready"')) continue;
      try {
        const obj = JSON.parse(line);
        if (obj && obj.type === "server_ready") {
          const port = typeof obj.port === "number" ? obj.port : parsePortText(String(obj.port ?? ""));
          if (validPort(port)) return port;
        }
      } catch {
        // Malformed line: keep scanning older lines.
      }
    }
  } catch {
    return null;
  }
  return null;
}

function portFromConfigFile(godotAutopilotDir) {
  try {
    const obj = JSON.parse(fs.readFileSync(joinPath(godotAutopilotDir, "config.json"), "utf8"));
    const port = typeof obj?.port === "number" ? obj.port : parsePortText(String(obj?.port ?? ""));
    return validPort(port) ? port : null;
  } catch {
    return null;
  }
}

function resolvePort(opts) {
  if (opts.portText !== undefined) {
    const port = parsePortText(opts.portText);
    if (port === null) usageError(`--port must be an integer in 1-65535, got: ${opts.portText}`);
    return { port, source: "cli" };
  }
  const envText = process.env.GODOT_AUTOPILOT_PORT;
  if (envText !== undefined && envText !== "") {
    const port = parsePortText(envText);
    if (port !== null) return { port, source: "env" };
  }
  const startDir = opts.projectDir ?? process.cwd();
  if (opts.projectDir !== undefined) {
    try {
      if (!fs.statSync(resolvePath(opts.projectDir)).isDirectory()) {
        usageError(`--project-dir is not a directory: ${opts.projectDir}`);
      }
    } catch (e) {
      if (e && e.code === "ENOENT") usageError(`--project-dir does not exist: ${opts.projectDir}`);
      throw e;
    }
  }
  const projectName = readProjectName(startDir);
  if (projectName) {
    const base = userDataBase();
    if (base) {
      const dir = joinPath(base, projectName, "godot_autopilot");
      const tracePort = portFromLatestTrace(dir);
      if (tracePort !== null) return { port: tracePort, source: "trace" };
      const configPort = portFromConfigFile(dir);
      if (configPort !== null) return { port: configPort, source: "config" };
    }
  }
  return { port: DEFAULT_PORT, source: "default" };
}

function resolveTimeoutSeconds(text) {
  if (text === undefined) return DEFAULT_TIMEOUT_SEC;
  const n = Number(text);
  if (!Number.isFinite(n) || n <= 0) usageError(`--timeout must be a positive number of seconds, got: ${text}`);
  return n;
}

function loadToolArguments(opts) {
  if (opts.argsJson !== undefined && opts.argsFile !== undefined) {
    usageError("use only one of --args and --args-file.");
  }
  let args = {};
  if (opts.argsJson !== undefined) {
    try {
      args = JSON.parse(opts.argsJson);
    } catch {
      usageError("--args is not valid JSON.");
    }
  } else if (opts.argsFile !== undefined) {
    let text;
    try {
      text = fs.readFileSync(resolvePath(opts.argsFile), "utf8");
    } catch {
      usageError(`cannot read --args-file: ${opts.argsFile}`);
    }
    try {
      args = JSON.parse(text);
    } catch {
      usageError(`--args-file does not contain valid JSON: ${opts.argsFile}`);
    }
  }
  if (args === null || typeof args !== "object" || Array.isArray(args)) {
    usageError("tool arguments must be a JSON object.");
  }
  return args;
}

function buildCall(sub, rest, opts) {
  switch (sub) {
    case "ping":
      return { tool: "ping", args: {}, direct: true, projectResults: false };
    case "status":
      return { tool: "system_status", args: {}, direct: false, projectResults: false };
    case "categories":
      return { tool: "list_categories", args: {}, direct: true, projectResults: false };
    case "search": {
      if (rest.length > 1) usageError("search takes at most one [query] argument.");
      const args = {};
      if (rest.length === 1) args.query = rest[0];
      if (opts.category !== undefined) args.category = opts.category;
      if (opts.tagsText !== undefined) {
        const tags = opts.tagsText.split(",").map((s) => s.trim()).filter((s) => s.length > 0);
        if (tags.length > 0) args.tags = tags;
      }
      return { tool: "search_tools", args, direct: true, projectResults: true };
    }
    case "describe": {
      if (rest.length !== 1) usageError("describe requires exactly one <tool> argument.");
      return { tool: "get_tool_detail", args: { name: rest[0] }, direct: true, projectResults: false };
    }
    case "call": {
      if (rest.length !== 1) usageError("call requires exactly one <tool> argument.");
      const tool = rest[0];
      const args = loadToolArguments(opts);
      return { tool, args, direct: META_DIRECT_TOOLS.has(tool), projectResults: false };
    }
    default:
      usageError(`unknown command: ${sub}`);
      throw new Error("unreachable");
  }
}

async function postJson(url, body, timeoutMs) {
  let res;
  try {
    res = await fetch(url, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json, text/event-stream",
      },
      body,
      signal: AbortSignal.timeout(timeoutMs),
    });
  } catch (err) {
    if (err && (err.name === "TimeoutError" || err.name === "AbortError")) {
      transportError(
        "request timed out.",
        "Split the work into smaller calls or raise --timeout SEC.",
      );
    }
    const cause = err && err.cause ? `: ${err.cause}` : "";
    const detail = err && err.message ? `${err.message}${cause}` : String(err);
    transportError(
      `cannot reach the server (${detail}).`,
      "Is the editor open with the Godot Autopilot plugin enabled? Verify the port with --port or GODOT_AUTOPILOT_PORT.",
    );
  }
  let text = "";
  try {
    text = await res.text();
  } catch (err) {
    transportError(
      `cannot read the response body (${err && err.message ? err.message : err}).`,
      "Retry the call; if it persists, restart the editor session.",
    );
  }
  return { status: res.status, contentType: res.headers.get("content-type") || "", text };
}

function parseSseEnvelope(text) {
  const chunks = [];
  for (const line of text.split(/\r?\n/)) {
    if (line.startsWith("data:")) {
      let value = line.slice("data:".length);
      if (value.startsWith(" ")) value = value.slice(1);
      chunks.push(value);
    }
  }
  if (chunks.length === 0) return null;
  try {
    return JSON.parse(chunks.join("\n"));
  } catch {
    return undefined;
  }
}

function extractEnvelope(res) {
  if (res.contentType.includes("text/event-stream")) {
    const envelope = parseSseEnvelope(res.text);
    if (envelope === null) return { kind: "no-data" };
    if (envelope === undefined) return { kind: "bad-sse" };
    return { kind: "envelope", envelope };
  }
  try {
    return { kind: "envelope", envelope: JSON.parse(res.text), plain: true };
  } catch {
    return { kind: "bad-json" };
  }
}

function needsHandshake(status, envelope) {
  return (
    status === 400 &&
    !!envelope &&
    typeof envelope === "object" &&
    envelope.error &&
    typeof envelope.error === "object" &&
    envelope.error.code === -32600
  );
}

function adviceForStatus(status, bodyText) {
  switch (status) {
    case 400:
      return "Check the tool name and arguments; discover exact names with the search command.";
    case 403:
      return `The server only accepts loopback hosts; this script always targets ${HOST}.`;
    case 404:
      return "The endpoint only accepts POST at /mcp; verify the port discovery.";
    case 413:
      return `The request body exceeds 4 MiB: ${bodyText.length} bytes sent. Shrink the payload (split into batches).`;
    case 503:
      return "The editor may be closed or busy. Open it with the plugin enabled and reduce parallel calls.";
    case 504:
      return "Processing exceeded the server time budget. Split the work into smaller calls.";
    default:
      return "Verify the port (--port, GODOT_AUTOPILOT_PORT, trace file, config.json) and retry.";
  }
}

function redactedEnvelope(envelope) {
  const clone = JSON.parse(JSON.stringify(envelope));
  const content = clone && clone.result && clone.result.content;
  if (Array.isArray(content)) {
    for (const item of content) {
      if (item && item.type === "image" && typeof item.data === "string") {
        const bytes = Buffer.from(item.data, "base64").length;
        item.data = `<redacted ${bytes} bytes>`;
      }
    }
  }
  return clone;
}

function utcStamp() {
  return new Date().toISOString().replace(/[-:.]/g, "");
}

async function main() {
  const { opts, positionals } = parseArgv(process.argv.slice(2));
  if (opts.help || positionals[0] === "help") {
    console.log(usageText());
    return;
  }
  const audit = (msg) => {
    if (!opts.quiet) console.error(`[gda] ${msg}`);
  };
  const sub = positionals[0];
  if (sub === undefined) usageError("missing command.");
  if (sub !== "search" && (opts.category !== undefined || opts.tagsText !== undefined)) {
    usageError("--category and --tags only apply to the search command.");
  }
  if (sub !== "call" && (opts.argsJson !== undefined || opts.argsFile !== undefined)) {
    usageError("--args and --args-file only apply to the call command.");
  }
  const call = buildCall(sub, positionals.slice(1), opts);
  const { port, source } = resolvePort(opts);
  const timeoutSec = resolveTimeoutSeconds(opts.timeoutText);
  const timeoutMs = timeoutSec * 1000;
  const url = `http://${HOST}:${port}/mcp`;

  audit(`target ${url}`);
  audit(call.direct ? `tool ${call.tool} (direct)` : `tool call_tool wrapping ${call.tool}`);
  audit(`port ${port} (source: ${source})`);

  const body = JSON.stringify(makeEnvelope(1, call.tool, call.args, call.direct));
  if (Buffer.byteLength(body, "utf8") > MAX_BODY_BYTES) {
    transportError(
      `request body exceeds 4 MiB (${Buffer.byteLength(body, "utf8")} bytes).`,
      "Shrink the payload (split into batches).",
    );
  }

  let res = await postJson(url, body, timeoutMs);
  let extracted = extractEnvelope(res);
  let handshakeDone = false;
  if (
    (extracted.kind === "envelope" && needsHandshake(res.status, extracted.envelope)) ||
    (res.status === 400 && extracted.kind !== "envelope")
  ) {
    let envelope = extracted.kind === "envelope" ? extracted.envelope : null;
    if (envelope === null && extracted.kind !== "envelope") {
      try {
        envelope = JSON.parse(res.text);
      } catch {
        envelope = null;
      }
    }
    if (envelope !== null && !needsHandshake(res.status, envelope)) {
      // Fall through to the generic status handling below.
    } else {
      const hsBody = JSON.stringify({ jsonrpc: "2.0", method: "notifications/initialized" });
      const hs = await postJson(url, hsBody, timeoutMs);
      if (hs.status < 200 || hs.status >= 300) {
        transportError(
          `handshake failed with HTTP ${hs.status}.`,
          "Restart the editor session, then retry.",
        );
      }
      res = await postJson(url, JSON.stringify(makeEnvelope(2, call.tool, call.args, call.direct)), timeoutMs);
      extracted = extractEnvelope(res);
      handshakeDone = true;
      audit("handshake: sent notifications/initialized and retried the call once");
    }
  }
  if (!handshakeDone) audit("handshake: not needed");

  if (res.status < 200 || res.status >= 300) {
    let serverMsg = "";
    try {
      const parsed = JSON.parse(res.text);
      const err = parsed && parsed.error;
      if (err && (err.message || err.code !== undefined)) serverMsg = ` Server says: ${err.message ?? ""} (code ${err.code ?? "?"}).`;
      else if (parsed && parsed.message) serverMsg = ` Server says: ${parsed.message}.`;
    } catch {
      if (res.text.trim() !== "") serverMsg = ` Server says: ${res.text.trim().slice(0, 200)}.`;
    }
    transportError(`HTTP ${res.status}.${serverMsg}`, adviceForStatus(res.status, body));
  }
  if (extracted.kind === "no-data" || extracted.kind === "bad-sse") {
    transportError(
      "could not parse the SSE response (no usable data: frame).",
      "Retry the call; the editor plugin may need a restart.",
    );
  }
  if (extracted.kind === "bad-json") {
    transportError(
      "could not parse the plain-JSON response.",
      "Retry the call; the editor plugin may need a restart.",
    );
  }
  const envelope = extracted.envelope;
  if (!envelope || typeof envelope !== "object") {
    transportError("the response is not a JSON-RPC envelope.", "Retry the call.");
  }
  if (envelope.error && typeof envelope.error === "object") {
    const code = envelope.error.code;
    const message = envelope.error.message ?? JSON.stringify(envelope.error);
    transportError(`JSON-RPC error ${code ?? "?"}: ${message}`, adviceForStatus(res.status, body));
  }
  const result = envelope.result;
  const content = result && result.content;
  if (!Array.isArray(content) || content.length === 0) {
    transportError("the response has no result.content entries.", "Retry the call.");
  }

  const textPayloads = [];
  const imageReports = [];
  let saveDir = null;
  if (opts.saveImages !== undefined) {
    saveDir = resolvePath(opts.saveImages);
    try {
      fs.mkdirSync(saveDir, { recursive: true });
    } catch {
      transportError(`cannot create --save-images directory: ${opts.saveImages}`, "Use a writable directory.");
    }
  }
  const stamp = utcStamp();
  let imageIndex = 0;
  for (const item of content) {
    if (item && item.type === "text" && typeof item.text === "string") {
      try {
        textPayloads.push(JSON.parse(item.text));
      } catch {
        transportError("the business payload is not valid JSON.", "Retry the call.");
      }
    } else if (item && item.type === "image" && typeof item.data === "string") {
      const bytes = Buffer.from(item.data, "base64").length;
      const mimeType = item.mimeType || "image/png";
      if (saveDir !== null) {
        const fileName = `gda_${stamp}_${imageIndex}.png`;
        const full = joinPath(saveDir, fileName);
        try {
          fs.writeFileSync(full, Buffer.from(item.data, "base64"));
        } catch {
          transportError(`cannot write image file: ${full}`, "Use a writable --save-images directory.");
        }
        imageReports.push({ mimeType, bytes, saved: true, path: full });
        audit(`image saved: ${full}`);
      } else {
        imageReports.push({ mimeType, bytes, saved: false });
      }
      imageIndex++;
    }
  }

  let display;
  if (textPayloads.length === 1) {
    display = textPayloads[0];
  } else if (textPayloads.length > 1) {
    display = textPayloads;
  } else if (imageReports.length > 0) {
    display = {};
  } else {
    transportError("the response carries neither text nor image content.", "Retry the call.");
  }

  const hasInnerError = (p) => {
    if (!p || typeof p !== "object" || Array.isArray(p)) return false;
    if ("error" in p) return true;
    const innerResult = p.result !== undefined ? p.result : p.data;
    if (innerResult && typeof innerResult === "object" && !Array.isArray(innerResult) && "error" in innerResult) return true;
    if (Array.isArray(p.results)) {
      return p.results.some((it) => it && typeof it === "object" && (it.status === "error" || "error" in it));
    }
    return false;
  };
  const toolFailed =
    (result && result.isError === true) ||
    (Array.isArray(display) ? display.some(hasInnerError) : hasInnerError(display));

  if (call.projectResults && display && typeof display === "object" && !Array.isArray(display)) {
    const scopes = [display.results, display.result && display.result.results, display.data && display.data.results];
    for (const candidate of scopes) {
      if (Array.isArray(candidate)) {
        display = candidate;
        break;
      }
    }
  }

  if (imageReports.length > 0) {
    if (display && typeof display === "object" && !Array.isArray(display)) {
      display = { ...display, images: imageReports };
    } else {
      display = { result: display, images: imageReports };
    }
  }

  if (opts.raw) {
    console.log(JSON.stringify(redactedEnvelope(envelope), null, 2));
  } else {
    console.log(JSON.stringify(display, null, 2));
  }
  if (toolFailed) {
    console.error("[gda] tool-level failure reported in the business payload.");
    process.exit(1);
  }
}

main().catch((err) => {
  if (err && err.code !== undefined) throw err;
  console.error(`[gda] unexpected failure: ${err && err.stack ? err.stack : err}`);
  process.exit(2);
});
