import type { ProbeLevel, ProbeTarget } from "../probe";
import { probeEndpoint } from "../probe";
import {
  deriveEndpoint,
  endpointBaseUrl,
  type EndpointEntry,
  type EndpointKind,
  type PluginInstanceRecord,
} from "../instances";
import { isAlive } from "../process";

export type ControlRoute =
  | { kind: "health" }
  | { kind: "instances" }
  | { kind: "instance"; ref: string; endpoint: EndpointKind; restPath: string }
  | { kind: "default"; endpoint: EndpointKind; restPath: string }
  | { kind: "not_found" };

export interface EndpointView {
  url: string;
  host: string;
  port: number;
  path: string;
  protocol: string;
  auth: string;
}

export interface InstanceSummary {
  id: string;
  port: number | null;
  pid: number | null;
  project: string;
  version: string;
  live: boolean;
  endpoints: { mcp: EndpointView | null; api: EndpointView | null };
}

export type DefaultSelection =
  | { kind: "none" }
  | { kind: "single"; record: PluginInstanceRecord }
  | { kind: "ambiguous"; candidates: InstanceSummary[] };

export type AlivePredicate = (pid: number) => boolean;

export type ProbeFn = (target: ProbeTarget, level: ProbeLevel) => Promise<boolean>;

const HEALTH_ROUTE = /^\/control\/health$/;
const INSTANCES_ROUTE = /^\/control\/instances$/;
const INSTANCE_ROUTE = /^\/i\/([^/]+)\/(api|mcp)(\/.*)?$/;
const DEFAULT_ROUTE = /^\/(api|mcp)(\/.*)?$/;

export function matchRoute(pathname: string): ControlRoute {
  if (HEALTH_ROUTE.test(pathname)) return { kind: "health" };
  if (INSTANCES_ROUTE.test(pathname)) return { kind: "instances" };
  const scoped = INSTANCE_ROUTE.exec(pathname);
  if (scoped !== null) {
    return {
      kind: "instance",
      ref: scoped[1],
      endpoint: scoped[2] as EndpointKind,
      restPath: `/${scoped[2]}${scoped[3] ?? ""}`,
    };
  }
  const fallback = DEFAULT_ROUTE.exec(pathname);
  if (fallback !== null) {
    return {
      kind: "default",
      endpoint: fallback[1] as EndpointKind,
      restPath: `/${fallback[1]}${fallback[2] ?? ""}`,
    };
  }
  return { kind: "not_found" };
}

export function instanceEndpoint(record: PluginInstanceRecord, kind: EndpointKind): EndpointEntry | null {
  const declared = kind === "mcp" ? record.mcpEndpoint : record.apiEndpoint;
  if (declared !== null) return declared;
  if (record.port === null) return null;
  return deriveEndpoint(record.port, kind);
}

export function endpointView(endpoint: EndpointEntry): EndpointView {
  return {
    url: endpointBaseUrl(endpoint),
    host: endpoint.host,
    port: endpoint.port,
    path: endpoint.path,
    protocol: endpoint.protocol,
    auth: endpoint.auth,
  };
}

export function instanceLabel(record: PluginInstanceRecord): string {
  if (record.instanceId !== "") return record.instanceId;
  if (record.port !== null) return `port:${record.port}`;
  return "unknown";
}

export function instanceSummary(
  record: PluginInstanceRecord,
  alive: AlivePredicate = isAlive,
): InstanceSummary {
  const mcp = instanceEndpoint(record, "mcp");
  const api = instanceEndpoint(record, "api");
  return {
    id: record.instanceId,
    port: record.port,
    pid: record.pid,
    project: record.projectPath,
    version: record.version,
    live: record.pid !== null && alive(record.pid),
    endpoints: {
      mcp: mcp === null ? null : endpointView(mcp),
      api: api === null ? null : endpointView(api),
    },
  };
}

export function activeRecords(
  records: PluginInstanceRecord[],
  alive: AlivePredicate = isAlive,
): PluginInstanceRecord[] {
  return records.filter((record) => record.pid !== null && alive(record.pid));
}

export function controlInstancesPayload(
  records: PluginInstanceRecord[],
  alive: AlivePredicate = isAlive,
): { instances: InstanceSummary[] } {
  return { instances: activeRecords(records, alive).map((record) => instanceSummary(record, alive)) };
}

export function resolveInstanceByRef(
  records: PluginInstanceRecord[],
  ref: string,
  alive: AlivePredicate = isAlive,
): PluginInstanceRecord | undefined {
  const matchedById = records.find((record) => record.instanceId !== "" && record.instanceId === ref);
  if (matchedById !== undefined) return matchedById;
  if (!/^\d+$/.test(ref)) return undefined;
  const port = Number(ref);
  if (!Number.isInteger(port) || port < 1 || port > 65535) return undefined;
  const matchedByPort = records.filter(
    (record) => record.port === port || record.mcpEndpoint?.port === port || record.apiEndpoint?.port === port,
  );
  return matchedByPort.find((record) => record.pid !== null && alive(record.pid)) ?? matchedByPort[0];
}

export function selectDefaultInstance(
  records: PluginInstanceRecord[],
  alive: AlivePredicate = isAlive,
): DefaultSelection {
  const active = activeRecords(records, alive);
  if (active.length === 0) return { kind: "none" };
  if (active.length === 1) return { kind: "single", record: active[0] };
  return { kind: "ambiguous", candidates: active.map((record) => instanceSummary(record, alive)) };
}

export async function checkInstanceReachable(
  record: PluginInstanceRecord,
  kind: EndpointKind,
  probe: ProbeFn = probeEndpoint,
  alive: AlivePredicate = isAlive,
): Promise<boolean> {
  if (record.pid === null || !alive(record.pid)) return false;
  const endpoint = instanceEndpoint(record, kind);
  if (endpoint === null) return false;
  return await probe({ host: endpoint.host, port: endpoint.port, url: endpointBaseUrl(endpoint) }, "light");
}

export function upstreamUrl(endpoint: EndpointEntry, restPath: string): string {
  const host = endpoint.host.includes(":") ? `[${endpoint.host}]` : endpoint.host;
  return `http://${host}:${endpoint.port}${restPath}`;
}
