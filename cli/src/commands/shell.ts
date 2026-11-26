import { spawn, type ChildProcess } from "node:child_process";

import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";

export type ShellFrame = Record<string, unknown>;

export interface ShellSession {
  handle(line: string): Promise<boolean>;
  shutdown(): Promise<void>;
}

function parseRequest(line: string): Record<string, unknown> {
  let parsed: unknown;
  try {
    parsed = JSON.parse(line);
  } catch {
    throw new CliError(ExitCode.Usage, "shell ndjson line is not valid JSON.");
  }
  if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) {
    throw new CliError(ExitCode.Usage, "shell ndjson request must be a JSON object.");
  }
  return parsed as Record<string, unknown>;
}

function requestId(request: Record<string, unknown>): string {
  const id = request.id;
  if (typeof id !== "string" || id === "") {
    throw new CliError(ExitCode.Usage, "shell ndjson request requires a string id.");
  }
  return id;
}

function argvFor(request: Record<string, unknown>): { argv: string[]; command?: string } {
  const command = request.command;
  if (typeof command === "string" && command !== "") {
    return { argv: ["sh", "-c", command], command };
  }
  const argv = request.argv;
  if (!Array.isArray(argv) || argv.length === 0 || argv.some((item) => typeof item !== "string")) {
    throw new CliError(ExitCode.Usage, "shell ndjson run requires argv: string[] or command: string.");
  }
  return { argv: argv as string[] };
}

export function createShellSession(emit: (frame: ShellFrame) => void): ShellSession {
  const children = new Map<string, ChildProcess>();

  function track(id: string, child: ChildProcess): void {
    children.set(id, child);
    let carry = "";
    child.stdout?.on("data", (chunk: Buffer) => {
      carry += chunk.toString("utf8");
      const lines = carry.split(/\r?\n/);
      carry = lines.pop() ?? "";
      for (const line of lines) {
        if (line.trim() === "") continue;
        let envelope: unknown;
        try {
          envelope = JSON.parse(line);
        } catch {
          envelope = { text: line };
        }
        emit({ type: "envelope", id, envelope });
      }
    });
    child.stderr?.on("data", (chunk: Buffer) => {
      emit({ type: "envelope", id, envelope: { stderr: chunk.toString("utf8") } });
    });
    child.once("error", (error) => {
      children.delete(id);
      emit({ type: "envelope", id, envelope: { error: error.message } });
      emit({ type: "exitCode", id, exitCode: 1 });
    });
    child.once("close", (code) => {
      children.delete(id);
      emit({ type: "exitCode", id, exitCode: code ?? 1 });
    });
  }

  return {
    async handle(line: string): Promise<boolean> {
      if (line.trim() === "") return false;
      const request = parseRequest(line);
      const id = requestId(request);
      const type = request.type;
      if (type === "shutdown") {
        for (const child of children.values()) child.kill();
        children.clear();
        emit({ type: "shutdown", id, ok: true });
        return true;
      }
      if (type === "envelope") {
        emit({ type: "envelope", id, envelope: request.envelope ?? null });
        return false;
      }
      if (type === "argv" || type === "command" || type === "run") {
        const { argv, command } = argvFor(request);
        emit({ type: "argv", id, argv, ...(command !== undefined ? { command } : {}) });
        const child = command !== undefined
          ? spawn(command, { shell: true })
          : spawn(argv[0], argv.slice(1));
        track(id, child);
        return false;
      }
      emit({ type: "error", id, error: `unknown shell request type: ${String(type)}` });
      return false;
    },
    async shutdown(): Promise<void> {
      for (const child of children.values()) child.kill();
      children.clear();
    },
  };
}

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  const protocol = ((opts as unknown as Record<string, unknown>)["--protocol"] ?? "text");
  if (protocol !== "ndjson") {
    throw new CliError(ExitCode.Usage, "gda shell only supports --protocol ndjson in this stage.");
  }
  const session = createShellSession((frame) => {
    process.stdout.write(`${JSON.stringify(frame)}\n`);
  });
  let carry = "";
  for await (const chunk of process.stdin) {
    carry += chunk.toString("utf8");
    const lines = carry.split(/\r?\n/);
    carry = lines.pop() ?? "";
    for (const line of lines) {
      if (await session.handle(line)) {
        await session.shutdown();
        return ExitCode.Success;
      }
    }
  }
  if (carry.trim() !== "" && await session.handle(carry)) {
    await session.shutdown();
    return ExitCode.Success;
  }
  await session.shutdown();
  process.stdout.write(`${JSON.stringify({ type: "shutdown", id: "eof", ok: true })}\n`);
  return ExitCode.Success;
}
