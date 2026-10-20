import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { callTool, META_DIRECT_TOOLS } from "../mcp/client";
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
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const envelope = await callTool(tool, args, {
    port: target.port,
    timeoutSec: callTimeoutSec(opts),
    direct: META_DIRECT_TOOLS.has(tool),
  });
  const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  return emit(extracted, opts);
}
