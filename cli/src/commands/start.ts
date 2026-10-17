import type { GlobalOptions } from "../args";
import { ExitCode } from "../errors";
import { err } from "../output";

export async function run(_positional: string[], _opts: GlobalOptions): Promise<number> {
  err("not implemented: start");
  return ExitCode.Usage;
}
