import { spawnSync } from "node:child_process";
import fs from "node:fs";

import { isAlive, stopForce } from "../../src/process";

const POLL_INTERVAL_MS = 50;
const KILL_TIMEOUT_MS = 10_000;
const STATE_TIMEOUT_MS = 2_000;
const RM_ATTEMPTS = 6;
const RM_BASE_DELAY_MS = 100;

export async function waitFor(probe: () => boolean, timeoutMs = STATE_TIMEOUT_MS): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    if (probe()) return true;
    if (Date.now() >= deadline) return false;
    await Bun.sleep(POLL_INTERVAL_MS);
  }
}

export async function waitDead(pid: number, timeoutMs = KILL_TIMEOUT_MS): Promise<boolean> {
  return waitFor(() => !isAlive(pid), timeoutMs);
}

export async function killPid(pid: number, timeoutMs = KILL_TIMEOUT_MS): Promise<void> {
  if (!isAlive(pid)) return;
  stopForce(pid);
  if (!(await waitDead(pid, timeoutMs))) {
    process.stderr.write(`[cleanup] pid ${pid} survived force stop after ${timeoutMs}ms\n`);
  }
}

export function imageRunning(imageName: string): boolean {
  const result = spawnSync("tasklist", ["/FI", `IMAGENAME eq ${imageName}`, "/NH", "/FO", "CSV"], { encoding: "utf8" });
  return (result.stdout ?? "").toLowerCase().includes(`"${imageName.toLowerCase()}"`);
}

export function imageNameOf(pid: number): string | null {
  const result = spawnSync("tasklist", ["/FI", `PID eq ${pid}`, "/NH", "/FO", "CSV"], { encoding: "utf8" });
  for (const line of (result.stdout ?? "").split(/\r?\n/)) {
    const m = /^"([^"]*)","(\d+)"/.exec(line.trim());
    if (m !== null && Number(m[2]) === pid) return m[1];
  }
  return null;
}

export async function killImage(imageName: string, timeoutMs = KILL_TIMEOUT_MS): Promise<void> {
  if (!imageRunning(imageName)) return;
  spawnSync("taskkill", ["/F", "/IM", imageName], { stdio: "ignore" });
  if (!(await waitFor(() => !imageRunning(imageName), timeoutMs))) {
    process.stderr.write(`[cleanup] image ${imageName} survived force stop after ${timeoutMs}ms\n`);
  }
}

export async function removeDirWithRetry(dir: string): Promise<void> {
  let delayMs = RM_BASE_DELAY_MS;
  for (let attempt = 1; attempt <= RM_ATTEMPTS; attempt += 1) {
    try {
      fs.rmSync(dir, { recursive: true, force: true });
      return;
    } catch (error) {
      const code = (error as NodeJS.ErrnoException).code;
      const transient = code === "EBUSY" || code === "EPERM" || code === "ENOTEMPTY";
      if (!transient || attempt === RM_ATTEMPTS) {
        process.stderr.write(`[cleanup] cannot remove ${dir} (attempt ${attempt}): ${String(error)}\n`);
        return;
      }
      await Bun.sleep(delayMs);
      delayMs *= 2;
    }
  }
}
