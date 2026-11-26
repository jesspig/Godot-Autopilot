import path from "node:path";

import type { GlobalOptions } from "./args";
import { CliError, ExitCode } from "./errors";
import {
  findPluginRecordByPort,
  findPluginRecordByProject,
  readPluginInstanceIndex,
  type PluginInstanceRecord,
} from "./instances";
import type { IpcEndpoint } from "./ipc/client";
import { isAlive, processStartTime } from "./process";
import { matchesProcessIdentity } from "./security-policy";

import { projectPathKey } from "./platform";

export { projectPathKey };

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
  chain: TargetChain;
  instance?: InstanceEntry;
  pluginRecord?: PluginInstanceRecord;
  ipcEndpoint?: IpcEndpoint;
  portSource: string;
  pluginVersion: string;
  instanceId?: string;
  pinned?: boolean;
  stale?: boolean;
}

export type TargetChain = "port" | "id" | "socket" | "env" | "cli-registry" | "plugin-index";

export type TargetSelector = Pick<GlobalOptions, "portText" | "id" | "project" | "socketText">;

export interface TargetCandidate {
  id: string;
  project: string;
  port: number;
  pid: number;
}

export class AmbiguousTargetError extends CliError {
  constructor(
    message: string,
    public readonly candidates: TargetCandidate[],
  ) {
    super(ExitCode.Transport, message, "Use --id to select exactly one instance.");
    this.name = "AmbiguousTargetError";
  }
}

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
  const primaryCount = [opts.id, opts.project, opts.socketText].filter((v) => v !== undefined).length;
  if (primaryCount > 1) {
    throw new CliError(ExitCode.Usage, "use only one of --id/--socket/--port/--project");
  }
  if (opts.portText !== undefined && opts.socketText === undefined &&
      (opts.id !== undefined || opts.project !== undefined)) {
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

function rejectAmbiguous(label: string, candidates: TargetCandidate[]): void {
  if (candidates.length <= 1) return;
  throw new AmbiguousTargetError(
    `${label} matches ${candidates.length} live instances; select one with --id`,
    candidates,
  );
}

function assertInstanceFresh(entry: InstanceEntry): void {
  if (!isAlive(entry.pid)) {
    throw new CliError(
      ExitCode.Transport,
      `stale instance ${entry.id} (pid ${entry.pid}) is no longer running; run 'gda editor list' and select a live instance.`,
    );
  }
  const recorded = Number(entry.startedAt);
  if (Number.isFinite(recorded) && recorded > 0) {
    const observed = processStartTime(entry.pid);
    if (observed !== null && !matchesProcessIdentity(
      { pid: entry.pid, started_at: recorded },
      { pid: entry.pid, started_at: observed },
    )) {
      throw new CliError(
        ExitCode.Transport,
        `stale instance ${entry.id} (pid ${entry.pid}) no longer matches its recorded start time; refusing to pin a reused pid.`,
      );
    }
  }
}

function pin(target: ResolvedTarget, instanceId: string | undefined): ResolvedTarget {
  return { ...target, instanceId: instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "", pinned: true, stale: false };
}

function nativeBySocket(socketText: string): ResolvedTarget {
  const socketPath = path.resolve(socketText);
  const matched = readPluginInstanceIndex().filter((record) =>
    record.ipcEndpoint !== null && path.resolve(record.ipcEndpoint.path) === socketPath,
  );
  const live = matched.filter((record) => record.pid !== null && isAlive(record.pid));
  if (live.length > 1) {
    throw new AmbiguousTargetError(
      `--socket ${socketText} matches ${live.length} live instances; select one with --id`,
      live.map((record) => ({
        id: record.instanceId,
        project: record.projectPath,
        port: record.port ?? 0,
        pid: record.pid ?? 0,
      })),
    );
  }
  const record = live[0] ?? matched[0];
  const endpoint: IpcEndpoint = (record?.ipcEndpoint != null ? endpointFor(record) : undefined)
    ?? { kind: "unix", path: socketPath };
  const port = record?.port ?? 0;
  const result = resolved(port, "socket", "socket", record);
  return pin({ ...result, ipcEndpoint: endpoint }, record?.instanceId);
}

function nativeByPort(port: number): ResolvedTarget {
  const records = readPluginInstanceIndex().filter((record) => record.port === port || record.mcpEndpoint?.port === port);
  const live = records.filter((record) => record.pid !== null && isAlive(record.pid));
  rejectAmbiguous(`--port ${port}`, live.map((record) => ({
    id: record.instanceId,
    project: record.projectPath,
    port: record.port ?? port,
    pid: record.pid ?? 0,
  })).filter((candidate) => candidate.id !== ""));
  const record = findPluginRecordByPort(port);
  return pin(resolved(port, "cli", "port", record), record?.instanceId);
}

export function resolveNativeTarget(opts: TargetSelector, ctx: TargetContext): ResolvedTarget {
  assertSingleSelector(opts);

  if (opts.id !== undefined) {
    const entry = ctx.findById(opts.id);
    if (entry === undefined) {
      throw new CliError(ExitCode.Transport, `unknown instance: ${opts.id} (run 'gda editor list' to see running instances)`);
    }
    assertInstanceFresh(entry);
    const record = findPluginRecordByPort(entry.port);
    return pin(resolved(entry.port, "id", "id", record, entry), entry.id);
  }

  if (opts.socketText !== undefined) return nativeBySocket(opts.socketText);

  if (opts.portText !== undefined) return nativeByPort(parseTargetPort(opts.portText));

  const target = resolveTarget(opts, ctx);
  const usesProjectScope = opts.project !== undefined || target.chain === "plugin-index" ||
    target.chain === "cli-registry" || target.chain === "env";
  if (usesProjectScope) {
    const projectPath = opts.project !== undefined ? normalizePath(opts.project) : process.cwd();
    const matches = readPluginInstanceIndex().filter((record) =>
      record.projectPath !== "" && projectPathKey(record.projectPath) === projectPathKey(projectPath) &&
      record.pid !== null && isAlive(record.pid),
    );
    if (matches.length > 1) {
      throw new AmbiguousTargetError(
        `project ${projectPath} has ${matches.length} live instances; select one with --id`,
        matches.map((record) => ({
          id: record.instanceId,
          project: record.projectPath,
          port: record.port ?? 0,
          pid: record.pid ?? 0,
        })),
      );
    }
  }
  if (target.instance !== undefined) assertInstanceFresh(target.instance);
  return pin(target, target.pluginRecord?.instanceId ?? target.instance?.id);
}
