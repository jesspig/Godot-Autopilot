import fs from "node:fs";

import type { GlobalOptions } from "../args";
import type { RiskLevel } from "../command-spec";
import { nativeToolInvoke, type NativeCallOutcome } from "../discovery";
import { CliError, ConfirmationRequiredError, ExitCode } from "../errors";
import { promptConfirmation, type ConfirmationSummary } from "../output";
import { findById, findLiveByProject } from "../registry";
import { resolveNativeTarget } from "../target";
import { writeJson } from "./json-output";

const TARGET_CONTEXT = { findById, findLiveByProject };

export interface DomainCommand {
  tool: string | ((opts: GlobalOptions) => string);
  risk: RiskLevel;
  action?: string;
  build(opts: GlobalOptions, positional: string[]): Record<string, unknown>;
}

export function flagString(opts: GlobalOptions, name: string): string | undefined {
  const value = (opts as unknown as Record<string, unknown>)[name];
  return typeof value === "string" && value !== "" ? value : undefined;
}

export function flagNumber(opts: GlobalOptions, name: string): number | undefined {
  const value = (opts as unknown as Record<string, unknown>)[name];
  return typeof value === "number" && Number.isFinite(value) ? value : undefined;
}

export function flagBool(opts: GlobalOptions, name: string): boolean {
  return (opts as unknown as Record<string, unknown>)[name] === true;
}

export function confirmed(opts: GlobalOptions): boolean {
  return opts.confirm === true || flagBool(opts, "--confirm");
}

export function parseJsonValue(text: string, label: string): unknown {
  try {
    return JSON.parse(text);
  } catch {
    throw new CliError(ExitCode.Usage, `${label} is not valid JSON`);
  }
}

export function readArgsPayload(opts: GlobalOptions): Record<string, unknown> {
  const argsJson = opts.argsJson ?? flagString(opts, "--args");
  const argsFile = opts.argsFile ?? flagString(opts, "--args-file");
  if (argsJson !== undefined && argsFile !== undefined) {
    throw new CliError(ExitCode.Usage, "use only one of --args and --args-file.");
  }
  let text: string | undefined;
  if (argsJson !== undefined) {
    text = argsJson;
  } else if (argsFile !== undefined) {
    if (argsFile === "-") {
      try {
        text = fs.readFileSync(0, "utf8");
      } catch {
        throw new CliError(ExitCode.Usage, "cannot read --args-file - from stdin.");
      }
    } else {
      try {
        text = fs.readFileSync(argsFile, "utf8");
      } catch {
        throw new CliError(ExitCode.Usage, `cannot read --args-file: ${argsFile}`);
      }
    }
  }
  if (text === undefined) return {};
  const parsed = parseJsonValue(text, "tool arguments");
  if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) {
    throw new CliError(ExitCode.Usage, "tool arguments must be a JSON object.");
  }
  return parsed as Record<string, unknown>;
}

export function copyFlags(
  opts: GlobalOptions,
  pairs: ReadonlyArray<readonly [string, string]>,
): Record<string, unknown> {
  const args: Record<string, unknown> = {};
  for (const [flag, key] of pairs) {
    const value = (opts as unknown as Record<string, unknown>)[flag];
    if (value !== undefined && value !== false && value !== "") {
      args[key] = value;
    }
  }
  return args;
}

function textOf(value: unknown): string | undefined {
  return typeof value === "string" && value !== "" ? value : undefined;
}

function emitDomainEnvelope(
  command: string,
  spec: DomainCommand,
  tool: string,
  outcome: NativeCallOutcome,
  args: Record<string, unknown>,
  opts: GlobalOptions,
): void {
  const payload = outcome.payload;
  const record: Record<string, unknown> = payload !== null && typeof payload === "object" && !Array.isArray(payload)
    ? payload as Record<string, unknown>
    : {};
  writeJson(
    {
      schema_version: 1,
      ok: outcome.ok,
      command,
      tool,
      action: spec.action ?? undefined,
      scene_path: textOf(record.scene_path) ?? textOf(args.path) ?? textOf(args.parent_path) ?? "",
      revision: textOf(record.revision) ?? "",
      receipt: {
        request_id: outcome.request_id,
        digest: outcome.digest,
        artifact_id: outcome.artifact_id,
      },
      result: outcome.status === "ok" ? payload : null,
      task_id: outcome.task_id === "" ? undefined : outcome.task_id,
      error: outcome.error_message === "" ? undefined : outcome.error_message,
      new_errors_since_last_call: typeof record.new_errors_since_last_call === "number"
        ? record.new_errors_since_last_call
        : 0,
    },
    opts,
  );
}

export async function invokeDomainCommand(
  command: string,
  spec: DomainCommand,
  opts: GlobalOptions,
  positional: string[],
): Promise<number> {
  const target = resolveNativeTarget(opts, TARGET_CONTEXT);
  if (target.ipcEndpoint === undefined) {
    throw new CliError(
      ExitCode.Transport,
      "no native IPC endpoint is available; pass --socket or --id from 'gda editor list'.",
    );
  }
  const instanceId = target.instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "";
  const tool = typeof spec.tool === "function" ? spec.tool(opts) : spec.tool;
  const args = spec.build(opts, positional);
  if (spec.action !== undefined && args.action === undefined) args.action = spec.action;
  if (spec.risk === "high" || opts.confirm === true) {
    const summary: ConfirmationSummary = {
      command,
      tool,
      target: instanceId !== "" ? `instance:${instanceId}` : `port:${target.port}`,
      risk: spec.risk,
      args: { ...args },
    };
    await promptConfirmation(summary, opts);
    args.confirm = true;
  }
  const outcome = await nativeToolInvoke(target.ipcEndpoint, instanceId, tool, args, {
    confirm: spec.risk === "high" || opts.confirm === true,
  });
  if (outcome.status === "needs_confirmation") {
    throw new ConfirmationRequiredError(
      `gda ${command} was denied: confirmation_required (${outcome.error_message || "no confirmation"}).`,
      {
        command,
        tool,
        target: instanceId !== "" ? `instance:${instanceId}` : `port:${target.port}`,
        risk: spec.risk,
        args: { ...args },
      },
    );
  }
  emitDomainEnvelope(command, spec, tool, outcome, args, opts);
  return outcome.ok ? ExitCode.Success : ExitCode.ToolFailure;
}

