import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { listSkills, readSkillFile, resolveSkillsRoot } from "../skills";
import { writeJson } from "./json-output";

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, "skills requires a subcommand: list or show <name>");
  }
  const root = resolveSkillsRoot();
  if (sub === "list") {
    writeJson(listSkills(root), opts);
    return ExitCode.Success;
  }
  if (sub === "show") {
    const name = positional[1];
    if (name === undefined) {
      throw new CliError(ExitCode.Usage, "skills show requires a skill name");
    }
    process.stdout.write(readSkillFile(root, name, opts.file));
    return ExitCode.Success;
  }
  throw new CliError(ExitCode.Usage, `unknown skills subcommand: ${sub} (expected list or show)`);
}