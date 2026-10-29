import { parseProbePort, type GlobalOptions } from "../args";
import { ExitCode } from "../errors";
import {
  deriveEndpoint,
  endpointBaseUrl,
  readPluginInstanceIndex,
} from "../instances";
import { err } from "../output";
import { probeEndpoint } from "../probe";
import { isAlive } from "../process";
import { loadRegistry, type InstanceEntry } from "../registry";
import { writeJson } from "./json-output";

type ListSource = "cli" | "plugin" | "probe";

interface ListEntry {
  id: string;
  source: ListSource;
  project: string;
  port: number;
  pid: number | null;
  headless: boolean;
  startedAt: string;
  live: boolean;
}

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  const probePort = opts.probe === undefined ? null : parseProbePort(opts.probe);
  const cliEntries = loadRegistry().instances.map(cliEntry);
  const pluginEntries = pluginListEntries(cliEntries);
  const probeEntries = await probeListEntries(probePort);
  const instances = [...cliEntries, ...pluginEntries, ...probeEntries];
  writeJson(instances, opts);
  if (instances.length === 0) {
    reportEmpty(opts);
  }
  return ExitCode.Success;
}

function cliEntry(entry: InstanceEntry): ListEntry {
  return {
    id: entry.id,
    source: "cli",
    project: entry.project,
    port: entry.port,
    pid: entry.pid,
    headless: entry.headless,
    startedAt: entry.startedAt,
    live: isAlive(entry.pid),
  };
}

function pluginListEntries(cliEntries: ListEntry[]): ListEntry[] {
  const knownPids = new Set<number>();
  const knownPorts = new Set<number>();
  for (const entry of cliEntries) {
    if (entry.pid !== null) {
      knownPids.add(entry.pid);
    }
    knownPorts.add(entry.port);
  }
  const records = readPluginInstanceIndex().sort((a, b) => b.startedAt - a.startedAt);
  const entries: ListEntry[] = [];
  for (const record of records) {
    const port = record.port;
    const pid = record.pid;
    if (port === null || pid === null || !isAlive(pid)) {
      continue;
    }
    if (knownPids.has(pid) || knownPorts.has(port)) {
      continue;
    }
    knownPids.add(pid);
    knownPorts.add(port);
    entries.push({
      id: record.instanceId,
      source: "plugin",
      project: record.projectPath,
      port,
      pid,
      headless: false,
      startedAt: isoFromEpochMs(record.startedAt),
      live: true,
    });
  }
  return entries;
}

async function probeListEntries(port: number | null): Promise<ListEntry[]> {
  if (port === null) {
    return [];
  }
  const endpoint = deriveEndpoint(port, "mcp");
  const target = { host: endpoint.host, port: endpoint.port, url: endpointBaseUrl(endpoint) };
  if (!(await probeEndpoint(target, "light"))) {
    return [];
  }
  return [
    {
      id: `probe:${port}`,
      source: "probe",
      project: "",
      port,
      pid: null,
      headless: false,
      startedAt: new Date().toISOString(),
      live: true,
    },
  ];
}

function isoFromEpochMs(value: number): string {
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? "" : date.toISOString();
}

function reportEmpty(opts: GlobalOptions): void {
  if (opts.probe !== undefined) {
    err("no live instance answered on the probed port; check that the editor is running and the port is correct.");
    return;
  }
  err("no CLI-started or plugin-indexed instances; add --probe <port> to probe one specific port (no port scanning).");
}
