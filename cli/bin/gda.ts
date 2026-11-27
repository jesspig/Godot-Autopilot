#!/usr/bin/env bun
import { parseArgv, type GlobalOptions } from "../src/args";
import {
  extractCommandTokens,
  findSpec,
  helpTextForTokens,
  isGroup,
  parseCommandArgs,
  resolveCommand,
  type CommandSpec,
} from "../src/command-spec";
import { run as runDebug } from "../src/commands/debug";
import { run as runDoctor } from "../src/commands/doctor";
import { run as runEditor } from "../src/commands/editor";
import { run as runGame } from "../src/commands/game";
import { run as runJob } from "../src/commands/job";
import { run as runLogs } from "../src/commands/logs";
import { run as runManifest } from "../src/commands/manifest";
import { run as runMonitor } from "../src/commands/monitor";
import { run as runProject } from "../src/commands/project";
import { run as runResource } from "../src/commands/resource";
import { runGenericToolCall, run as runScene } from "../src/commands/scene";
import { run as runScript } from "../src/commands/script";
import { run as runShell } from "../src/commands/shell";
import { run as runTest } from "../src/commands/test";
import { run as runTool } from "../src/commands/tool";
import { run as runTrace } from "../src/commands/trace";
import { CliError, ExitCode, errorMessage } from "../src/errors";
import { main } from "../src/main";
import { setActiveCommand, writeFailure } from "../src/output";

type GroupRunner = (positional: string[], opts: GlobalOptions) => Promise<number>;

process.on("SIGINT", () => {
  process.exit(ExitCode.Interrupted);
});

async function runToolGroup(positional: string[], opts: GlobalOptions): Promise<number> {
  if (positional[0] === "call") {
    const toolName = positional[1];
    if (toolName === undefined) {
      throw new CliError(ExitCode.Usage, "gda tool call requires a <tool> argument.");
    }
    setActiveCommand("tool.call");
    return runGenericToolCall(toolName, opts);
  }
  return runTool(positional, opts);
}

const GROUP_RUNNERS: Readonly<Record<string, GroupRunner>> = {
  editor: runEditor,
  project: runProject,
  tool: runToolGroup,
  doctor: runDoctor,
  diagnose: (positional, opts) => runDoctor(["diagnose", ...positional], opts),
  shell: runShell,
  scene: runScene,
  script: runScript,
  resource: runResource,
  game: runGame,
  debug: runDebug,
  logs: runLogs,
  test: runTest,
  job: runJob,
  trace: runTrace,
  monitor: runMonitor,
};

const DEFAULT_SUBCOMMAND: Readonly<Record<string, string>> = {
  editor: "status",
  project: "list",
  tool: "list",
  doctor: "run",
  diagnose: "diagnose",
  shell: "run",
  scene: "tree",
  script: "list",
  resource: "list",
  game: "status",
  debug: "status",
  logs: "editor",
  test: "list",
  job: "list",
  trace: "list",
  monitor: "status",
};

const FLAG_TO_OPTION: Readonly<Record<string, keyof GlobalOptions>> = {
  "--id": "id",
  "--socket": "socketText",
  "--port": "portText",
  "--project": "project",
  "--offline": "offline",
  "--category": "category",
  "--tags": "tagsText",
  "--tag": "tag",
  "--include": "include",
  "--force-stop": "force",
  "--force-clear": "force",
  "--force-install": "force",
  "--all-projects": "all",
  "--all-buckets": "all",
  "--probe-level": "probe",
  "--probe-port": "probe",
  "--report-file": "file",
  "--skill-file": "file",
  "--input-file": "file",
  "--export-format": "exportFormat",
  "--format": "formatText",
  "--wait": "waitText",
  "--godot": "godot",
  "--headless": "headless",
  "--play": "play",
  "--no-import": "noImport",
  "--projects": "projectsView",
  "--stale": "stale",
  "--timeout": "timeoutText",
  "--pretty": "pretty",
  "--raw": "raw",
  "--confirm": "confirm",
  "--progress": "progress",
  "--args": "argsJson",
  "--args-file": "argsFile",
};

function asGlobalOptions(options: Record<string, string | number | boolean | string[]>): GlobalOptions {
  const mapped: Record<string, unknown> = { ...options };
  for (const [flag, key] of Object.entries(FLAG_TO_OPTION)) {
    if (options[flag] !== undefined) mapped[key] = options[flag];
  }
  return mapped as GlobalOptions;
}

function reportGroupError(error: unknown, tokens: string[]): number {
  const command = tokens.join(".");
  const code = error instanceof CliError ? error.code : ExitCode.ToolFailure;
  const failure = error instanceof CliError
    ? error
    : new CliError(ExitCode.ToolFailure, errorMessage(error));
  writeFailure(command, failure, {});
  const help = helpTextForTokens(tokens.slice(0, 2));
  if (help !== undefined) process.stderr.write(`${help}\n`);
  return code;
}

async function runGroup(tokens: string[], rest: string[]): Promise<number | undefined> {
  const group = tokens[0];
  const runner = GROUP_RUNNERS[group];
  if (runner === undefined) return undefined;
  const defaultSub = DEFAULT_SUBCOMMAND[group];
  const sub = tokens[1] ?? defaultSub;
  const key = group === "diagnose" ? "doctor.diagnose" : `${group}.${sub}`;
  const spec: CommandSpec | undefined = findSpec(key) ?? findSpec(`${group}.${defaultSub}`);
  if (spec === undefined) {
    throw new CliError(ExitCode.Usage, `no command spec for ${key}`);
  }
  const parsed = parseCommandArgs(spec, rest);
  const positional = group === "diagnose" ? parsed.positionals : [sub, ...parsed.positionals];
  setActiveCommand(spec.path);
  return runner(positional, asGlobalOptions(parsed.options));
}

const argv = process.argv.slice(2);
const { tokens, indexes } = extractCommandTokens(argv);
const rest = argv.filter((_, index) => !indexes.includes(index));

if (tokens[0] === "commands") {
  const { opts } = parseArgv(rest);
  setActiveCommand("commands");
  process.exit(await runManifest([], opts));
}

if (argv.includes("--help")) {
  const help = helpTextForTokens(tokens);
  if (help !== undefined) {
    process.stdout.write(`${help}\n`);
    process.exit(ExitCode.Success);
  }
}

if (GROUP_RUNNERS[tokens[0]] !== undefined) {
  try {
    const code = await runGroup(tokens, rest);
    if (code !== undefined) process.exit(code);
  } catch (error) {
    process.exit(reportGroupError(error, tokens));
  }
}

if (isGroup(tokens[0])) {
  const spec = resolveCommand(tokens.slice(0, 2));
  if (spec !== undefined) {
    try {
      parseCommandArgs(spec, rest);
    } catch (error) {
      process.exit(reportGroupError(error, tokens));
    }
  }
}

process.exit(await main(argv));
