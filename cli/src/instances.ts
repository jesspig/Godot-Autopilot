import fs from "node:fs";
import path from "node:path";

import { gdaRoot, projectPathKey } from "./platform";
import { isAlive } from "./process";

export const PLUGIN_INDEX_FILE = "instances.json";
export const DEFAULT_HOST = "127.0.0.1";
export const DEFAULT_MCP_PATH = "/mcp";
export const DEFAULT_API_PATH = "/api";
export const MCP_PROTOCOL = "mcp";
export const HTTP_PROTOCOL = "http";
export const MCP_AUTH = "none";
export const API_AUTH = "bearer";

export interface EndpointEntry {
  host: string;
  port: number;
  path: string;
  protocol: string;
  auth: string;
}

export interface PluginInstanceRecord {
  instanceId: string;
  port: number | null;
  pid: number | null;
  projectPath: string;
  apiToken: string;
  mcpEndpoint: EndpointEntry | null;
  apiEndpoint: EndpointEntry | null;
  startedAt: number;
  version: string;
}

export type EndpointKind = "mcp" | "api";

export function pluginIndexPath(): string {
  return path.join(gdaRoot(), PLUGIN_INDEX_FILE);
}

export function deriveEndpoint(port: number, kind: EndpointKind): EndpointEntry {
  if (kind === "mcp") {
    return { host: DEFAULT_HOST, port, path: DEFAULT_MCP_PATH, protocol: MCP_PROTOCOL, auth: MCP_AUTH };
  }
  return { host: DEFAULT_HOST, port, path: DEFAULT_API_PATH, protocol: HTTP_PROTOCOL, auth: API_AUTH };
}

export function endpointBaseUrl(endpoint: EndpointEntry): string {
  const base = endpoint.path === "/" ? "" : endpoint.path.replace(/\/+$/, "");
  return `http://${endpoint.host}:${endpoint.port}${base}`;
}

export function readPluginInstanceIndex(): PluginInstanceRecord[] {
  let raw: string;
  try {
    raw = fs.readFileSync(pluginIndexPath(), "utf8");
  } catch {
    return [];
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return [];
  }
  const records: PluginInstanceRecord[] = [];
  for (const item of extractItems(parsed)) {
    const record = parseRecord(item);
    if (record !== null) records.push(record);
  }
  return records;
}

export function findPluginRecordByProject(projectPath: string): PluginInstanceRecord | undefined {
  const key = projectPathKey(projectPath);
  const matched = readPluginInstanceIndex().filter(
    (r) => r.projectPath !== "" && projectPathKey(r.projectPath) === key,
  );
  return pickPreferred(matched);
}

export function findPluginRecordByPort(port: number): PluginInstanceRecord | undefined {
  const matched = readPluginInstanceIndex().filter(
    (r) => r.port === port || r.mcpEndpoint?.port === port || r.apiEndpoint?.port === port,
  );
  return pickPreferred(matched);
}

function pickPreferred(records: PluginInstanceRecord[]): PluginInstanceRecord | undefined {
  if (records.length === 0) return undefined;
  const alive = records.filter((r) => r.pid !== null && isAlive(r.pid));
  const pool = alive.length > 0 ? alive : records;
  return pool.reduce((best, current) => (current.startedAt > best.startedAt ? current : best));
}

function extractItems(parsed: unknown): unknown[] {
  if (Array.isArray(parsed)) return parsed;
  if (typeof parsed !== "object" || parsed === null) return [];
  const doc = parsed as Record<string, unknown>;
  for (const key of ["instances", "records"]) {
    const value = doc[key];
    if (Array.isArray(value)) return value;
  }
  return [];
}

function parseRecord(item: unknown): PluginInstanceRecord | null {
  if (typeof item !== "object" || item === null || Array.isArray(item)) return null;
  const doc = item as Record<string, unknown>;
  const endpoints = readObject(doc["endpoints"]);
  const flatPort = readPort(doc);
  const mcpPort = readEndpointPort(endpoints["mcp"]) ?? flatPort;
  const apiPort = readEndpointPort(endpoints["api"]) ?? mcpPort ?? flatPort;
  return {
    instanceId: readString(doc, ["instance_id", "instanceId", "id"]) ?? "",
    port: flatPort ?? mcpPort ?? apiPort,
    pid: readNumber(doc, ["pid"]),
    projectPath: readString(doc, ["project_path", "projectPath", "project"]) ?? "",
    apiToken: readString(doc, ["api_token", "apiToken", "token"]) ?? "",
    mcpEndpoint: buildEndpoint(endpoints["mcp"], mcpPort, DEFAULT_MCP_PATH, MCP_PROTOCOL, MCP_AUTH),
    apiEndpoint: buildEndpoint(endpoints["api"], apiPort, DEFAULT_API_PATH, HTTP_PROTOCOL, API_AUTH),
    startedAt: readNumber(doc, ["started_at", "startedAt"]) ?? 0,
    version: readString(doc, ["version"]) ?? "",
  };
}

function buildEndpoint(
  raw: unknown,
  port: number | null,
  defaultPath: string,
  defaultProtocol: string,
  defaultAuth: string,
): EndpointEntry | null {
  if (port === null) return null;
  const doc = readObject(raw);
  return {
    host: readString(doc, ["host"]) ?? DEFAULT_HOST,
    port,
    path: normalizePathValue(readString(doc, ["path"])) ?? defaultPath,
    protocol: readString(doc, ["protocol"]) ?? defaultProtocol,
    auth: readString(doc, ["auth"]) ?? defaultAuth,
  };
}

function readObject(value: unknown): Record<string, unknown> {
  if (typeof value !== "object" || value === null || Array.isArray(value)) return {};
  return value as Record<string, unknown>;
}

function readString(doc: Record<string, unknown>, keys: string[]): string | undefined {
  for (const key of keys) {
    const value = doc[key];
    if (typeof value === "string" && value.trim() !== "") return value;
  }
  return undefined;
}

function readNumber(doc: Record<string, unknown>, keys: string[]): number | null {
  for (const key of keys) {
    const value = doc[key];
    if (typeof value === "number" && Number.isFinite(value)) return value;
    if (typeof value === "string" && /^\d+$/.test(value.trim())) return Number(value.trim());
  }
  return null;
}

function readPort(doc: Record<string, unknown>): number | null {
  const value = readNumber(doc, ["port"]);
  if (value === null) return null;
  if (!Number.isInteger(value) || value < 1 || value > 65535) return null;
  return value;
}

function readEndpointPort(raw: unknown): number | null {
  const doc = readObject(raw);
  if (Object.keys(doc).length === 0) return null;
  return readPort(doc);
}

function normalizePathValue(value: string | undefined): string | undefined {
  if (value === undefined) return undefined;
  const trimmed = value.trim();
  if (trimmed === "") return undefined;
  return trimmed.startsWith("/") ? trimmed : `/${trimmed}`;
}
