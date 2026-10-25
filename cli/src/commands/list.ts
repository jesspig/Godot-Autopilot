import type { GlobalOptions } from "../args";
import { ExitCode } from "../errors";
import { err } from "../output";
import { isAlive } from "../process";
import { loadRegistry } from "../registry";
import { writeJson } from "./json-output";

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  const data = loadRegistry();
  const instances = data.instances.map((e) => ({
    id: e.id,
    project: e.project,
    port: e.port,
    pid: e.pid,
    headless: e.headless,
    startedAt: e.startedAt,
    live: isAlive(e.pid),
  }));
  writeJson(instances, opts);
  if (instances.length === 0) {
    err("no CLI-started instances; a live editor started elsewhere is visible via `gda endpoints`.");
  }
  return ExitCode.Success;
}
