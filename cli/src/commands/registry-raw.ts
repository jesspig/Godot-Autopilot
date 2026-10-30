import fs from "node:fs";

import { CliError, ExitCode } from "../errors";
import { joinPath } from "../platform";
import { stateDir, normalizeEntry, withLock, type InstanceEntry } from "../registry";

export function readRawEntries(): InstanceEntry[] {
  return withLock(() => {
    let raw: string;
    try {
      raw = fs.readFileSync(joinPath(stateDir(), "instances.json"), "utf8");
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code === "ENOENT") return [];
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
    return (parsed as { instances: InstanceEntry[] }).instances.map(normalizeEntry);
  });
}

export function readRawEntry(id: string): InstanceEntry | undefined {
  return readRawEntries().find((e) => e.id === id);
}
