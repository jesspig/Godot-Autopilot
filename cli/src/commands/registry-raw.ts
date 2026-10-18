import fs from "node:fs";

import { CliError, ExitCode } from "../errors";
import { joinPath } from "../platform";
import { stateDir, type InstanceEntry } from "../registry";

export function readRawEntry(id: string): InstanceEntry | undefined {
  let raw: string;
  try {
    raw = fs.readFileSync(joinPath(stateDir(), "instances.json"), "utf8");
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") return undefined;
    throw error;
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    throw new CliError(ExitCode.Transport, "cannot parse instances.json");
  }
  if (typeof parsed !== "object" || parsed === null || !Array.isArray((parsed as { instances?: unknown }).instances)) {
    throw new CliError(ExitCode.Transport, "cannot parse instances.json");
  }
  return (parsed as { instances: InstanceEntry[] }).instances.find((e) => e.id === id);
}
