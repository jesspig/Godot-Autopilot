import fs from "node:fs";

import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import type { IpcEndpoint } from "../ipc/client";
import {
  callTool,
  closeBatchIpcSession,
  createBatchIpcSession,
  META_DIRECT_TOOLS,
} from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

interface BatchOperation {
  tool: string;
  args: Record<string, unknown>;
  confirm?: boolean;
}

function readBatchFile(filePath: string): BatchOperation[] {
  let text: string;
  try {
    text = fs.readFileSync(filePath, "utf8");
  } catch {
    throw new CliError(ExitCode.Usage, `cannot read --args-file: ${filePath}`);
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(text);
  } catch {
    throw new CliError(ExitCode.Usage, `--args-file does not contain valid JSON: ${filePath}`);
  }
  const operations =
    Array.isArray(parsed)
      ? parsed
      : parsed !== null && typeof parsed === "object" && !Array.isArray(parsed)
        ? (parsed as { operations?: unknown }).operations
        : undefined;
  if (!Array.isArray(operations)) {
    throw new CliError(
      ExitCode.Usage,
      "batch --args-file must be a JSON array of operations or an object with an operations array.",
    );
  }
  return operations.map((item, index) => {
    if (item === null || typeof item !== "object" || Array.isArray(item)) {
      throw new CliError(ExitCode.Usage, `batch operation ${index} must be an object.`);
    }
    const record = item as { tool?: unknown; args?: unknown; confirm?: unknown };
    if (typeof record.tool !== "string" || record.tool === "") {
      throw new CliError(ExitCode.Usage, `batch operation ${index} is missing a tool name.`);
    }
    if (
      record.args !== undefined &&
      (record.args === null || typeof record.args !== "object" || Array.isArray(record.args))
    ) {
      throw new CliError(ExitCode.Usage, `batch operation ${index} args must be an object.`);
    }
    const operation: BatchOperation = {
      tool: record.tool,
      args: (record.args ?? {}) as Record<string, unknown>,
    };
    if (record.confirm !== undefined) {
      if (typeof record.confirm !== "boolean") {
        throw new CliError(ExitCode.Usage, `batch operation ${index} confirm must be a boolean.`);
      }
      operation.confirm = record.confirm;
    }
    return operation;
  });
}

function resolveIpcEndpoint(opts: GlobalOptions): IpcEndpoint {
  if (opts.socketText !== undefined) {
    return {
      path: opts.socketText,
      token: process.env.GODOT_AUTOPILOT_IPC_TOKEN ?? "",
    };
  }
  const target = resolveTarget(opts, TARGET_CONTEXT);
  if (target.ipcEndpoint === undefined) {
    throw new CliError(
      ExitCode.Transport,
      "batch requires a reachable IPC endpoint; pass --socket or ensure the target instance exposes one.",
      "Run 'gda status --json' to inspect the instance endpoint.",
    );
  }
  return target.ipcEndpoint;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("batch", opts);
  if (positional.length !== 0) {
    throw new CliError(ExitCode.Usage, "batch takes a single --args-file and no positional arguments.");
  }
  if (opts.argsFile === undefined) {
    throw new CliError(ExitCode.Usage, "batch requires --args-file <path>.");
  }
  if (opts.argsJson !== undefined) {
    throw new CliError(ExitCode.Usage, "batch uses --args-file only; --args is not supported.");
  }
  const operations = readBatchFile(opts.argsFile);
  const endpoint = resolveIpcEndpoint(opts);
  const timeoutSec = callTimeoutSec(opts);
  const session = await createBatchIpcSession(endpoint, timeoutSec * 1000);
  try {
    const results: { index: number; tool: string; ok: boolean; display: unknown }[] = [];
    for (let index = 0; index < operations.length; index += 1) {
      const operation = operations[index];
      const args: Record<string, unknown> = { ...operation.args };
      if (operation.confirm === true) {
        args.confirm = true;
      }
      const envelope = await callTool(operation.tool, args, {
        port: 0,
        timeoutSec,
        direct: META_DIRECT_TOOLS.has(operation.tool),
        ipcEndpoint: endpoint,
        transport: "ipc",
        noFallback: true,
        ipcSession: session,
      });
      const extracted = extractPayload(envelope, { raw: opts.raw });
      results.push({
        index,
        tool: operation.tool,
        ok: !extracted.toolFailed,
        display: extracted.display,
      });
    }
    writeJson({ results }, opts);
    return results.every((result) => result.ok) ? ExitCode.Success : ExitCode.ToolFailure;
  } finally {
    closeBatchIpcSession(session);
  }
}
