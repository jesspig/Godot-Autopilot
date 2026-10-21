import fs from "node:fs";
import path from "node:path";
import type { GlobalOptions } from "./args";
import { CliError, ExitCode } from "./errors";
import {
  findPluginRecordByProject,
  type PluginInstanceRecord,
} from "./instances";
import { readProjectName, userDataBase } from "./platform";
import { isAlive } from "./process";

export { projectPathKey } from "./platform";

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

export type TargetChain =
  | "port"
  | "id"
  | "env"
  | "cli-registry"
  | "plugin-index"
  | "legacy"
  | "default";

export interface ResolvedTarget {
  port: number;
  source: string;
  chain: TargetChain;
  instance?: InstanceEntry;
  pluginRecord?: PluginInstanceRecord;
}

export type TargetSelector = Pick<GlobalOptions, "portText" | "id" | "project">;

const DEFAULT_PORT = 9527;

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

export function resolveTarget(opts: TargetSelector, ctx: TargetContext): ResolvedTarget {
  assertSingleSelector(opts);

  if (opts.portText !== undefined) {
    return { port: parseTargetPort(opts.portText), source: "cli", chain: "port" };
  }

  if (opts.id !== undefined) {
    const entry = ctx.findById(opts.id);
    if (entry === undefined || !isAlive(entry.pid)) {
      throw new CliError(ExitCode.Transport, `unknown instance: ${opts.id} (run "gda list" to see running instances)`);
    }
    return { port: entry.port, source: "id", chain: "id", instance: entry };
  }

  const envText = process.env.GODOT_AUTOPILOT_PORT;
  if (envText !== undefined && envText !== "") {
    const port = parsePortText(envText);
    if (port !== null) return { port, source: "env", chain: "env" };
  }

  const projectOption = opts.project;
  const scoped = projectOption !== undefined;
  const projectPath = scoped ? normalizePath(projectOption) : process.cwd();

  const entry = ctx.findLiveByProject(projectPath);
  if (entry !== undefined) {
    return { port: entry.port, source: scoped ? "project" : "cwd", chain: "cli-registry", instance: entry };
  }

  const record = findPluginRecordByProject(projectPath);
  if (record !== undefined && record.port !== null) {
    return { port: record.port, source: "registry", chain: "plugin-index", pluginRecord: record };
  }

  const found = portFromTraceOrConfig(projectPath);
  if (found !== null) {
    return { port: found.port, source: found.source, chain: "legacy" };
  }

  return { port: DEFAULT_PORT, source: "default", chain: "default" };
}
