import fs from "node:fs";

import builtinCatalog from "../assets/catalog.json";

import { gdaRoot, joinPath } from "./platform";
import { CLI_VERSION } from "./version";

export interface CatalogTool {
  name: string;
  category?: unknown;
  tags?: unknown;
  side_effect?: unknown;
  description?: unknown;
  input_schema?: unknown;
}

export interface ToolCatalog {
  generated_at: string;
  version: string;
  tools: CatalogTool[];
}

const SAFE_FIELDS = ["category", "tags", "side_effect", "description", "input_schema"] as const;

export function catalogPath(): string {
  return joinPath(gdaRoot(), "cache", "catalog", `${CLI_VERSION}.json`);
}

function sanitizeTool(item: unknown): CatalogTool | null {
  if (typeof item !== "object" || item === null || Array.isArray(item)) return null;
  const doc = item as Record<string, unknown>;
  if (typeof doc.name !== "string" || doc.name === "") return null;
  const tool: CatalogTool = { name: doc.name };
  for (const field of SAFE_FIELDS) {
    if (doc[field] !== undefined) {
      (tool as unknown as Record<string, unknown>)[field] = doc[field];
    }
  }
  return tool;
}

export function writeCatalog(items: unknown[]): ToolCatalog {
  const tools: CatalogTool[] = [];
  for (const item of items) {
    const sanitized = sanitizeTool(item);
    if (sanitized !== null) tools.push(sanitized);
  }
  const catalog: ToolCatalog = {
    generated_at: new Date().toISOString(),
    version: CLI_VERSION,
    tools,
  };
  fs.mkdirSync(joinPath(gdaRoot(), "cache", "catalog"), { recursive: true });
  fs.writeFileSync(catalogPath(), `${JSON.stringify(catalog, null, 2)}\n`, "utf8");
  return catalog;
}

export function builtinToolCatalog(): ToolCatalog {
  return {
    generated_at: "builtin",
    version: CLI_VERSION,
    tools: builtinCatalog.tools as CatalogTool[],
  };
}

export function readCatalog(): ToolCatalog | null {
  try {
    const parsed = JSON.parse(fs.readFileSync(catalogPath(), "utf8")) as ToolCatalog;
    if (Array.isArray(parsed.tools) && parsed.tools.length > 0) {
      return parsed;
    }
  } catch {
  }
  return builtinToolCatalog();
}