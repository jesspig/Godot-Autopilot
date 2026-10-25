import path from "node:path";

import type { GlobalOptions } from "../args";
import {
  apiRequest,
  API_TOOLS_SEGMENT,
  extractPayloadError,
  extractServerError,
  mapStatusToExitCode,
  statusAdvice,
  type ApiRequestOptions,
  type ApiResponse,
} from "../api/client";
import { CliError, ExitCode } from "../errors";
import {
  deriveEndpoint,
  endpointBaseUrl,
  findPluginRecordByPort,
  findPluginRecordByProject,
  pluginIndexPath,
  type EndpointEntry,
  type PluginInstanceRecord,
} from "../instances";
import { err, renderJson } from "../output";
import { callTimeoutSec, loadToolArguments } from "../payload";
import { parseProbeLevel, probeEndpoint, type ProbeLevel } from "../probe";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type ResolvedTarget, type TargetContext } from "../target";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

interface ApiContext {
  endpoint: EndpointEntry;
  token: string | undefined;
  target: ResolvedTarget;
  record: PluginInstanceRecord | undefined;
}

interface ToolFilter {
  query?: string;
  category?: string;
  tags?: string[];
  limit?: number;
  offset?: number;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, "api requires a subcommand: list | describe | call.");
  }
  const rest = positional.slice(1);
  if (sub === "list") {
    assertSubcommandOptions("list", opts);
    return await runList(rest, opts);
  }
  if (sub === "describe") {
    assertSubcommandOptions("describe", opts);
    return await runDescribe(rest, opts);
  }
  if (sub === "call") {
    assertSubcommandOptions("call", opts);
    return await runCall(rest, opts);
  }
  throw new CliError(ExitCode.Usage, `unknown api subcommand: ${sub} (list | describe | call).`);
}

async function runList(positional: string[], opts: GlobalOptions): Promise<number> {
  if (positional.length > 1) {
    throw new CliError(ExitCode.Usage, "api list takes at most one [query] argument.");
  }
  const query = resolveQuery(positional, opts);
  const tags = parseTags(opts.tagsText);
  const filter: ToolFilter = {
    query,
    category: opts.category,
    tags,
    limit: parsePageNumber("--limit", opts.limitText, 1, 2000),
    offset: parsePageNumber("--offset", opts.offsetText, 0, 2147483647),
  };
  const context = await prepareApiContext(opts);
  const suffix = `${API_TOOLS_SEGMENT}${buildQueryString(filter)}`;
  const response = await apiRequest("GET", suffix, undefined, requestOptions(context, opts));
  return emitResponse(response, opts, applyFilter(response.body, filter));
}

async function runDescribe(positional: string[], opts: GlobalOptions): Promise<number> {
  if (positional.length !== 1) {
    throw new CliError(ExitCode.Usage, "api describe requires exactly one <tool> argument.");
  }
  const context = await prepareApiContext(opts);
  const suffix = `${API_TOOLS_SEGMENT}/${encodeURIComponent(positional[0])}`;
  const response = await apiRequest("GET", suffix, undefined, requestOptions(context, opts));
  return emitResponse(response, opts, response.body);
}

async function runCall(positional: string[], opts: GlobalOptions): Promise<number> {
  if (positional.length !== 1) {
    throw new CliError(ExitCode.Usage, "api call requires exactly one <tool> argument.");
  }
  const args = loadToolArguments(opts);
  const context = await prepareApiContext(opts);
  const suffix = `${API_TOOLS_SEGMENT}/${encodeURIComponent(positional[0])}`;
  const response = await apiRequest("POST", suffix, args, requestOptions(context, opts));
  return emitResponse(response, opts, response.body);
}

