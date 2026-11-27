import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { err } from "../output";
import { isAlive, stopForce, stopGraceful } from "../process";
import type { InstanceEntry } from "../registry";
import { writeJson } from "./json-output";
import { instanceView, start } from "./start";
import { collectTaskStorePoisoning, gracefulRestartSuggestion } from "./stop-diagnosis";
import {
  DEFAULT_STOP_WAIT_SEC,
  parseWaitSec,
  removeFromRegistry,
  resolveTarget,
  stopMethod,
  waitUntilDead,
} from "./stop";

export function restartPortNote(previousPort: number, newPort: number): string | null {
  if (previousPort === newPort) return null;
  return `restarted on port ${newPort}; the control-plane port is owned by the plugin, so it may differ from the previous port ${previousPort}`;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  try {
    return await restart(positional, opts);
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

async function restart(positional: string[], opts: GlobalOptions): Promise<number> {
  const stopWaitSec = parseWaitSec(opts.waitText, DEFAULT_STOP_WAIT_SEC);
  const entry = resolveTarget(positional, opts, "restart");
  const method = stopMethod(opts.force, entry.headless);

  if (isAlive(entry.pid)) {
    if (method === "force") {
      stopForce(entry.pid);
    } else {
      stopGraceful(entry.pid);
    }
    const gone = await waitUntilDead(entry.pid, stopWaitSec);
    if (!gone) {
      const poisoned = method === "graceful" && collectTaskStorePoisoning(entry);
      throw new CliError(
        ExitCode.Transport,
        `instance ${entry.id} (pid ${entry.pid}) is still running after ${stopWaitSec}s; refusing to start a replacement to avoid two editors on the same project`,
        gracefulRestartSuggestion(poisoned),
      );
    }
  }

  removeFromRegistry(entry.id);

  let started: InstanceEntry;
  try {
    started = await start(
      {
        ...opts,
        project: entry.project,
        godot: entry.godot,
        headless: entry.headless,
      },
      { writeOutput: false },
    );
  } catch (error) {
    const base = error instanceof CliError ? error : new CliError(ExitCode.Transport, String(error));
    throw new CliError(
      base.code,
      `${base.message}; the original instance ${entry.id} (pid ${entry.pid}) was already stopped`,
      base.suggestion,
    );
  }

  const portNote = restartPortNote(entry.port, started.port);
  writeJson(
    {
      ok: true,
      restarted: true,
      previous: { id: entry.id, pid: entry.pid, port: entry.port },
      ...instanceView(started),
      ...(portNote !== null ? { note: portNote } : {}),
    },
    opts,
  );
  return ExitCode.Success;
}
