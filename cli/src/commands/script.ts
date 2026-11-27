import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";

import {
  type DomainCommand,
  flagBool,
  flagString,
  parseJsonValue,
  readArgsPayload,
  runDomainGroup,
} from "./scene";

function pathOrPositional(opts: GlobalOptions, positional: string[], index: number): string | undefined {
  return flagString(opts, "--path") ?? positional[index];
}

function sourceArgs(opts: GlobalOptions, extraKey: string): Record<string, unknown> {
  const args: Record<string, unknown> = { ...readArgsPayload(opts) };
  const source = flagString(opts, "--source");
  if (source !== undefined) {
    args[extraKey] = parseJsonValue(source, "--source");
  }
  return args;
}

const SCRIPT_COMMANDS: Readonly<Record<string, DomainCommand>> = {
  list: {
    tool: "get_editor_files",
    risk: "none",
    action: "list",
    build: (opts) => {
      const args: Record<string, unknown> = { action: "list" };
      const directory = flagString(opts, "--directory");
      if (directory !== undefined) args.directory = directory;
      return args;
    },
  },
  read: {
    tool: "os_file_text",
    risk: "none",
    action: "read",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { action: "read" };
      const path = pathOrPositional(opts, positional, 0);
      if (path !== undefined) args.path = path;
      return args;
    },
  },
  create: {
    tool: "create_script",
    risk: "low",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts) };
      const path = pathOrPositional(opts, positional, 0);
      const extendsClass = flagString(opts, "--extends");
      const source = flagString(opts, "--source") ?? positional[1];
      if (path !== undefined) args.path = path;
      if (extendsClass !== undefined) args.extends = extendsClass;
      if (source !== undefined) args.source_code = source;
      return args;
    },
  },
  delete: {
    tool: "resource_file",
    risk: "high",
    action: "remove",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { action: "remove" };
      const path = pathOrPositional(opts, positional, 0);
      if (path !== undefined) args.path = path;
      return args;
    },
  },
  patch: {
    tool: "patch_script",
    risk: "low",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts) };
      const path = pathOrPositional(opts, positional, 0);
      const source = flagString(opts, "--source") ?? positional[1];
      if (path !== undefined) args.path = path;
      if (source !== undefined) args.source_code = source;
      if (flagBool(opts, "--dry-run")) args.dry_run = true;
      return args;
    },
  },
  compile: {
    tool: "patch_script",
    risk: "none",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts) };
      const path = pathOrPositional(opts, positional, 0);
      const source = flagString(opts, "--source") ?? positional[1];
      if (path !== undefined) args.path = path;
      if (source !== undefined) args.source_code = source;
      args.dry_run = true;
      return args;
    },
  },
  attach: {
    tool: "attach_script_to_node",
    risk: "low",
    build: (opts, positional) => {
      const args: Record<string, unknown> = {};
      const node = flagString(opts, "--node") ?? positional[0];
      const script = flagString(opts, "--script") ?? positional[1];
      if (node !== undefined) args.node_path = node;
      if (script !== undefined) args.script_path = script;
      return args;
    },
  },
  exec: {
    tool: "execute_script",
    risk: "high",
    build: (opts) => sourceArgs(opts, "source_code"),
  },
  reload: {
    tool: "reload_script",
    risk: "low",
    build: (opts, positional) => {
      const args: Record<string, unknown> = {};
      const path = pathOrPositional(opts, positional, 0);
      if (path !== undefined) args.path = path;
      if (flagBool(opts, "--keep-state")) args.keep_state = true;
      return args;
    },
  },
  test: {
    tool: "run_gdscript_tests",
    risk: "none",
    build: (opts) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts) };
      const directory = flagString(opts, "--directory");
      const pattern = flagString(opts, "--pattern");
      const timeout = flagString(opts, "--timeout");
      if (directory !== undefined) args.directory = directory;
      if (pattern !== undefined) args.pattern = pattern;
      if (timeout !== undefined) args.timeout_ms = Number(timeout) * 1000;
      return args;
    },
  },
};

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  return runDomainGroup("script", SCRIPT_COMMANDS, positional, opts);
}

export function assertScriptSubcommand(sub: string | undefined): void {
  if (sub === undefined || !(sub in SCRIPT_COMMANDS)) {
    throw new CliError(
      ExitCode.Usage,
      `unknown script subcommand: ${sub ?? "(none)"} (expected ${Object.keys(SCRIPT_COMMANDS).join("/")})`,
    );
  }
}
