import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { listProjects } from "../projects";
import { writeJson } from "./json-output";
import { run as runProjects } from "./projects";
import { run as runStart } from "./start";

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  const rest = positional.slice(1);
  if (sub === undefined || sub === "list") {
    return runProjects(rest, opts);
  }
  if (sub === "info") {
    const key = rest[0];
    if (key === undefined) {
      throw new CliError(ExitCode.Usage, "project info requires <dir|hash>.");
    }
    const found = listProjects().find((project) => project.hash === key || project.path === key);
    writeJson(found ?? null, opts);
    return ExitCode.Success;
  }
  if (sub === "open") {
    const dir = rest[0];
    if (dir === undefined) {
      throw new CliError(ExitCode.Usage, "project open requires <dir>.");
    }
    return runStart(rest.slice(1), { ...opts, project: dir });
  }
  throw new CliError(ExitCode.Usage, `unknown project subcommand: ${sub} (expected list/info/open)`);
}
