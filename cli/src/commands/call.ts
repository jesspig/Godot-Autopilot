import fs from "node:fs";

import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { callTool, META_DIRECT_TOOLS } from "../mcp/client";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

function loadToolArguments(opts: GlobalOptions): Record<string, unknown> {
  if (opts.argsJson !== undefined && opts.argsFile !== undefined) {
    throw new CliError(ExitCode.Usage, "use only one of --args and --args-file.");
  }
  let args: unknown = {};
  if (opts.argsJson !== undefined) {
    try {
      args = JSON.parse(opts.argsJson);
    } catch {
      throw new CliError(ExitCode.Usage, "--args is not valid JSON.");
    }
  } else if (opts.argsFile !== undefined) {
    let text: string;
    try {
      text = fs.readFileSync(opts.argsFile, "utf8");
    } catch {
      throw new CliError(ExitCode.Usage, `cannot read --args-file: ${opts.argsFile}`);
    }
    try {
      args = JSON.parse(text);
    } catch {
      throw new CliError(ExitCode.Usage, `--args-file does not contain valid JSON: ${opts.argsFile}`);
    }
  }
  if (args === null || typeof args !== "object" || Array.isArray(args)) {
    throw new CliError(ExitCode.Usage, "tool arguments must be a JSON object.");
  }
  return args as Record<string, unknown>;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("call", opts);
  if (positional.length !== 1) {
    throw new CliError(ExitCode.Usage, "call requires exactly one <tool> argument.");
  }
  const tool = positional[0];
  const args = loadToolArguments(opts);
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const envelope = await callTool(tool, args, {
    port: target.port,
    timeoutSec: callTimeoutSec(opts),
    direct: META_DIRECT_TOOLS.has(tool),
  });
  const { display, toolFailed } = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  return emit(display, toolFailed, opts);
}
