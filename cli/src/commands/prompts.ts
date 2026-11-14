import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { request } from "../mcp/client";
import { callTimeoutSec, loadToolArguments } from "../payload";
import { err } from "../output";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

interface PromptMessage {
  role?: unknown;
  content?: unknown;
}

function promptArguments(opts: GlobalOptions): Record<string, string> {
  const raw = loadToolArguments(opts);
  const args: Record<string, string> = {};
  for (const [key, value] of Object.entries(raw)) {
    if (typeof value === "string") {
      args[key] = value;
    } else if (typeof value === "number" || typeof value === "boolean") {
      args[key] = String(value);
      err(`prompt argument '${key}' converted from ${typeof value} to string`);
    } else {
      throw new CliError(ExitCode.Usage, `prompt argument '${key}' must be a string`);
    }
  }
  return args;
}

function renderMessages(messages: unknown): string {
  if (!Array.isArray(messages)) return "";
  const blocks: string[] = [];
  for (const message of messages as PromptMessage[]) {
    const role = typeof message.role === "string" ? message.role : "unknown";
    const content = message.content;
    let text = "";
    if (content !== null && typeof content === "object" && !Array.isArray(content)) {
      const record = content as { text?: unknown; type?: unknown };
      if (typeof record.text === "string") text = record.text;
      else text = JSON.stringify(content, null, 2);
    } else if (typeof content === "string") {
      text = content;
    }
    blocks.push(`[${role}] ${text}`);
  }
  return blocks.join("\n\n");
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === "list") {
    const target = resolveTarget(opts, TARGET_CONTEXT);
    const response = await request("prompts/list", {}, {
      port: target.port,
      timeoutSec: callTimeoutSec(opts),
    });
    if (response.error !== undefined) {
      throw new CliError(ExitCode.ToolFailure, `prompts/list failed: ${response.error.message ?? "server error"} (code ${response.error.code})`);
    }
    const result = response.result as { prompts?: unknown } | null;
    writeJson({ prompts: result?.prompts ?? [] }, opts);
    return ExitCode.Success;
  }
  if (sub === "get") {
    const name = positional[1];
    if (name === undefined) {
      throw new CliError(ExitCode.Usage, "prompts get requires a <name> argument.");
    }
    if (positional.length > 2) {
      throw new CliError(ExitCode.Usage, "prompts get takes exactly one <name> argument.");
    }
    const target = resolveTarget(opts, TARGET_CONTEXT);
    const response = await request("prompts/get", { name, arguments: promptArguments(opts) }, {
      port: target.port,
      timeoutSec: callTimeoutSec(opts),
    });
    if (response.error !== undefined) {
      throw new CliError(ExitCode.ToolFailure, `prompts/get failed: ${response.error.message ?? "server error"} (code ${response.error.code})`);
    }
    const result = response.result as { messages?: unknown } | null;
    if (opts.json === true || opts.raw === true) {
      writeJson(opts.raw === true ? response : result, opts);
      return ExitCode.Success;
    }
    const text = renderMessages(result?.messages);
    if (text === "") {
      throw new CliError(ExitCode.ToolFailure, `prompt '${name}' returned no messages`);
    }
    process.stdout.write(`${text}\n`);
    return ExitCode.Success;
  }
  throw new CliError(ExitCode.Usage, `unknown prompts subcommand: ${sub ?? "(none)"} (expected list or get)`);
}
