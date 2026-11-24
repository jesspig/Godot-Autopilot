import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { err } from "../output";
import { isAlive, processStartTime, stopDecision, stopForce, stopGraceful } from "../process";
import { matchesProcessIdentity } from "../security-policy";
import { cachedEndpointAlive } from "../probe";
import { findLiveByProject, withRegistryLock, type InstanceEntry } from "../registry";
import { assertSingleSelector, parseTargetPort } from "../target";
import { readRawEntries, readRawEntry } from "./registry-raw";
import { collectTaskStorePoisoning, stopFailureHint } from "./stop-diagnosis";
import { writeJson } from "./json-output";

export const DEFAULT_STOP_WAIT_SEC = 10;
const POLL_INTERVAL_MS = 100;

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  try {
    return await stop(positional, opts);
  } catch (error) {
    if (error instanceof CliError) {
      err(error.message);
      if (error.suggestion !== undefined) {
        process.stderr.write(`[gda] suggestion: ${error.suggestion}\n`);
      }
      return error.code;
    }
    throw error;
  }
}

export function stopMethod(force: boolean | undefined, headless: boolean): "force" | "graceful" {
  return force === true || headless ? "force" : "graceful";
}

async function stop(positional: string[], opts: GlobalOptions): Promise<number> {
  const waitSec = parseWaitSec(opts.waitText, DEFAULT_STOP_WAIT_SEC);
  const entry = resolveTarget(positional, opts);
  const method = stopMethod(opts.force, entry.headless);

  const endpointPort = entry.instancePort ?? entry.port ?? null;
  const endpointAlive = typeof endpointPort === "number" && endpointPort > 0
    ? cachedEndpointAlive(endpointPort)
    : null;
  if (entry.pid !== null && entry.startedAt > 0) {
    const observedStart = processStartTime(entry.pid);
    if (observedStart !== null &&
        !matchesProcessIdentity(
          { pid: entry.pid, started_at: entry.startedAt },
          { pid: entry.pid, started_at: observedStart },
        )) {
      throw new CliError(
        ExitCode.Transport,
        `instance ${entry.id} (pid ${entry.pid}) no longer matches its recorded start time; refusing to stop a reused pid`,
        "Run 'gda list' and select the current instance explicitly.",
      );
    }
  }
  const decision = stopDecision(isAlive(entry.pid), endpointPort, endpointAlive);
  if (decision.identityWarning) {
    err(`instance ${entry.id} (pid ${entry.pid}) endpoint on port ${endpointPort} did not confirm identity; falling back to pid-based stop`);
  }
  if (decision.stop) {
    if (method === "force") {
      stopForce(entry.pid);
    } else {
      stopGraceful(entry.pid);
    }
    const gone = await waitUntilDead(entry.pid, waitSec);
    if (!gone) {
      const poisoned = method === "graceful" && collectTaskStorePoisoning(entry);
      const hint = stopFailureHint(method, poisoned);
      writeJson({ ok: false, method, id: entry.id, pid: entry.pid, project: entry.project, hint }, opts);
      err(`instance ${entry.id} (pid ${entry.pid}) still alive after ${waitSec}s; ${hint}`);
      return ExitCode.Transport;
    }
  }

  removeFromRegistry(entry.id);
  writeJson({ ok: true, method, id: entry.id, pid: entry.pid, project: entry.project }, opts);
  return ExitCode.Success;
}

export function resolveTarget(positional: string[], opts: GlobalOptions, usageCommand = "stop"): InstanceEntry {
  const positionalId = positional[0];
  if (positionalId !== undefined) {
    if (opts.id !== undefined || opts.portText !== undefined || opts.project !== undefined) {
      throw new CliError(ExitCode.Usage, "use only one of <id>/--id/--port/--project");
    }
    return registeredEntry(positionalId);
  }

  assertSingleSelector(opts);

  if (opts.id !== undefined) {
    return registeredEntry(opts.id);
  }

  if (opts.portText !== undefined) {
    const port = parseTargetPort(opts.portText);
    const entry = entryByPort(port);
    if (entry === undefined) {
      throw new CliError(
        ExitCode.Transport,
        `no registered instance on port ${port} (the CLI can only stop instances it started; run "gda list" to see running instances)`,
      );
    }
    return entry;
  }

  if (opts.project !== undefined) {
    const entry = findLiveByProject(opts.project);
    if (entry === undefined) {
      throw new CliError(ExitCode.Transport, `no running instance for project: ${opts.project} (run "gda list" to see running instances)`);
    }
    return entry;
  }

  throw new CliError(
    ExitCode.Usage,
    `usage: gda ${usageCommand} <id> [--force] [--wait <sec>] | gda ${usageCommand} (--id <id> | --port <port> | --project <dir>) [--force] [--wait <sec>]`,
  );
}

function registeredEntry(id: string): InstanceEntry {
  const entry = readRawEntry(id);
  if (entry === undefined) {
    throw new CliError(ExitCode.Transport, `unknown instance: ${id} (run "gda list" to see running instances)`);
  }
  return entry;
}

function entryByPort(port: number): InstanceEntry | undefined {
  const entries = readRawEntries().filter((e) => e.port === port);
  return entries.find((e) => isAlive(e.pid)) ?? entries[0];
}

export function removeFromRegistry(id: string): void {
  withRegistryLock((data) => {
    data.instances = data.instances.filter((e) => e.id !== id);
  });
}

export async function waitUntilDead(pid: number, waitSec: number): Promise<boolean> {
  const deadline = Date.now() + waitSec * 1000;
  for (;;) {
    if (!isAlive(pid)) return true;
    if (Date.now() >= deadline) return false;
    await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
  }
}

export function parseWaitSec(text: string | undefined, fallback: number): number {
  if (text === undefined) return fallback;
  const n = Number(text);
  if (!Number.isFinite(n) || n < 0) {
    throw new CliError(ExitCode.Usage, `--wait must be a non-negative number of seconds, got: ${text}`);
  }
  return n;
}
