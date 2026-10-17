import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { callTool } from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("search", opts);
  if (positional.length > 1) {
    throw new CliError(ExitCode.Usage, "search takes at most one [query] argument.");
  }
  const args: Record<string, unknown> = {};
  if (positional.length === 1) args.query = positional[0];
  if (opts.category !== undefined) args.category = opts.category;
  if (opts.tagsText !== undefined) {
    const tags = opts.tagsText
      .split(",")
      .map((s) => s.trim())
      .filter((s) => s.length > 0);
    if (tags.length > 0) args.tags = tags;
  }
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const envelope = await callTool("search_tools", args, { port: target.port, timeoutSec: callTimeoutSec(opts), direct: true });
  const { display, toolFailed } = extractPayload(envelope, {
    saveImages: opts.saveImages,
    raw: opts.raw,
    projectResults: true,
  });
  return emit(display, toolFailed, opts);
}
