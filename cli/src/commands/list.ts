import type { GlobalOptions } from "../args";
import { ExitCode } from "../errors";
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
  return ExitCode.Success;
}
