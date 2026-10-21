import { endpointBaseUrl, readPluginInstanceIndex } from "./instances";
import { parseProbeLevel, probeEndpoint, type ProbeLevel } from "./probe";
import { projectPathKey } from "./platform";
import { isAlive } from "./process";
import { findLiveByProject } from "./registry";

export const MULTI_INSTANCE_ENV = "GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE";

export type ActiveInstanceSource = "cli-registry" | "plugin-index";

export interface ActiveInstance {
  source: ActiveInstanceSource;
  port: number | null;
  pid: number | null;
  instanceId: string | null;
}

export function multiInstanceAllowed(): boolean {
  const raw = process.env[MULTI_INSTANCE_ENV];
  if (raw === undefined) return false;
  const value = raw.trim().toLowerCase();
  if (value === "") return false;
  if (value === "0" || value === "false" || value === "off" || value === "no") return false;
  return true;
}

export function resolveProbeLevel(probeText: string | undefined): ProbeLevel {
  return parseProbeLevel(probeText);
}

export async function detectActiveInstance(
  projectDir: string,
  level: ProbeLevel = "light",
): Promise<ActiveInstance | undefined> {
  const registered = findLiveByProject(projectDir);
  if (registered !== undefined) {
    return {
      source: "cli-registry",
      port: registered.port,
      pid: registered.pid,
      instanceId: registered.id,
    };
  }
  return await detectLivePluginInstance(projectDir, level);
}

async function detectLivePluginInstance(
  projectDir: string,
  level: ProbeLevel,
): Promise<ActiveInstance | undefined> {
  const key = projectPathKey(projectDir);
  for (const record of readPluginInstanceIndex()) {
    if (record.projectPath === "") continue;
    if (projectPathKey(record.projectPath) !== key) continue;
    if (record.pid === null || !isAlive(record.pid)) continue;
    const endpoint = record.apiEndpoint ?? record.mcpEndpoint;
    if (endpoint === null) continue;
    const alive = await probeEndpoint(
      { host: endpoint.host, port: endpoint.port, url: endpointBaseUrl(endpoint) },
      level,
    );
    if (!alive) continue;
    return {
      source: "plugin-index",
      port: record.port,
      pid: record.pid,
      instanceId: record.instanceId === "" ? null : record.instanceId,
    };
  }
  return undefined;
}

export function describeInstance(active: ActiveInstance): string {
  const port = active.port === null ? "an unknown port" : `port ${active.port}`;
  const pid = active.pid === null ? "an unknown pid" : `pid ${active.pid}`;
  return `${port} (${pid})`;
}
