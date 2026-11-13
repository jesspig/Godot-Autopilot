import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { run as runInstall } from "./install";
import { updateCheck, updateCli, writeUpdateResult, type UpdateItemResult } from "../update";
import { writeJson } from "./json-output";

type Target = "cli" | "skills" | "plugin" | "all";

function parseTarget(text: string | undefined): Target {
  if (text === "cli" || text === "skills" || text === "plugin" || text === "all") return text;
  throw new CliError(ExitCode.Usage, "update requires cli, skills, plugin, or all");
}

function item(artifact: UpdateItemResult["artifact"], ok: boolean, error: string): UpdateItemResult {
  return {
    artifact,
    from: "",
    to: "",
    source: "cache",
    ok,
    action: ok ? "updated" : "failed",
    error,
  };
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const target = parseTarget(positional[0]);
  if (opts.category === "check") {
    throw new CliError(ExitCode.Usage, "use --check instead of a positional check");
  }
  if (opts.check === true) {
    writeJson(updateCheck(), opts);
    return ExitCode.Success;
  }
  const startedAt = new Date().toISOString();
  const results: UpdateItemResult[] = [];
  const runOne = async (itemTarget: Exclude<Target, "all">): Promise<boolean> => {
    try {
      const code =
        itemTarget === "cli"
          ? await updateCli()
          : await runInstall(
              [itemTarget],
              { ...opts, sourceText: opts.sourceText ?? "online" },
            );
      results.push(item(itemTarget, code === ExitCode.Success, ""));
      return code === ExitCode.Success;
    } catch (error) {
      results.push(item(itemTarget, false, String(error)));
      return false;
    }
  };

  let ok = true;
  if (target === "all") {
    ok = (await runOne("skills")) && ok;
    if (ok) ok = (await runOne("plugin")) && ok;
    writeUpdateResult({
      schema_version: 1,
      started_at: startedAt,
      finished_at: new Date().toISOString(),
      targets: ["skills", "plugin", "cli"],
      results,
      rollback: { performed: false, reason: "" },
      cli_restart: { exit_requested: false, exit_code: 0 },
    });
    if (ok) {
      ok = (await runOne("cli")) && ok;
    }
  } else {
    ok = await runOne(target);
    writeUpdateResult({
      schema_version: 1,
      started_at: startedAt,
      finished_at: new Date().toISOString(),
      targets: [target],
      results,
      rollback: { performed: false, reason: "" },
      cli_restart: { exit_requested: target === "cli", exit_code: 0 },
    });
  }
  writeJson(
    {
      schema_version: 1,
      ok,
      targets: target === "all" ? ["skills", "plugin", "cli"] : [target],
      results,
    },
    opts,
  );
  return ok ? ExitCode.Success : ExitCode.ToolFailure;
}