function assertSubcommandOptions(sub: string, opts: GlobalOptions): void {
  if (sub !== "list" && opts.category !== undefined) {
    throw new CliError(ExitCode.Usage, "--category only applies to the api list subcommand.");
  }
  if (sub !== "list" && opts.query !== undefined) {
    throw new CliError(ExitCode.Usage, "--query only applies to the api list subcommand.");
  }
  if (sub !== "list" && opts.tagsText !== undefined) {
    throw new CliError(ExitCode.Usage, "--tags only applies to the api list subcommand.");
  }
  if (sub !== "list" && opts.limitText !== undefined) {
    throw new CliError(ExitCode.Usage, "--limit only applies to the api list subcommand.");
  }
  if (sub !== "list" && opts.offsetText !== undefined) {
    throw new CliError(ExitCode.Usage, "--offset only applies to the api list subcommand.");
  }
  if (sub !== "call" && (opts.argsJson !== undefined || opts.argsFile !== undefined)) {
    throw new CliError(ExitCode.Usage, "--args and --args-file only apply to the api call subcommand.");
  }
}

function resolveQuery(positional: string[], opts: GlobalOptions): string | undefined {
  const fromArgs = positional.length === 1 ? positional[0] : undefined;
  if (fromArgs !== undefined && opts.query !== undefined) {
    throw new CliError(ExitCode.Usage, "use only one of the api list query argument and --query.");
  }
  const value = opts.query ?? fromArgs;
  if (value === undefined) return undefined;
  const trimmed = value.trim();
  return trimmed === "" ? undefined : trimmed;
}

function parseTags(text: string | undefined): string[] | undefined {
  if (text === undefined) return undefined;
  const tags = text
    .split(",")
    .map((s) => s.trim())
    .filter((s) => s.length > 0);
  return tags.length === 0 ? undefined : tags;
}

function buildQueryString(filter: ToolFilter): string {
  const parts: string[] = [];
  if (filter.query !== undefined) parts.push(`query=${encodeURIComponent(filter.query)}`);
  if (filter.category !== undefined) parts.push(`category=${encodeURIComponent(filter.category)}`);
  if (filter.tags !== undefined) parts.push(`tags=${encodeURIComponent(filter.tags.join(","))}`);
  if (filter.limit !== undefined) parts.push(`limit=${filter.limit}`);
  if (filter.offset !== undefined) parts.push(`offset=${filter.offset}`);
  return parts.length === 0 ? "" : `?${parts.join("&")}`;
}

function parsePageNumber(
  flag: string,
  text: string | undefined,
  min: number,
  max: number,
): number | undefined {
  if (text === undefined) return undefined;
  const trimmed = text.trim();
  if (!/^\d+$/.test(trimmed)) {
    throw new CliError(ExitCode.Usage, `${flag} must be an integer between ${min} and ${max}.`);
  }
  const value = Number(trimmed);
  if (!Number.isSafeInteger(value) || value < min || value > max) {
    throw new CliError(ExitCode.Usage, `${flag} must be an integer between ${min} and ${max}.`);
  }
  return value;
}

async function prepareApiContext(opts: GlobalOptions): Promise<ApiContext> {
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const record =
    target.pluginRecord ??
    findPluginRecordByPort(target.port) ??
    findPluginRecordByProject(opts.project !== undefined ? path.resolve(opts.project) : process.cwd());
  const endpoint = record?.apiEndpoint ?? deriveEndpoint(target.port, "api");
  const level = parseProbeLevel(opts.probe);
  await ensureReachable({ endpoint, token: undefined, target, record }, level);
  const token = record !== undefined && record.apiToken !== "" ? record.apiToken : undefined;
  if (token === undefined && endpoint.auth !== "none") {
    throw new CliError(
      ExitCode.Transport,
      `no API token is available for port ${target.port} (source ${target.source}).`,
      `Fix the port in the plugin control panel, then read api_token from the instance registry at ${pluginIndexPath()}.`,
    );
  }
  return { endpoint, token, target, record };
}

