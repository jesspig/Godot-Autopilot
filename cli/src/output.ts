import type { GlobalOptions } from "./args";

export function renderJson(payload: unknown, opts: GlobalOptions): string {
  if (opts.pretty === true) {
    return JSON.stringify(payload, null, 2);
  }
  return JSON.stringify(payload);
}

export function err(message: string): void {
  process.stderr.write(`[gda] ${message}\n`);
}
