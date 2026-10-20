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
