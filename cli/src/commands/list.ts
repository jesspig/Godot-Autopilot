import { parseProbePort, type GlobalOptions } from "../args";
import { ExitCode } from "../errors";
import {
  deriveEndpoint,
  endpointBaseUrl,
  readPluginInstanceIndex,
} from "../instances";
import { err } from "../output";
import { endpointAlive, probeEndpoint } from "../probe";
import { isAlive } from "../process";
import { listProjects } from "../projects";
import { loadRegistry, type InstanceEntry } from "../registry";
import { writeJson } from "./json-output";

type ListSource = "cli" | "plugin" | "probe" | "project";

interface LiveDetail {
  pid: boolean;
  endpoint: boolean | null;
}

interface ListEntry {
  id: string;
  source: ListSource;
  project: string;
  port: number | null;
  pid: number | null;
  headless: boolean | null;
  startedAt: string | null;
  live: boolean;
  liveDetail: LiveDetail;
}

interface ProjectAggregate {
  project: string;
  entries: number;
  live: boolean;
  ports: number[];
}

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  const probePort = opts.probe === undefined ? null : parseProbePort(opts.probe);
  const cliEntries = loadRegistry().instances.map(cliEntry);
  const pluginEntries = pluginListEntries(cliEntries);
  const probeEntries = await probeListEntries(probePort);
  let entries = [...cliEntries, ...pluginEntries, ...probeEntries];
  await fillEndpointLiveness(entries);
  if (opts.stale === true) {
    entries = entries.filter((entry) => !entry.live);
  }
  if (opts.all === true) {
    entries = withIdleProjects(entries);
  }
  if (opts.projectsView === true) {
    writeJson(aggregateByProject(entries), opts);
    return ExitCode.Success;
  }
  writeJson(entries, opts);
  if (entries.length === 0) {
    reportEmpty(opts);
  }
  return ExitCode.Success;
}

function cliEntry(entry: InstanceEntry): ListEntry {
  const alive = isAlive(entry.pid);
  return {
    id: entry.id,
    source: "cli",
    project: entry.project,
    port: entry.port,
    pid: entry.pid,
    headless: entry.headless,
    startedAt: entry.startedAt,
    live: alive,
    liveDetail: { pid: alive, endpoint: null },
  };
}

function pluginListEntries(cliEntries: ListEntry[]): ListEntry[] {
  const knownPids = new Set<number>();
  const knownPorts = new Set<number>();
  for (const entry of cliEntries) {
    if (entry.pid !== null) {
      knownPids.add(entry.pid);
    }
    if (entry.port !== null) {
      knownPorts.add(entry.port);
    }
  }
  const records = readPluginInstanceIndex().sort((a, b) => b.startedAt - a.startedAt);
  const entries: ListEntry[] = [];
  for (const record of records) {
    const pid = record.pid;
    const port = record.port;
    if ((pid !== null && knownPids.has(pid)) || (port !== null && knownPorts.has(port))) {
      continue;
    }
    if (pid !== null) {
      knownPids.add(pid);
    }
    if (port !== null) {
      knownPorts.add(port);
    }
    const alive = pid !== null && isAlive(pid);
    entries.push({
      id: record.instanceId,
      source: "plugin",
      project: record.projectPath,
      port,
      pid,
      headless: null,
      startedAt: null,
      live: alive,
      liveDetail: { pid: alive, endpoint: null },
    });
  }
  return entries;
}

async function probeListEntries(port: number | null): Promise<ListEntry[]> {
  if (port === null) {
    return [];
  }
  const endpoint = deriveEndpoint(port);
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
      headless: null,
      startedAt: null,
      live: true,
      liveDetail: { pid: false, endpoint: true },
    },
  ];
}

async function fillEndpointLiveness(entries: ListEntry[]): Promise<void> {
  await Promise.all(
    entries.map(async (entry) => {
      if (entry.port === null || entry.source === "probe") {
        return;
      }
      const alive = await endpointAlive(entry.port);
      entry.liveDetail = { pid: entry.liveDetail.pid, endpoint: alive };
      entry.live = entry.liveDetail.pid || alive;
    }),
  );
}

function withIdleProjects(entries: ListEntry[]): ListEntry[] {
  const covered = new Set(entries.map((entry) => entry.project).filter((value) => value !== ""));
  const extras: ListEntry[] = [];
  for (const project of listProjects()) {
    if (project.path === "" || covered.has(project.path) || project.live) {
      continue;
    }
    extras.push({
      id: `project:${project.hash}`,
      source: "project",
      project: project.path,
      port: null,
      pid: null,
      headless: null,
      startedAt: project.lastActive === "" ? null : project.lastActive,
      live: false,
      liveDetail: { pid: false, endpoint: null },
    });
  }
  return [...entries, ...extras];
}

function aggregateByProject(entries: ListEntry[]): ProjectAggregate[] {
  const groups = new Map<string, ProjectAggregate>();
  for (const entry of entries) {
    const key = entry.project === "" ? "(unknown)" : entry.project;
    const group = groups.get(key) ?? { project: key, entries: 0, live: false, ports: [] };
    group.entries += 1;
    group.live = group.live || entry.live;
    if (entry.port !== null && !group.ports.includes(entry.port)) {
      group.ports.push(entry.port);
    }
    groups.set(key, group);
  }
  return [...groups.values()].sort((a, b) => a.project.localeCompare(b.project));
}

function reportEmpty(opts: GlobalOptions): void {
  if (opts.probe !== undefined) {
    err("no live instance answered on the probed port; check that the editor is running and the port is correct.");
    return;
  }
  err("no CLI-started or plugin-indexed instances; add --probe <port> to probe one specific port (no port scanning).");
}