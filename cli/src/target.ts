import fs from "node:fs";
import path from "node:path";
import type { GlobalOptions } from "./args";
import { CliError, ExitCode } from "./errors";
import { gdaRoot, readProjectName, userDataBase } from "./platform";
import { isAlive } from "./process";

export interface InstanceEntry {
  id: string;
  project: string;
  port: number;
  pid: number;
  startedAt: string;
  headless: boolean;
  godot: string;
  log: string;
}

export interface TargetContext {
  findById(id: string): InstanceEntry | undefined;
  findLiveByProject(project: string): InstanceEntry | undefined;
}

export interface ResolvedTarget {
  port: number;
  source: string;
  instance?: InstanceEntry;
}

export type TargetSelector = Pick<GlobalOptions, "portText" | "id" | "project">;

const DEFAULT_PORT = 9527;
const PLUGIN_INSTANCE_INDEX_FILE = "instances.json";

function validPort(value: number | null | undefined): value is number {
  return typeof value === "number" && Number.isInteger(value) && value >= 1 && value <= 65535;
}

function parsePortText(text: string): number | null {
  if (!/^\d+$/.test(text.trim())) return null;
  const n = Number(text.trim());
  return validPort(n) ? n : null;
}

function normalizePath(p: string): string {
  return path.resolve(p);
}

export function projectPathKey(p: string): string {
  const resolved = path.resolve(p);
  return process.platform === "win32" ? resolved.toLowerCase() : resolved;
}

export function assertSingleSelector(opts: TargetSelector): void {
  const explicitCount = [opts.portText, opts.id, opts.project].filter((v) => v !== undefined).length;
  if (explicitCount > 1) {
    throw new CliError(ExitCode.Usage, "use only one of --id/--port/--project");
  }
}

export function parseTargetPort(text: string): number {
  const port = parsePortText(text);
  if (port === null) {
    throw new CliError(ExitCode.Usage, `--port must be an integer in 1-65535, got: ${text}`);
  }
  return port;
}

function portFromLatestTrace(godotAutopilotDir: string): number | null {
  const tracesDir = path.join(godotAutopilotDir, "traces");
  let entries: string[];
  try {
    entries = fs.readdirSync(tracesDir);
  } catch {
    return null;
  }
  let best: { full: string; mtime: number } | null = null;
  for (const name of entries) {
    if (!name.startsWith("trace-") || !name.endsWith(".jsonl")) continue;
    const full = path.join(tracesDir, name);
    try {
      const mtime = fs.statSync(full).mtimeMs;
      if (best === null || mtime > best.mtime) best = { full, mtime };
    } catch {
    }
  }
  if (best === null) return null;
  try {
    const lines = fs.readFileSync(best.full, "utf8").split(/\r?\n/);
    for (let i = lines.length - 1; i >= 0; i--) {
      const line = lines[i].trim();
      if (!line.includes('"type":"server_ready"') && !line.includes('"type": "server_ready"')) continue;
      try {
        const obj = JSON.parse(line) as { type?: unknown; port?: unknown };
        if (obj && obj.type === "server_ready") {
          const port = typeof obj.port === "number" ? obj.port : parsePortText(String(obj.port ?? ""));
          if (validPort(port)) return port;
        }
      } catch {
      }
    }
  } catch {
    return null;
  }
  return null;
}

function portFromConfigFile(godotAutopilotDir: string): number | null {
  try {
    const obj = JSON.parse(fs.readFileSync(path.join(godotAutopilotDir, "config.json"), "utf8")) as { port?: unknown };
    const port = typeof obj?.port === "number" ? obj.port : parsePortText(String(obj?.port ?? ""));
    return validPort(port) ? port : null;
  } catch {
    return null;
  }
}

function portFromTraceOrConfig(startDir: string): { port: number; source: string } | null {
  const projectName = readProjectName(startDir);
  if (projectName) {
    const base = userDataBase();
    if (base) {
      const dir = path.join(base, projectName, "godot_autopilot");
      const tracePort = portFromLatestTrace(dir);
      if (tracePort !== null) return { port: tracePort, source: "trace" };
      const configPort = portFromConfigFile(dir);
      if (configPort !== null) return { port: configPort, source: "config" };
    }
  }
  return null;
}

