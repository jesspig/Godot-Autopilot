import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import {
  applySkillSync,
  builtinSkillsDigest,
  builtinSkillsFiles,
  builtinSkillsVersion,
  listSkills,
  planSkillSync,
  readSkillFile,
  resolveSkillsRoot,
  verifyInstalledSkills,
  type SkillLanguage,
} from "../skills";
import { writeJson } from "./json-output";

async function runSync(positional: string[], opts: GlobalOptions): Promise<number> {
  const lang: SkillLanguage = opts.zh === true ? "zh" : "en";
  const target = opts.pathText !== undefined
    ? path.resolve(opts.pathText)
    : path.join(os.homedir(), ".agents", "skills");
  const dryRun = opts.check === true || positional.includes("--dry-run");
  const force = opts.force === true || positional.includes("--force");

  const verification = verifyInstalledSkills(target, lang);
  if (!dryRun && !force && !verification.ok && verification.reason !== "not_installed") {
    throw new CliError(
      ExitCode.Transport,
      `installed skills are damaged: ${verification.reason}`,
      "Run 'gda skills sync --force' to repair the managed files.",
    );
  }
  const files = builtinSkillsFiles(lang);
  const plan = planSkillSync(target, files, lang, {
    version: builtinSkillsVersion(),
    content_digest: builtinSkillsDigest(),
    dryRun,
    force,
    prune: opts.prune === true || force,
  });
  const applied = applySkillSync(plan);
  if (!applied.ok) {
    throw new CliError(
      ExitCode.Transport,
      `skill sync failed: ${applied.error}`,
      applied.rolled_back
        ? "The previous installation was preserved. Pass --force to overwrite managed files."
        : "Run 'gda skills sync --force' to repair.",
    );
  }
  writeJson(
    {
      ok: true,
      artifact: "skills",
      action: "sync",
      dry_run: plan.dry_run,
      force: plan.force,
      version: builtinSkillsVersion(),
      content_digest: builtinSkillsDigest(),
      lang,
      target,
      managed_books: plan.managed_books,
      old_version: plan.old_version,
      would_delete: plan.deletes,
      would_overwrite: plan.overwrites,
      would_remove_retired: plan.retired_removed,
      preserved: plan.preserved,
      written: applied.written,
      deleted: applied.deleted,
      retired_removed: applied.retired_removed,
      rolled_back: applied.rolled_back,
      verification: {
        ok: verification.ok,
        reason: verification.reason,
        version: verification.version,
      },
    },
    opts,
  );
  return ExitCode.Success;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, "skills requires a subcommand: list, show <name>, or sync");
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
  if (sub === "sync") {
    return await runSync(positional.slice(1), opts);
  }
  throw new CliError(ExitCode.Usage, `unknown skills subcommand: ${sub} (expected list, show, or sync)`);
}
