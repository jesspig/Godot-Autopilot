import fs from "node:fs";

import type { GlobalOptions } from "../args";
import { toolRisk } from "../command-spec";
import { CliError, ExitCode, isIpcRequestSent } from "../errors";
import type { IpcEndpoint } from "../ipc/client";
import {
  callTool,
  closeBatchIpcSession,
  createBatchIpcSession,
  META_DIRECT_TOOLS,
} from "../rpc/client";
import { normalizeEnvelope, recordConfirmation, writeEnvelope, batchStatus } from "../output";
import type { ConfirmationSummary } from "../output";
import { assertCallFamilyUsage, callTimeoutSec, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

export function batchFailureEntry(index: number, tool: string, error: string): {
  index: number;
  tool: string;
  ok: boolean;
  display: unknown;
  error?: string;
} {
  return {
    index,
    tool,
    ok: false,
    display: { error },
    error,
  };
}

export function batchExitCode(
  results: readonly { ok: boolean }[],
  errorCodes: readonly number[] = [],
): number {
  const priority = [
    ExitCode.Usage,
    ExitCode.Transport,
    ExitCode.Interrupted,
    ExitCode.UserDeclined,
    ExitCode.ConfirmationRequired,
  ];
  for (const code of priority) {
    if (errorCodes.includes(code)) return code;
  }
  return results.every((result) => result.ok) && !errorCodes.includes(ExitCode.ToolFailure)
    ? ExitCode.Success
    : ExitCode.ToolFailure;
}

interface BatchOperation {
  tool: string;
  args: Record<string, unknown>;
  confirm?: boolean;
}

function readBatchFile(filePath: string): BatchOperation[] {
  let text: string;
  try {
    text = filePath === "-" ? fs.readFileSync(0, "utf8") : fs.readFileSync(filePath, "utf8");
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
    const token = process.env.GODOT_AUTOPILOT_IPC_TOKEN ?? "";
    if (opts.socketText.startsWith("tcp:")) {
      const address = opts.socketText.slice(4);
      const separator = address.lastIndexOf(":");
      if (separator > 0) {
        const port = Number(address.slice(separator + 1));
        if (Number.isInteger(port) && port > 0 && port <= 65535) {
          return {
            kind: "tcp",
            host: address.slice(0, separator),
            port,
            token,
          };
        }
      }
      throw new CliError(
        ExitCode.Usage,
        `invalid tcp IPC endpoint: ${opts.socketText}.`,
        "Use tcp:<host>:<port>.",
      );
    }
    return { path: opts.socketText, token };
  }
  const target = resolveTarget(opts, TARGET_CONTEXT);
  if (target.ipcEndpoint === undefined) {
    throw new CliError(
      ExitCode.Transport,
      "batch requires a reachable IPC endpoint; pass --socket or ensure the target instance exposes one.",
      "Run 'gda editor status' to inspect the instance endpoint.",
    );
  }
  return target.ipcEndpoint;
}

function confirmationSummary(operation: BatchOperation, endpoint: IpcEndpoint, index: number): ConfirmationSummary {
  return {
    command: `tool.batch[${index}]`,
    tool: operation.tool,
    target: endpoint.path !== undefined && endpoint.path !== "" ? `socket:${endpoint.path}` : `port:${endpoint.port}`,
    risk: toolRisk(operation.tool),
    args: { ...operation.args },
    items: index + 1,
  };
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("batch", opts);
  if (positional.length !== 0) {
    throw new CliError(ExitCode.Usage, "batch takes a single --args-file and no positional arguments.");
  }
  if (opts.argsFile === undefined) {
    throw new CliError(ExitCode.Usage, "batch requires --args-file <path> (use - for stdin).");
  }
  if (opts.argsJson !== undefined) {
    throw new CliError(ExitCode.Usage, "batch uses --args-file only; --args is not supported.");
  }
  const operations = readBatchFile(opts.argsFile);
  const endpoint = resolveIpcEndpoint(opts);
  const timeoutSec = callTimeoutSec(opts);
  const session = await createBatchIpcSession(endpoint, timeoutSec * 1000);
  try {
    const results: { index: number; tool: string; ok: boolean; display: unknown; error?: string }[] = [];
    const errorCodes: number[] = [];
    for (let index = 0; index < operations.length; index += 1) {
      const operation = operations[index];
      const args: Record<string, unknown> = { ...operation.args };
      const risk = toolRisk(operation.tool);
      const summary = confirmationSummary(operation, endpoint, index);
      if (risk === "high") {
        if (operation.confirm !== true) {
          results.push(batchFailureEntry(
            index,
            operation.tool,
            `CONFIRMATION_REQUIRED: each high-risk batch item needs confirm=true (${summary.command}).`,
          ));
          errorCodes.push(ExitCode.ConfirmationRequired);
          continue;
        }
        recordConfirmation(summary);
      }
      if (operation.confirm === true) {
        args.confirm = true;
      }
      try {
        const envelope = await callTool(operation.tool, args, {
          port: 0,
          timeoutSec,
          direct: META_DIRECT_TOOLS.has(operation.tool),
          ipcEndpoint: endpoint,
          ipcSession: session,
        });
        const extracted = extractPayload(envelope, { raw: opts.raw });
        results.push({
          index,
          tool: operation.tool,
          ok: !extracted.toolFailed,
          display: extracted.display,
        });
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        const code = error instanceof CliError ? error.code : ExitCode.ToolFailure;
        errorCodes.push(code);
        results.push(batchFailureEntry(index, operation.tool, message));
        if (isIpcRequestSent(error) || code === ExitCode.Transport) {
          break;
        }
      }
    }
    const status = batchStatus(results);
    const envelope = normalizeEnvelope(
      {
        schema_version: 1,
        ok: status.failed === 0,
        data: {
          results,
          status: status.partial ? "partial" : "complete",
          partial: status.partial,
        },
      },
      "tool.batch",
      opts,
      status.failed === 0,
    );
    envelope.meta = { ...envelope.meta, ...status };
    writeEnvelope(envelope, opts);
    return batchExitCode(results, errorCodes);
  } finally {
    closeBatchIpcSession(session);
  }
}
