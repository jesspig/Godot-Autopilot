import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { callTool } from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("describe", opts);
  if (positional.length !== 1) {
    throw new CliError(ExitCode.Usage, "describe requires exactly one <tool> argument.");
  }
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const envelope = await callTool(
    "get_tool_detail",
    { name: positional[0] },
    { port: target.port, timeoutSec: callTimeoutSec(opts), direct: true },
  );
  const { display, toolFailed } = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  return emit(display, toolFailed, opts);
}