export async function runDomainGroup(
  group: string,
  commands: Readonly<Record<string, DomainCommand>>,
  positional: string[],
  opts: GlobalOptions,
): Promise<number> {
  const sub = positional[0];
  if (sub === undefined) {
    throw new CliError(ExitCode.Usage, `gda ${group} requires a subcommand: ${Object.keys(commands).join("/")}.`);
  }
  const spec = commands[sub];
  if (spec === undefined) {
    throw new CliError(
      ExitCode.Usage,
      `unknown ${group} subcommand: ${sub} (expected ${Object.keys(commands).join("/")})`,
    );
  }
  return invokeDomainCommand(`${group}.${sub}`, spec, opts, positional.slice(1));
}

const SCENE_COMMANDS: Readonly<Record<string, DomainCommand>> = {
  list: {
    tool: "get_editor_state",
    risk: "none",
    action: "open_scenes",
    build: () => ({}),
  },
  open: {
    tool: "editor_scene_control",
    risk: "low",
    action: "open",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { action: "open" };
      const path = flagString(opts, "--path") ?? positional[0];
      if (path !== undefined) args.path = path;
      return args;
    },
  },
  add: {
    tool: "create_scene_node",
    risk: "low",
    build: (opts, positional) => {
      const args: Record<string, unknown> = {};
      const type = flagString(opts, "--type") ?? positional[0];
      const parent = flagString(opts, "--parent");
      if (type !== undefined) args.type = type;
      if (parent !== undefined) args.parent_path = parent;
      return args;
    },
  },
  save: {
    tool: "save_editor_scene",
    risk: "low",
    build: (opts) => {
      const args: Record<string, unknown> = { action: "save" };
      const path = flagString(opts, "--path");
      if (path !== undefined) {
        args.action = "save_as";
        args.path = path;
      }
      return args;
    },
  },
  tree: {
    tool: "get_scene_tree",
    risk: "none",
    build: (opts) => copyFlags(opts, [
      ["--max-depth", "max_depth"],
      ["--pattern", "pattern"],
      ["--type", "type"],
      ["--group", "group"],
      ["--include-properties", "include_properties"],
      ["--include-groups", "include_groups"],
      ["--include-hash", "include_hash"],
    ]),
  },
  node: {
    tool: "create_scene_node",
    risk: "low",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts) };
      const type = flagString(opts, "--type") ?? positional[0];
      const name = flagString(opts, "--name") ?? positional[1];
      const parent = flagString(opts, "--parent");
      const props = flagString(opts, "--props");
      if (type !== undefined) args.type = type;
      if (name !== undefined) args.name = name;
      if (parent !== undefined) args.parent_path = parent;
      if (props !== undefined) args.properties = parseJsonValue(props, "--props");
      return args;
    },
  },
  property: {
    tool: "property_set",
    risk: "low",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts) };
      const path = flagString(opts, "--path") ?? positional[0];
      const property = flagString(opts, "--property") ?? positional[1];
      const value = flagString(opts, "--value") ?? positional[2];
      const hint = flagString(opts, "--type-hint");
      if (path !== undefined) args.path = path;
      if (property !== undefined) args.property = property;
      if (value !== undefined) args.value = parseJsonValue(value, "--value");
      if (hint !== undefined) args.type_hint = hint;
      return args;
    },
  },
  signal: {
    tool: (opts) => (flagBool(opts, "--disconnect") ? "signal_disconnect" : "signal_connect"),
    risk: "low",
    build: (opts) => {
      const args: Record<string, unknown> = {};
      const from = flagString(opts, "--from");
      const signal = flagString(opts, "--signal");
      const to = flagString(opts, "--to");
      const method = flagString(opts, "--method");
      if (from !== undefined) args.source_path = from;
      if (signal !== undefined) args.signal = signal;
      if (to !== undefined) args.target_path = to;
      if (method !== undefined) args.method = method;
      if (flagBool(opts, "--no-persist")) args.persist = false;
      return args;
    },
  },
  verify: {
    tool: "get_scene_tree",
    risk: "none",
    build: (opts) => {
      const args = copyFlags(opts, [["--max-depth", "max_depth"], ["--pattern", "pattern"]]);
      args.verify = true;
      return args;
    },
  },
  diff: {
    tool: "get_scene_tree",
    risk: "none",
    build: (opts) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts) };
      const maxDepth = flagNumber(opts, "--max-depth");
      if (maxDepth !== undefined) args.max_depth = maxDepth;
      args.verify = true;
      args.include_hash = true;
      return args;
    },
  },
};

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  return runDomainGroup("scene", SCENE_COMMANDS, positional, opts);
}

export async function runGenericToolCall(toolName: string, opts: GlobalOptions): Promise<number> {
  const args = readArgsPayload(opts);
  if (opts.confirm === true || flagBool(opts, "--confirm")) {
    args.confirm = true;
  }
  const spec: DomainCommand = {
    tool: toolName,
    risk: "none",
    build: () => args,
  };
  return invokeDomainCommand(`tool.${toolName}`, spec, opts, []);
}
