import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { err } from "../output";
import { isAlive, stopForce, stopGraceful } from "../process";
import type { InstanceEntry } from "../registry";
import { writeJson } from "./json-output";
import { instanceView, start } from "./start";
import {
  DEFAULT_STOP_WAIT_SEC,
  removeFromRegistry,
  resolveTarget,
  stopMethod,
  waitUntilDead,
} from "./stop";

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
  const entry = resolveTarget(positional, opts, "restart");
  const method = stopMethod(opts.force, entry.headless);

  if (isAlive(entry.pid)) {
    if (method === "force") {
      stopForce(entry.pid);
    } else {
      stopGraceful(entry.pid);
    }
    const gone = await waitUntilDead(entry.pid, DEFAULT_STOP_WAIT_SEC);
    if (!gone) {
      throw new CliError(
        ExitCode.Transport,
        `instance ${entry.id} (pid ${entry.pid}) is still running after ${DEFAULT_STOP_WAIT_SEC}s; refusing to start a replacement to avoid two editors on the same project`,
        "retry with --force, or stop it manually before running gda restart again",
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
        portText: String(entry.port),
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

  writeJson(
    {
      ok: true,
      restarted: true,
      previous: { id: entry.id, pid: entry.pid, port: entry.port },
      ...instanceView(started),
    },
    opts,
  );
  return ExitCode.Success;
}
