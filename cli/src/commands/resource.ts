import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";

import {
  type DomainCommand,
  flagString,
  parseJsonValue,
  readArgsPayload,
  runDomainGroup,
} from "./scene";

function locatorArgs(opts: GlobalOptions, positional: string[]): Record<string, unknown> {
  const args: Record<string, unknown> = {};
  const path = flagString(opts, "--path") ?? positional[0];
  const name = flagString(opts, "--name");
  const objectId = flagString(opts, "--object-id");
  if (path !== undefined) args.path = path;
  if (name !== undefined) args.name = name;
  if (objectId !== undefined) args.object_id = objectId;
  return args;
}

function valueArg(opts: GlobalOptions, args: Record<string, unknown>): void {
  const value = flagString(opts, "--value");
  if (value !== undefined) args.value = parseJsonValue(value, "--value");
}

const RESOURCE_COMMANDS: Readonly<Record<string, DomainCommand>> = {
  list: {
    tool: "resource_query",
    risk: "none",
    action: "types",
    build: () => ({}),
  },
  templates: {
    tool: "resource_query",
    risk: "none",
    action: "types",
    build: () => ({}),
  },
  read: {
    tool: "resource_load",
    risk: "none",
    action: "load",
    build: (opts, positional) => {
      const args = locatorArgs(opts, positional);
      const typeHint = flagString(opts, "--type-hint");
      if (typeHint !== undefined) args.type_hint = typeHint;
      const raw = flagString(opts, "--raw");
      if (raw !== undefined) args.raw = raw;
      return args;
    },
  },
  watch: {
    tool: "resource_query",
    risk: "none",
    action: "exists",
    build: (opts, positional) => locatorArgs(opts, positional),
  },
  prompts: {
    tool: "os_file_text",
    risk: "none",
    action: "read",
    build: (_opts, positional) => {
      const args: Record<string, unknown> = { action: "read" };
      const name = positional[0];
      if (name !== undefined) args.path = name;
      return args;
    },
  },
  inspect: {
    tool: "resource_query",
    risk: "none",
    action: "type",
    build: (opts, positional) => locatorArgs(opts, positional),
  },
  get: {
    tool: "resource_property",
    risk: "none",
    action: "get",
    build: (opts, positional) => {
      const args = locatorArgs(opts, positional);
      const property = flagString(opts, "--property") ?? positional[1];
      if (property !== undefined) args.property = property;
      return args;
    },
  },
  set: {
    tool: "resource_property",
    risk: "low",
    action: "set",
    build: (opts, positional) => {
      const args = { ...locatorArgs(opts, positional), ...readArgsPayload(opts) };
      const property = flagString(opts, "--property") ?? positional[1];
      if (property !== undefined) args.property = property;
      valueArg(opts, args);
      return args;
    },
  },
  save: {
    tool: "resource_save",
    risk: "low",
    action: "save",
    build: (opts, positional) => {
      const args = locatorArgs(opts, positional);
      const dest = flagString(opts, "--dest");
      if (dest !== undefined) args.dest_path = dest;
      return args;
    },
  },
  import: {
    tool: "resource_import",
    risk: "low",
    action: "reimport",
    build: (opts, positional) => {
      const args: Record<string, unknown> = { ...readArgsPayload(opts), action: "reimport" };
      const path = flagString(opts, "--path") ?? positional[0];
      if (path !== undefined) {
        const existing = args.paths;
        args.paths = Array.isArray(existing) ? [...existing, path] : [path];
      }
      return args;
    },
  },
  rename: {
    tool: "resource_file",
    risk: "high",
    action: "rename",
    build: (opts, positional) => {
      const args = locatorArgs(opts, positional);
      const to = flagString(opts, "--to") ?? positional[1];
      if (to !== undefined) args.new_path = to;
      return args;
    },
  },
  move: {
    tool: "resource_file",
    risk: "high",
    action: "move",
    build: (opts, positional) => {
      const args = locatorArgs(opts, positional);
      const to = flagString(opts, "--to") ?? positional[1];
      if (to !== undefined) args.new_directory = to;
      return args;
    },
  },
  references: {
    tool: "resource_query",
    risk: "none",
    action: "references",
    build: (opts, positional) => {
      const args = locatorArgs(opts, positional);
      const uid = flagString(opts, "--uid");
      const className = flagString(opts, "--class");
      if (uid !== undefined) args.uid = uid;
      if (className !== undefined) args.class_name = className;
      return args;
    },
  },
};

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  return runDomainGroup("resource", RESOURCE_COMMANDS, positional, opts);
}

export function assertResourceSubcommand(sub: string | undefined): void {
  if (sub === undefined || !(sub in RESOURCE_COMMANDS)) {
    throw new CliError(
      ExitCode.Usage,
      `unknown resource subcommand: ${sub ?? "(none)"} (expected ${Object.keys(RESOURCE_COMMANDS).join("/")})`,
    );
  }
}
