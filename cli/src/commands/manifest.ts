import type { GlobalOptions } from "../args";
import { buildManifest, commandsHelpText } from "../command-spec";
import { writeJson } from "./json-output";

export async function run(_positionals: string[], opts: GlobalOptions): Promise<number> {
  if (opts.help === true) {
    process.stdout.write(`${commandsHelpText()}\n`);
    return 0;
  }
  writeJson(buildManifest(), opts);
  return 0;
}
