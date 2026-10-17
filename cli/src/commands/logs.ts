import fs from "node:fs";

import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { readRawEntry } from "./registry-raw";

const DEFAULT_TAIL = 50;

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  try {
    return await logs(positional, opts);
  } catch (error) {
    if (error instanceof CliError) {
      process.stderr.write(`[gda] ${error.message}\n`);
      if (error.suggestion !== undefined) {
        process.stderr.write(`[gda] suggestion: ${error.suggestion}\n`);
      }
      return error.code;
    }
    throw error;
  }
}

async function logs(positional: string[], _opts: GlobalOptions): Promise<number> {
  const id = positional[0];
  if (id === undefined) {
    throw new CliError(ExitCode.Usage, "usage: gda logs <id> [--tail <n>]");
  }
  const entry = readRawEntry(id);
  if (entry === undefined) {
    throw new CliError(ExitCode.Transport, `unknown instance: ${id} (run "gda list" to see running instances)`);
  }
  const tail = parseTail(_opts.tailText);
  const lines = readLogLines(entry.log, id);
  const body = lines.slice(-tail).join("\n");
  if (body.length > 0) {
    process.stdout.write(`${body}\n`);
  }
  return ExitCode.Success;
}

function readLogLines(logPath: string, id: string): string[] {
  let text: string;
  try {
    text = fs.readFileSync(logPath, "utf8");
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") {
      throw new CliError(ExitCode.Transport, `log file missing for instance ${id}: ${logPath}`);
    }
    throw error;
  }
  const lines = text.split(/\r?\n/);
  if (lines.length > 0 && lines[lines.length - 1] === "") {
    lines.pop();
  }
  return lines;
}

function parseTail(text: string | undefined): number {
  if (text === undefined) return DEFAULT_TAIL;
  const n = Number(text);
  if (!Number.isInteger(n) || n < 0) {
    throw new CliError(ExitCode.Usage, `--tail must be a non-negative integer, got: ${text}`);
  }
  return n;
}
