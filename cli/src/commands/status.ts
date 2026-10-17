import type { GlobalOptions } from "../args";
import { callTool } from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("status", opts);
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const envelope = await callTool("system_status", {}, { port: target.port, timeoutSec: callTimeoutSec(opts) });
  const { display, toolFailed } = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  return emit(display, toolFailed, opts);
}
