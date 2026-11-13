import type { GlobalOptions } from "../args";
import { filterProtocolTools } from "../discovery";
import { CliError, ExitCode } from "../errors";
import { callTool } from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

function searchResults(payload: unknown): Array<{ name: string; score: number }> {
  if (payload === null || typeof payload !== "object" || Array.isArray(payload)) return [];
  const results = (payload as { results?: unknown }).results;
  if (!Array.isArray(results)) return [];
  const entries: Array<{ name: string; score: number }> = [];
  for (const item of results) {
    if (item === null || typeof item !== "object" || Array.isArray(item)) continue;
    const record = item as { name?: unknown; score?: unknown };
    if (typeof record.name !== "string" || record.name === "") continue;
    entries.push({ name: record.name, score: typeof record.score === "number" ? record.score : 0 });
  }
  return filterProtocolTools(entries);
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("search", opts);
  if (positional.length > 1) {
    throw new CliError(ExitCode.Usage, "search takes at most one [query] argument.");
  }
  const target = resolveTarget(opts, TARGET_CONTEXT);
  if (positional.length === 0) {
    const categories = await callTool("list_categories", {}, {
      port: target.port,
      timeoutSec: callTimeoutSec(opts),
      direct: true,
    });
    const extracted = extractPayload(categories, { saveImages: opts.saveImages, raw: opts.raw });
    if (extracted.toolFailed) return emit(extracted, opts);
    writeJson(extracted.display, opts);
    return ExitCode.Success;
  }

  const args: Record<string, unknown> = { query: positional[0] };
  if (opts.category !== undefined) args.category = opts.category;
  if (opts.tagsText !== undefined) {
    const tags = opts.tagsText
      .split(",")
      .map((tag) => tag.trim())
      .filter((tag) => tag.length > 0);
    if (tags.length > 0) args.tags = tags;
  }
  const envelope = await callTool("search_tools", args, {
    port: target.port,
    timeoutSec: callTimeoutSec(opts),
    direct: true,
  });
  const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  if (extracted.toolFailed) return emit(extracted, opts);
  writeJson({ results: searchResults(extracted.display) }, opts);
  return ExitCode.Success;
}
