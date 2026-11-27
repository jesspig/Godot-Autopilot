import net from "node:net";

import { CliError, ExitCode } from "./errors";
import { IpcSession, type IpcEndpoint } from "./ipc/client";

export type ProbeLevel = "light" | "strict";

export const PROBE_TIMEOUT_MS = 800;

export interface NativeProbeTarget {
  host: string;
  port: number;
}

export function parseProbeLevel(text: string | undefined): ProbeLevel {
  if (text === undefined) return "light";
  const value = text.trim().toLowerCase();
  if (value === "light" || value === "strict") return value;
  throw new CliError(ExitCode.Usage, `--probe must be "light" or "strict", got: ${text}`);
}

export function probeTcp(host: string, port: number, timeoutMs: number = PROBE_TIMEOUT_MS): Promise<boolean> {
  return new Promise<boolean>((resolve) => {
    const socket = new net.Socket();
    let settled = false;
    const finish = (alive: boolean): void => {
      if (settled) return;
      settled = true;
      socket.destroy();
      resolve(alive);
    };
    socket.setTimeout(timeoutMs);
    socket.once("connect", () => finish(true));
    socket.once("timeout", () => finish(false));
    socket.once("error", () => finish(false));
    socket.connect(port, host);
  });
}

function probeLocalPath(endpoint: IpcEndpoint, timeoutMs: number): Promise<boolean> {
  const target = endpoint.kind === "pipe"
    ? `\\\\.\\pipe\\${endpoint.name ?? ""}`
    : endpoint.path ?? "";
  if (target === "") return Promise.resolve(false);
  return new Promise<boolean>((resolve) => {
    const socket = new net.Socket();
    let settled = false;
    const finish = (alive: boolean): void => {
      if (settled) return;
      settled = true;
      socket.destroy();
      resolve(alive);
    };
    socket.setTimeout(timeoutMs);
    socket.once("connect", () => finish(true));
    socket.once("timeout", () => finish(false));
    socket.once("error", () => finish(false));
    socket.connect(target);
  });
}

function probeTransport(endpoint: IpcEndpoint, timeoutMs: number): Promise<boolean> {
  if (endpoint.kind === "tcp") return probeTcp(endpoint.host ?? "127.0.0.1", endpoint.port ?? 0, timeoutMs);
  const empty = endpoint.kind === "pipe" ? endpoint.name === undefined || endpoint.name === "" : endpoint.path === undefined || endpoint.path === "";
  return empty ? Promise.resolve(false) : probeLocalPath(endpoint, timeoutMs);
}

export async function probeNativeEndpoint(
  endpoint: IpcEndpoint,
  level: ProbeLevel,
  timeoutMs: number = PROBE_TIMEOUT_MS,
): Promise<boolean> {
  if (!(await probeTransport(endpoint, timeoutMs))) return false;
  if (level === "light") return true;
  const session = new IpcSession(endpoint, timeoutMs);
  try {
    await session.open();
    await session.request({
      jsonrpc: "2.0",
      id: 1,
      method: "initialize",
      params: { protocolVersion: "1", capabilities: {}, clientInfo: { name: "gda-probe", version: "1" } },
    });
    return true;
  } catch {
    return false;
  } finally {
    session.close();
  }
}

export async function probeEndpoint(
  target: NativeProbeTarget,
  level: ProbeLevel,
  timeoutMs: number = PROBE_TIMEOUT_MS,
): Promise<boolean> {
  return await probeNativeEndpoint(
    { kind: "tcp", host: target.host, port: target.port },
    level,
    timeoutMs,
  );
}

export const ENDPOINT_ALIVE_TTL_MS = 5000;
export const ENDPOINT_ALIVE_TIMEOUT_MS = 300;

interface AliveCacheEntry {
  alive: boolean;
  at: number;
}

const aliveCache = new Map<number, AliveCacheEntry>();

export async function endpointAlive(
  port: number,
  timeoutMs: number = ENDPOINT_ALIVE_TIMEOUT_MS,
): Promise<boolean> {
  const cached = aliveCache.get(port);
  if (cached !== undefined && Date.now() - cached.at < ENDPOINT_ALIVE_TTL_MS) {
    return cached.alive;
  }
  const alive = await probeTcp("127.0.0.1", port, timeoutMs);
  aliveCache.set(port, { alive, at: Date.now() });
  return alive;
}

export function cachedEndpointAlive(port: number): boolean | null {
  const cached = aliveCache.get(port);
  if (cached === undefined || Date.now() - cached.at >= ENDPOINT_ALIVE_TTL_MS) {
    return null;
  }
  return cached.alive;
}

export function primeEndpointAlive(port: number, alive: boolean): void {
  aliveCache.set(port, { alive, at: Date.now() });
}
