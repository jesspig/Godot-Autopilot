import type { GlobalOptions } from "../args";
import { filterProtocolToolNames } from "../discovery";
import { CliError, ExitCode } from "../errors";
import { callTool } from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

function tagsFrom(opts: GlobalOptions): string[] {
  if (opts.tagsText === undefined) return [];
  return opts.tagsText
    .split(",")
    .map((tag) => tag.trim())
    .filter((tag) => tag.length > 0);
}

function toolNames(payload: unknown): string[] {
  if (payload === null || typeof payload !== "object" || Array.isArray(payload)) return [];
  const results = (payload as { results?: unknown }).results;
  if (!Array.isArray(results)) return [];
  const names: string[] = [];
  for (const item of results) {
    if (item === null || typeof item !== "object" || Array.isArray(item)) continue;
    const name = (item as { name?: unknown }).name;
    if (typeof name === "string" && name !== "") names.push(name);
  }
  return names;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("tools", opts);
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, "tools requires a subcommand: list");
  }
  if (sub !== "list") {
    throw new CliError(ExitCode.Usage, `unknown tools subcommand: ${sub} (expected list)`);
  }
  if (positional.length > 1) {
    throw new CliError(ExitCode.Usage, "tools list takes no positional arguments.");
  }

  const args: Record<string, unknown> = { limit: 0 };
  if (opts.category !== undefined) args.category = opts.category;
  const tags = tagsFrom(opts);
  if (tags.length > 0) args.tags = tags;

  const target = resolveTarget(opts, TARGET_CONTEXT);
  const envelope = await callTool("search_tools", args, {
    port: target.port,
    timeoutSec: callTimeoutSec(opts),
    direct: true,
  });
  const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  if (extracted.toolFailed) return emit(extracted, opts);

  const tools = filterProtocolToolNames(toolNames(extracted.display));
  writeJson({ tools, total: tools.length }, opts);
  return ExitCode.Success;
}
