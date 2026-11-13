import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { listSkills, readSkillFile, resolveSkillsRoot, type SkillLanguage } from "../skills";
import { writeJson } from "./json-output";

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, "skills requires a subcommand: list or show <name>");
  }
  const lang: SkillLanguage = opts.zh === true ? "zh" : "en";
  const root = resolveSkillsRoot(lang);
  if (sub === "list") {
    writeJson(listSkills(root, lang), opts);
    return ExitCode.Success;
  }
  if (sub === "show") {
    const name = positional[1];
    if (name === undefined) {
      throw new CliError(ExitCode.Usage, "skills show requires a skill name");
    }
    process.stdout.write(readSkillFile(root, name, opts.file, lang));
    return ExitCode.Success;
  }
  throw new CliError(ExitCode.Usage, `unknown skills subcommand: ${sub} (expected list or show)`);
}
