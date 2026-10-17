import type { GlobalOptions } from "../args";
import { callTool } from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("categories", opts);
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const envelope = await callTool("list_categories", {}, { port: target.port, timeoutSec: callTimeoutSec(opts), direct: true });
  const { display, toolFailed } = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  return emit(display, toolFailed, opts);
}