async function ensureReachable(context: ApiContext, level: ProbeLevel): Promise<void> {
  const url = endpointBaseUrl(context.endpoint);
  const alive = await probeEndpoint(
    { host: context.endpoint.host, port: context.endpoint.port, url },
    level,
  );
  if (alive) return;
  throw new CliError(
    ExitCode.Transport,
    `the REST endpoint is not reachable at ${url} (port ${context.target.port}, source ${context.target.source}, chain ${context.target.chain}).`,
    discoverySuggestion(context.target),
  );
}

function discoverySuggestion(target: ResolvedTarget): string {
  if (target.chain === "default") {
    return `No instance was discovered (tried: env, cli-registry, plugin-index, legacy, default ${target.port}); pass --port or GODOT_AUTOPILOT_PORT.`;
  }
  return "Open the editor with the plugin enabled, or pass --port with the fixed port of the instance.";
}

function requestOptions(context: ApiContext, opts: GlobalOptions): ApiRequestOptions {
  return {
    endpoint: context.endpoint,
    token: context.token,
    timeoutSec: callTimeoutSec(opts),
    origin: `port ${context.target.port}, source ${context.target.source}`,
  };
}

function emitResponse(response: ApiResponse, opts: GlobalOptions, payload: unknown): number {
  const mapped = mapStatusToExitCode(response.status);
  if (mapped !== ExitCode.Success) {
    const detail = extractServerError(response.body, response.text);
    throw new CliError(
      mapped,
      `HTTP ${response.status}${detail === null ? "" : `: ${detail}`}`,
      statusAdvice(response.status),
    );
  }
  const failure = extractPayloadError(payload);
  process.stdout.write(`${renderJson(payload, opts)}\n`);
  if (failure !== null) {
    err(`tool failure: ${failure}`);
    return ExitCode.ToolFailure;
  }
  return ExitCode.Success;
}

function applyFilter(body: unknown, filter: ToolFilter): unknown {
  if (filter.query === undefined && filter.category === undefined && filter.tags === undefined) return body;
  const located = locateToolArray(body);
  if (located === null) return body;
  const kept = located.items.filter((item) => matchesFilter(item, filter));
  if (located.key === null) return kept;
  return { ...(body as Record<string, unknown>), [located.key]: kept };
}

function locateToolArray(body: unknown): { key: string | null; items: unknown[] } | null {
  if (Array.isArray(body)) return { key: null, items: body };
  if (typeof body !== "object" || body === null) return null;
  const doc = body as Record<string, unknown>;
  for (const key of ["tools", "results", "items", "data"]) {
    const value = doc[key];
    if (Array.isArray(value)) return { key, items: value };
  }
  return null;
}

function matchesFilter(item: unknown, filter: ToolFilter): boolean {
  if (typeof item !== "object" || item === null || Array.isArray(item)) return true;
  const doc = item as Record<string, unknown>;
  if (filter.category !== undefined) {
    const categories = collectStrings(doc, ["category", "categories", "group"]);
    if (categories.length > 0 && !categories.some((c) => c.toLowerCase() === filter.category!.toLowerCase())) {
      return false;
    }
  }
  if (filter.tags !== undefined) {
    const tags = collectStrings(doc, ["tags", "tag"]);
    if (tags.length > 0 && !filter.tags.every((t) => tags.some((tag) => tag.toLowerCase() === t.toLowerCase()))) {
      return false;
    }
  }
  if (filter.query !== undefined) {
    const haystack = collectStrings(doc, ["name", "description", "title", "summary", "category", "categories", "tags", "tag", "keywords"]);
    const needle = filter.query.toLowerCase();
    if (haystack.length > 0 && !haystack.some((value) => value.toLowerCase().includes(needle))) {
      return false;
    }
  }
  return true;
}

function collectStrings(doc: Record<string, unknown>, keys: string[]): string[] {
  const values: string[] = [];
  for (const key of keys) {
    const value = doc[key];
    if (typeof value === "string" && value !== "") values.push(value);
    if (Array.isArray(value)) {
      for (const item of value) {
        if (typeof item === "string" && item !== "") values.push(item);
      }
    }
  }
  return values;
}
