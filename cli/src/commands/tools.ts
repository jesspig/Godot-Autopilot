import type { GlobalOptions } from "../args";
import { readCatalog, writeCatalog, type CatalogTool, type ToolCatalog } from "../catalog";
import { CliError, ExitCode } from "../errors";
import { listTools } from "../mcp/client";
import { callTimeoutSec } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

const REFRESH_HINT =
  "run 'gda tools list --refresh' against a live instance, or install the builtin catalog";

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, "tools requires a subcommand: list | search <query> | describe <name>");
  }
  let catalog: ToolCatalog | null = readCatalog();
  if (opts.refresh === true) {
    const target = resolveTarget(opts, TARGET_CONTEXT);
    catalog = writeCatalog(await listTools(target.port, callTimeoutSec(opts)));
  }
  if (catalog === null) {
    throw new CliError(ExitCode.Transport, `no offline tool catalog is cached; ${REFRESH_HINT}`);
  }
  if (sub === "list") {
    writeJson(catalog, opts);
    return ExitCode.Success;
  }
  if (sub === "search") {
    const query = positional[1];
    if (query === undefined) {
      throw new CliError(ExitCode.Usage, "tools search requires a query");
    }
    writeJson({ ...catalog, tools: searchTools(catalog.tools, query) }, opts);
    return ExitCode.Success;
  }
  if (sub === "describe") {
    const name = positional[1];
    if (name === undefined) {
      throw new CliError(ExitCode.Usage, "tools describe requires a tool name");
    }
    const tool = catalog.tools.find((entry) => entry.name === name);
    if (tool === undefined) {
      throw new CliError(
        ExitCode.Usage,
        `tool not found in the cached catalog: ${name} (run 'gda tools list' or pass --refresh)`,
      );
    }
    writeJson(tool, opts);
    return ExitCode.Success;
  }
  throw new CliError(ExitCode.Usage, `unknown tools subcommand: ${sub} (expected list, search or describe)`);
}

function searchTools(tools: CatalogTool[], query: string): CatalogTool[] {
  const needle = query.trim().toLowerCase();
  if (needle === "") return tools;
  return tools.filter((tool) => haystack(tool).includes(needle));
}

function haystack(tool: CatalogTool): string {
  const parts = [tool.name];
  if (typeof tool.description === "string") parts.push(tool.description);
  if (typeof tool.category === "string") parts.push(tool.category);
  if (Array.isArray(tool.tags)) parts.push(tool.tags.map((tag) => String(tag)).join(" "));
  return parts.join(" ").toLowerCase();
}