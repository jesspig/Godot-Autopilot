import type { GlobalOptions } from "../args";
import {
  catalogCategories,
  filterCatalogEntries,
  filterProtocolToolNames,
  hasOfflineCatalog,
  isProtocolTool,
  nativeCatalogList,
  readOfflineCatalog,
  writeOfflineCatalog,
  type NativeCatalogEntry,
} from "../discovery";
import { CliError, ExitCode } from "../errors";
import { findById, findLiveByProject } from "../registry";
import { resolveNativeTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

interface LoadedCatalog {
  tools: NativeCatalogEntry[];
  source: "live" | "offline";
  hash: string;
}

function tagsFrom(opts: GlobalOptions): string[] {
  if (opts.tagsText === undefined) return [];
  return opts.tagsText.split(",").map((tag) => tag.trim()).filter((tag) => tag.length > 0);
}

function nativeIdentity(target: ReturnType<typeof resolveNativeTarget>): string {
  return target.instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "";
}

async function loadCatalog(opts: GlobalOptions): Promise<LoadedCatalog> {
  if (opts.offline === true) {
    const doc = readOfflineCatalog();
    return { tools: doc.tools, source: "offline", hash: doc.hash };
  }
  try {
    const target = resolveNativeTarget(opts, TARGET_CONTEXT);
    if (target.ipcEndpoint === undefined) {
      throw new CliError(
        ExitCode.Transport,
        "no native IPC endpoint is available; pass --socket or --id from 'gda editor list'.",
      );
    }
    const tools = await nativeCatalogList(target.ipcEndpoint, nativeIdentity(target));
    const doc = writeOfflineCatalog(tools);
    return { tools, source: "live", hash: doc.hash };
  } catch (error) {
    if (hasOfflineCatalog()) {
      const doc = readOfflineCatalog();
      return { tools: doc.tools, source: "offline", hash: doc.hash };
    }
    throw error;
  }
}

export async function toolList(opts: GlobalOptions): Promise<number> {
  const catalog = await loadCatalog(opts);
  const entries = filterCatalogEntries(catalog.tools, { category: opts.category, tags: tagsFrom(opts) });
  const names = filterProtocolToolNames(entries.map((entry) => entry.name));
  writeJson({ tools: names, total: names.length, source: catalog.source, hash: catalog.hash }, opts);
  return ExitCode.Success;
}

export async function toolSearch(positional: string[], opts: GlobalOptions): Promise<number> {
  if (positional.length > 1) {
    throw new CliError(ExitCode.Usage, "tool search takes at most one [query] argument.");
  }
  const catalog = await loadCatalog(opts);
  const text = positional[0];
  if (text === undefined) {
    writeJson({ categories: catalogCategories(catalog.tools), source: catalog.source, hash: catalog.hash }, opts);
    return ExitCode.Success;
  }
  const entries = filterCatalogEntries(catalog.tools, {
    text,
    category: opts.category,
    tags: tagsFrom(opts),
    limit: 64,
  });
  const results = entries
    .filter((entry) => !isProtocolTool(entry.name))
    .map((entry, index) => ({
      name: entry.name,
      score: entries.length - index,
      description: entry.description,
      category: entry.category,
    }));
  writeJson({ results, source: catalog.source, hash: catalog.hash }, opts);
  return ExitCode.Success;
}

export async function toolDescribe(positional: string[], opts: GlobalOptions): Promise<number> {
  const name = positional[0];
  if (name === undefined || positional.length !== 1) {
    throw new CliError(ExitCode.Usage, "tool describe requires exactly one <tool> argument.");
  }
  const catalog = await loadCatalog(opts);
  const found = catalog.tools.find((entry) => entry.name === name || entry.aliases.includes(name));
  if (found === undefined) {
    writeJson({ found: false, name, source: catalog.source, hash: catalog.hash }, opts);
    return ExitCode.Success;
  }
  writeJson({ found: true, ...found, source: catalog.source, hash: catalog.hash }, opts);
  return ExitCode.Success;
}

export async function toolCategories(opts: GlobalOptions): Promise<number> {
  const catalog = await loadCatalog(opts);
  writeJson({ categories: catalogCategories(catalog.tools), source: catalog.source, hash: catalog.hash }, opts);
  return ExitCode.Success;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, "tool requires a subcommand: list/search/describe/categories.");
  }
  const rest = positional.slice(1);
  switch (sub) {
    case "list":
      return toolList(opts);
    case "search":
      return toolSearch(rest, opts);
    case "describe":
      return toolDescribe(rest, opts);
    case "categories":
      return toolCategories(opts);
    case "call":
    case "batch":
      throw new CliError(
        ExitCode.Usage,
        `gda tool ${sub} is not available in this stage; use the legacy 'gda ${sub}' command.`,
      );
    default:
      throw new CliError(ExitCode.Usage, `unknown tool subcommand: ${sub} (expected list/search/describe/categories)`);
  }
}
