import type { GlobalOptions } from "../args";
import { renderJson } from "../output";

export function writeJson(payload: unknown, opts: GlobalOptions): void {
  process.stdout.write(`${renderJson(payload, opts)}\n`);
}
