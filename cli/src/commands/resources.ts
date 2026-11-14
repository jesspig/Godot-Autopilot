import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { request } from "../mcp/client";
import { callTimeoutSec } from "../payload";
import { redactContents, renderTextContents, saveContents, type ResourceContent } from "../render";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

interface ResourceListResult {
  resources?: unknown[];
  resourceTemplates?: unknown[];
}

function contentsOf(result: unknown): ResourceContent[] {
  if (result === null || typeof result !== "object" || Array.isArray(result)) return [];
  const contents = (result as { contents?: unknown }).contents;
  if (!Array.isArray(contents)) return [];
  return contents.filter(
    (item): item is ResourceContent =>
      item !== null && typeof item === "object" && typeof (item as { uri?: unknown }).uri === "string",
  );
}

async function call(
  method: string,
  params: Record<string, unknown>,
  opts: GlobalOptions,
): Promise<{ response: unknown; result: unknown }> {
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const response = await request(method, params, {
    port: target.port,
    timeoutSec: callTimeoutSec(opts),
    transport: opts.transportText === undefined ? "auto" : (opts.transportText as "auto" | "ipc" | "http"),
    socketPath: opts.socketText,
    noFallback: opts.noFallback,
  });
  if (response.error !== undefined) {
    throw new CliError(
      ExitCode.ToolFailure,
      `${method} failed: ${response.error.message ?? "server error"} (code ${response.error.code})`,
    );
  }
  return { response, result: response.result };
}

function intervalMs(opts: GlobalOptions): number {
  if (opts.intervalText === undefined) return 1000;
  const seconds = Number(opts.intervalText);
  if (!Number.isFinite(seconds) || seconds <= 0) {
    throw new CliError(ExitCode.Usage, `--interval must be a positive number of seconds, got: ${opts.intervalText}`);
  }
  return Math.max(100, Math.round(seconds * 1000));
}

function outputContents(contents: ResourceContent[], opts: GlobalOptions): void {
  if (opts.raw === true) {
    writeJson(contents, opts);
    return;
  }
  const text = renderTextContents(contents, true);
  if (text !== "") process.stdout.write(`${text}\n`);
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === "list") {
    const { result } = await call("resources/list", {}, opts);
    const listed = result as ResourceListResult;
    const payload: Record<string, unknown> = { resources: listed.resources ?? [] };
    if (opts.templates === true) {
      const templates = await call("resources/templates/list", {}, opts);
      payload.templates = (templates.result as ResourceListResult).resourceTemplates ?? [];
    }
    writeJson(payload, opts);
    return ExitCode.Success;
  }
  if (sub === "templates") {
    const { result } = await call("resources/templates/list", {}, opts);
    writeJson({ templates: (result as ResourceListResult).resourceTemplates ?? [] }, opts);
    return ExitCode.Success;
  }
  if (sub === "read") {
    const uri = positional[1];
    if (uri === undefined) {
      throw new CliError(ExitCode.Usage, "resources read requires a <uri> argument.");
    }
    const { response, result } = await call("resources/read", { uri }, opts);
    if (opts.raw === true) {
      writeJson(response, opts);
      return ExitCode.Success;
    }
    const contents = contentsOf(result);
    if (contents.length === 0) {
      throw new CliError(ExitCode.ToolFailure, `resource returned no contents: ${uri}`);
    }
    if (opts.saveText !== undefined) {
      const saved = saveContents(contents, opts.saveText);
      writeJson({ saved }, opts);
      return ExitCode.Success;
    }
    if (opts.json === true) {
      writeJson({ contents: redactContents(contents) }, opts);
      return ExitCode.Success;
    }
    outputContents(contents, opts);
    return ExitCode.Success;
  }
  if (sub === "watch") {
    const uri = positional[1];
    if (uri === undefined) {
      throw new CliError(ExitCode.Usage, "resources watch requires a <uri> argument.");
    }
    const delay = intervalMs(opts);
    let previous = "";
    for (;;) {
      const { result } = await call("resources/read", { uri }, opts);
      const contents = contentsOf(result);
      const fingerprint = JSON.stringify(contents);
      if (fingerprint !== previous) {
        previous = fingerprint;
        if (opts.json === true) {
          writeJson({ contents: redactContents(contents) }, opts);
        } else {
          outputContents(contents, opts);
        }
      }
      await new Promise((resolve) => setTimeout(resolve, delay));
    }
  }
  throw new CliError(ExitCode.Usage, `unknown resources subcommand: ${sub ?? "(none)"} (expected list, templates, read or watch)`);
}
