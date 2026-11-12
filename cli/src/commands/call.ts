import type { GlobalOptions } from "../args";
import { parseTransport } from "../args";
import { CliError, ExitCode } from "../errors";
import { callTool, META_DIRECT_TOOLS } from "../mcp/client";
import { err } from "../output";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload, loadToolArguments } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("call", opts);
  if (positional.length !== 1) {
    throw new CliError(ExitCode.Usage, "call requires exactly one <tool> argument.");
  }
  const tool = positional[0];
  const args = loadToolArguments(opts);
  if (opts.confirm === true) {
    args.confirm = true;
  }
  const timeoutSec = callTimeoutSec(opts);
  const transport = parseTransport(opts.transportText);
  if (opts.socketText !== undefined) {
    try {
      const envelope = await callTool(tool, args, {
        port: 0,
        timeoutSec,
        direct: META_DIRECT_TOOLS.has(tool),
        ipcEndpoint: {
          path: opts.socketText,
          token: process.env.GODOT_AUTOPILOT_IPC_TOKEN ?? "",
        },
        transport: "ipc",
        noFallback: opts.noFallback,
      });
      const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
      return emit(extracted, opts);
    } catch (error) {
      if (!(error instanceof CliError) || error.code !== ExitCode.Transport || opts.noFallback === true) {
        throw error;
      }
      err(`IPC channel unavailable (${error.message}); falling back to the port channel.`);
    }
  }
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const envelope = await callTool(tool, args, {
    port: target.port,
    timeoutSec,
    direct: META_DIRECT_TOOLS.has(tool),
    ipcEndpoint: target.ipcEndpoint,
    transport,
    noFallback: opts.noFallback,
  });
  const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  return emit(extracted, opts);
}
