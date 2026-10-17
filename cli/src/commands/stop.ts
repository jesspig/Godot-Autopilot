import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { err } from "../output";
import { isAlive, stopForce, stopGraceful } from "../process";
import { findLiveByProject, loadRegistry, saveRegistry, type InstanceEntry } from "../registry";
import { readRawEntry } from "./registry-raw";
import { writeJson } from "./json-output";

const DEFAULT_WAIT_SEC = 10;
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

async function stop(positional: string[], opts: GlobalOptions): Promise<number> {
  const waitSec = parseWaitSec(opts.waitText, DEFAULT_WAIT_SEC);
  const entry = resolveTarget(positional, opts);
  const method = opts.force === true ? "force" : "graceful";

  if (isAlive(entry.pid)) {
    if (method === "force") {
      stopForce(entry.pid);
    } else {
      stopGraceful(entry.pid);
    }
    const gone = await waitUntilDead(entry.pid, waitSec);
    if (!gone) {
      const hint = method === "force"
        ? "the process survived a force stop"
        : "the process is still running; retry with --force";
      writeJson({ ok: false, method, id: entry.id, pid: entry.pid, project: entry.project, hint }, opts);
      err(`instance ${entry.id} (pid ${entry.pid}) still alive after ${waitSec}s; ${hint}`);
      return ExitCode.Transport;
    }
  }

  removeFromRegistry(entry.id);
  writeJson({ ok: true, method, id: entry.id, pid: entry.pid, project: entry.project }, opts);
  return ExitCode.Success;
}

function resolveTarget(positional: string[], opts: GlobalOptions): InstanceEntry {
  const id = positional[0];
  if (id !== undefined) {
    const entry = readRawEntry(id);
    if (entry === undefined) {
      throw new CliError(ExitCode.Transport, `unknown instance: ${id} (run "gda list" to see running instances)`);
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
  throw new CliError(ExitCode.Usage, "usage: gda stop <id> [--force] [--wait <sec>] | gda stop --project <dir> [--force] [--wait <sec>]");
}

function removeFromRegistry(id: string): void {
  const data = loadRegistry();
  data.instances = data.instances.filter((e) => e.id !== id);
  saveRegistry(data);
}

async function waitUntilDead(pid: number, waitSec: number): Promise<boolean> {
  const deadline = Date.now() + waitSec * 1000;
  for (;;) {
    if (!isAlive(pid)) return true;
    if (Date.now() >= deadline) return false;
    await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
  }
}

function parseWaitSec(text: string | undefined, fallback: number): number {
  if (text === undefined) return fallback;
  const n = Number(text);
  if (!Number.isFinite(n) || n < 0) {
    throw new CliError(ExitCode.Usage, `--wait must be a non-negative number of seconds, got: ${text}`);
  }
  return n;
}