interface PluginInstanceRecord {
  port: number;
  projectPath: string;
}

function readPluginInstanceRecords(): PluginInstanceRecord[] {
  let raw: string;
  try {
    raw = fs.readFileSync(path.join(gdaRoot(), PLUGIN_INSTANCE_INDEX_FILE), "utf8");
  } catch {
    return [];
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return [];
  }
  if (typeof parsed !== "object" || parsed === null) return [];
  const items = (parsed as { instances?: unknown }).instances;
  if (!Array.isArray(items)) return [];
  const records: PluginInstanceRecord[] = [];
  for (const item of items) {
    if (typeof item !== "object" || item === null) continue;
    const doc = item as Record<string, unknown>;
    const projectPath = stringField(doc, "project_path") ?? stringField(doc, "projectPath");
    if (projectPath === undefined) continue;
    const port = portField(doc);
    if (port === null) continue;
    records.push({ port, projectPath });
  }
  return records;
}

function stringField(doc: Record<string, unknown>, key: string): string | undefined {
  const value = doc[key];
  return typeof value === "string" && value !== "" ? value : undefined;
}

function portField(doc: Record<string, unknown>): number | null {
  const direct = doc["port"];
  if (typeof direct === "number") return validPort(direct) ? direct : null;
  if (typeof direct === "string") return parsePortText(direct);
  const endpoints = doc["endpoints"];
  if (typeof endpoints === "object" && endpoints !== null) {
    const table = endpoints as Record<string, unknown>;
    for (const name of ["mcp", "api"]) {
      const entry = table[name];
      if (typeof entry !== "object" || entry === null) continue;
      const value = (entry as Record<string, unknown>)["port"];
      if (typeof value === "number" && validPort(value)) return value;
    }
  }
  return null;
}

function portFromInstanceRegistry(projectPath: string): number | null {
  const key = projectPathKey(projectPath);
  for (const record of readPluginInstanceRecords()) {
    if (projectPathKey(record.projectPath) === key) return record.port;
  }
  return null;
}

export function resolveTarget(opts: TargetSelector, ctx: TargetContext): ResolvedTarget {
  assertSingleSelector(opts);

  if (opts.id !== undefined) {
    const entry = ctx.findById(opts.id);
    if (entry === undefined || !isAlive(entry.pid)) {
      throw new CliError(ExitCode.Transport, `unknown instance: ${opts.id} (run "gda list" to see running instances)`);
    }
    return { port: entry.port, source: "id", instance: entry };
  }

  if (opts.portText !== undefined) {
    return { port: parseTargetPort(opts.portText), source: "cli" };
  }

  if (opts.project !== undefined) {
    const normalized = normalizePath(opts.project);
    const entry = ctx.findLiveByProject(normalized);
    if (entry !== undefined) {
      return { port: entry.port, source: "project", instance: entry };
    }
    const registered = portFromInstanceRegistry(normalized);
    if (registered !== null) return { port: registered, source: "registry" };
    const found = portFromTraceOrConfig(normalized);
    if (found !== null) return found;
    return { port: DEFAULT_PORT, source: "default" };
  }

  const envText = process.env.GODOT_AUTOPILOT_PORT;
  if (envText !== undefined && envText !== "") {
    const port = parsePortText(envText);
    if (port !== null) return { port, source: "env" };
  }
  const cwdEntry = ctx.findLiveByProject(process.cwd());
  if (cwdEntry !== undefined) {
    return { port: cwdEntry.port, source: "cwd", instance: cwdEntry };
  }
  const cwdRegistered = portFromInstanceRegistry(process.cwd());
  if (cwdRegistered !== null) return { port: cwdRegistered, source: "registry" };
  const found = portFromTraceOrConfig(process.cwd());
  if (found !== null) return found;
  return { port: DEFAULT_PORT, source: "default" };
}
