import path from "node:path";

import type { GlobalOptions } from "../args";
import {
  cacheStatus,
  ensureDocs,
  exportDocs,
  loadExternalDocs,
  readProjectPin,
  resolveEngine,
  type EngineResolution,
  type LoadedDocs,
} from "../docs/engine";
import { getClassView, parseKind, parseSection, resolveMember, searchDocs } from "../docs/search";
import { clearBuckets, listBuckets } from "../docs/store";
import { CliError, errorMessage, ExitCode } from "../errors";
import { resolveGodotBin } from "../process";
import { findLiveByProject } from "../registry";
import { writeJson } from "./json-output";

function parseLimit(text: string | undefined): number {
  if (text === undefined) return 20;
  const value = text.trim();
  if (!/^\d+$/.test(value)) {
    throw new CliError(ExitCode.Usage, `--limit must be an integer in 1-200, got: ${text}`);
  }
  const limit = Number(value);
  if (limit < 1 || limit > 200) {
    throw new CliError(ExitCode.Usage, `--limit must be an integer in 1-200, got: ${text}`);
  }
  return limit;
}

function requireArgument(positional: string[], index: number, usage: string): string {
  const value = positional[index];
  if (value === undefined || value.trim() === "") {
    throw new CliError(ExitCode.Usage, usage);
  }
  return value;
}

function loadDocs(opts: GlobalOptions): LoadedDocs {
  if (opts.docsFile !== undefined && opts.docsFile.trim() !== "") {
    return loadExternalDocs(opts.docsFile);
  }
  return ensureDocs(resolveEngine({ godot: opts.godot, project: opts.project }));
}

function runSync(opts: GlobalOptions): number {
  const engine = resolveEngine({ godot: opts.godot, project: opts.project });
  const bin = engine.binary.trim() !== ""
    ? engine.binary
    : resolveGodotBin({ explicit: opts.godot }).bin;
  const exported = exportDocs({
    bin,
    key: engine.key,
    sourceBinary: bin,
    expected: engine.version,
  });
  writeJson(
    {
      ok: true,
      engine,
      cache: {
        status: "synced",
        path: exported.apiPath,
        synced_at: exported.meta.synced_at,
        generated_by: exported.index.header.version_full_name ?? "",
        bytes: exported.meta.bytes,
      },
      bytes: exported.meta.bytes,
    },
    opts,
  );
  return ExitCode.Success;
}

function runStatus(opts: GlobalOptions): number {
  const projectPath = path.resolve(opts.project ?? process.cwd());
  const pin = readProjectPin(projectPath);
  let resolved: EngineResolution | null = null;
  let resolutionError: string | null = null;
  try {
    resolved = resolveEngine({ godot: opts.godot, project: projectPath });
  } catch (error) {
    resolutionError = errorMessage(error);
  }
  const cache = resolved === null ? null : cacheStatus(resolved);
  const buckets = listBuckets().map((bucket) => ({
    key: bucket.key,
    engine: bucket.meta.engine,
    source_binary: bucket.meta.source_binary,
    synced_at: bucket.meta.synced_at,
    bytes: bucket.meta.bytes,
  }));
  writeJson(
    {
      project: {
        path: projectPath,
        pinned: pin === undefined ? null : `${pin.major}.${pin.minor}`,
      },
      live: findLiveByProject(projectPath) !== undefined,
      resolved,
      resolution_error: resolutionError,
      cache,
      buckets,
    },
    opts,
  );
  return ExitCode.Success;
}

function runSearch(positional: string[], opts: GlobalOptions): number {
  const query = requireArgument(positional, 1, "docs search requires a query");
  const docs = loadDocs(opts);
  const result = searchDocs(docs.index, query, {
    limit: parseLimit(opts.limitText),
    kind: parseKind(opts.kind),
    className: opts.classFilter,
  });
  writeJson(
    {
      engine: docs.engine,
      cache: docs.cache,
      results: result.results,
      total: result.total,
      ...(result.total === 0
        ? { suggestion: "Run 'gda docs class <name>' for a known class, or widen the query." }
        : {}),
    },
    opts,
  );
  return result.total === 0 ? ExitCode.ToolFailure : ExitCode.Success;
}

function runClass(positional: string[], opts: GlobalOptions): number {
  const name = requireArgument(positional, 1, "docs class requires a class name");
  const docs = loadDocs(opts);
  const view = getClassView(docs.index, name, {
    section: parseSection(opts.section),
    member: opts.member,
    full: opts.full === true,
  });
  writeJson({ engine: docs.engine, cache: docs.cache, class: view }, opts);
  return ExitCode.Success;
}

function runMember(positional: string[], opts: GlobalOptions): number {
  const className = requireArgument(positional, 1, "docs member requires a class name");
  const memberName = requireArgument(positional, 2, "docs member requires a member name");
  const docs = loadDocs(opts);
  const matches = resolveMember(docs.index, className, memberName, parseKind(opts.kind));
  writeJson(
    {
      engine: docs.engine,
      cache: docs.cache,
      matches,
      total: matches.length,
      ...(matches.length === 0
        ? { suggestion: "Run 'gda docs class <name> --member <text>' to list near matches." }
        : {}),
    },
    opts,
  );
  return matches.length === 0 ? ExitCode.ToolFailure : ExitCode.Success;
}

function runClear(opts: GlobalOptions): number {
  const engineKey = opts.engine?.trim();
  if (engineKey !== undefined && opts.all === true) {
    throw new CliError(ExitCode.Usage, "docs clear accepts either --engine <x.y> or --all, not both");
  }
  if (opts.all === true && opts.force !== true) {
    throw new CliError(ExitCode.Usage, "docs clear --all requires --force");
  }
  let keys: string[];
  if (engineKey !== undefined && engineKey !== "") {
    if (!/^\d+\.\d+(-[a-z0-9]+)?$/.test(engineKey)) {
      throw new CliError(
        ExitCode.Usage,
        `--engine must be x.y or x.y-status (for example 4.7 or 4.8-dev), got: ${engineKey}`,
      );
    }
    keys = [engineKey];
  } else if (opts.all === true) {
    keys = listBuckets().map((bucket) => bucket.key);
  } else {
    throw new CliError(ExitCode.Usage, "docs clear requires --engine <x.y> or --all --force");
  }
  const removed = clearBuckets(keys);
  writeJson(
    {
      ok: true,
      removed,
      remaining: listBuckets().map((bucket) => bucket.key),
    },
    opts,
  );
  return ExitCode.Success;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(
      ExitCode.Usage,
      "docs requires a subcommand: sync, status, search, class, member, or clear",
    );
  }
  if (sub === "sync") return runSync(opts);
  if (sub === "status") return runStatus(opts);
  if (sub === "search") return runSearch(positional, opts);
  if (sub === "class") return runClass(positional, opts);
  if (sub === "member") return runMember(positional, opts);
  if (sub === "clear") return runClear(opts);
  throw new CliError(
    ExitCode.Usage,
    `unknown docs subcommand: ${sub} (expected sync, status, search, class, member, or clear)`,
  );
}
