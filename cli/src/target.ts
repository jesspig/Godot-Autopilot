import path from "node:path";

import type { GlobalOptions } from "./args";
import { CliError, ExitCode } from "./errors";
import {
  findPluginRecordByPort,
  findPluginRecordByProject,
  type PluginInstanceRecord,
} from "./instances";
import type { IpcEndpoint } from "./ipc/client";
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

export type TargetChain = "port" | "id" | "env" | "cli-registry" | "plugin-index";

export interface ResolvedTarget {
  port: number;
  source: string;
  chain: TargetChain;
  instance?: InstanceEntry;
  pluginRecord?: PluginInstanceRecord;
  ipcEndpoint?: IpcEndpoint;
  mcpUrl: string;
  transport: "ipc" | "http";
  portSource: string;
  pluginVersion: string;
}

export type TargetSelector = Pick<GlobalOptions, "portText" | "id" | "project">;

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

function endpointFor(record: PluginInstanceRecord): IpcEndpoint | undefined {
  const ipc = record.ipcEndpoint;
  if (ipc === null) return undefined;
  return {
    kind: ipc.kind,
    path: ipc.path,
    name: ipc.name,
    host: ipc.host,
    port: ipc.port ?? 0,
    token: ipc.token,
    framing: ipc.framing,
  };
}

function resolved(
  port: number,
  source: string,
  chain: TargetChain,
  record?: PluginInstanceRecord,
  instance?: InstanceEntry,
): ResolvedTarget {
  const ipcEndpoint = record !== undefined ? endpointFor(record) : undefined;
  return {
    port,
    source,
    chain,
    ...(instance !== undefined ? { instance } : {}),
    ...(record !== undefined ? { pluginRecord: record } : {}),
    ...(ipcEndpoint !== undefined ? { ipcEndpoint } : {}),
    mcpUrl: `http://127.0.0.1:${port}/mcp`,
    transport: ipcEndpoint !== undefined ? "ipc" : "http",
    portSource: record?.portSource ?? "",
    pluginVersion: record?.version ?? "",
  };
}

export function resolveTarget(opts: TargetSelector, ctx: TargetContext): ResolvedTarget {
  assertSingleSelector(opts);

  if (opts.portText !== undefined) {
    return resolved(parseTargetPort(opts.portText), "cli", "port");
  }

  if (opts.id !== undefined) {
    const entry = ctx.findById(opts.id);
    if (entry === undefined || !isAlive(entry.pid)) {
      throw new CliError(ExitCode.Transport, `unknown instance: ${opts.id} (run "gda list" to see running instances)`);
    }
    const record = findPluginRecordByPort(entry.port);
    return resolved(entry.port, "id", "id", record, entry);
  }

  const envText = process.env.GODOT_AUTOPILOT_PORT;
  if (envText !== undefined && envText !== "") {
    const port = parsePortText(envText);
    if (port !== null) {
      return resolved(port, "env", "env", findPluginRecordByPort(port));
    }
  }

  const projectOption = opts.project;
  const scoped = projectOption !== undefined;
  const projectPath = scoped ? normalizePath(projectOption) : process.cwd();

  const record = findPluginRecordByProject(projectPath);
  if (record !== undefined && record.port !== null) {
    return resolved(record.port, scoped ? "project" : "cwd", "plugin-index", record);
  }

  const entry = ctx.findLiveByProject(projectPath);
  if (entry !== undefined) {
    return resolved(entry.port, scoped ? "project" : "cwd", "cli-registry", undefined, entry);
  }

  throw new CliError(
    ExitCode.Transport,
    `no running Godot Autopilot instance for ${projectPath}`,
    "open the editor with the plugin enabled, or run 'gda start' and then 'gda list' to confirm the instance.",
  );
}
