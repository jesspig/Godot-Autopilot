import net from "node:net";

import { CliError, ExitCode } from "./errors";

export type ProbeLevel = "light" | "strict";

export const PROBE_TIMEOUT_MS = 800;

export interface ProbeTarget {
  host: string;
  port: number;
  url: string;
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

export async function probeHttp(url: string, method: string, timeoutMs: number = PROBE_TIMEOUT_MS): Promise<boolean> {
  try {
    await fetch(url, { method, signal: AbortSignal.timeout(timeoutMs) });
    return true;
  } catch {
    return false;
  }
}

export async function probeEndpoint(
  target: ProbeTarget,
  level: ProbeLevel,
  timeoutMs: number = PROBE_TIMEOUT_MS,
): Promise<boolean> {
  if (!(await probeTcp(target.host, target.port, timeoutMs))) return false;
  if (level === "light") return true;
  return await probeHttp(target.url, "GET", timeoutMs);
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
