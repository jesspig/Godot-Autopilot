#!/usr/bin/env bun
import { runBuild, type BuildOptions } from "./commands/build";
import { runClean } from "./commands/clean";
import { runDeploy, type DeployOptions } from "./commands/deploy";
import {
  runHarnessCoverage,
  runHarnessDigest,
  runHarnessSetup,
  type HarnessCoverageOptions,
  type HarnessDigestOptions,
  type HarnessSetupOptions,
} from "./commands/harness";
import { runJam, type JamOptions } from "./commands/jam";
import { runPackage } from "./commands/package";
import { runPackageCheck } from "./commands/package-check";
import { runTests, type TestOptions } from "./commands/test";
import { runToolAudit, type ToolAuditOptions } from "./commands/tool-audit";
import { runTui } from "./tui";

const USAGE = [
  "Usage: bun run gda-dev-tools <command> [options]",
  "",
  "Commands:",
  "  build                    [--release|--debug]",
  "  deploy                   [--release|--debug] [--project <dir>]...",
  "  clean                    [--project <dir>]...",
  "  package                  [--libs-dir <dir> | --preset debug|release]",
  "  package-check <zip>",
  "  test                     [--preset debug|release] [--single <用例名>]",
  "  audit-tools              [--json|--baseline|--target] [--report [--json] [--out <file>]]",
  "  jam                      [--theme <id|theme>] [--json]",
  "  jam create               --theme <id|theme> --name <name> [--slug <slug>] [--out <dir>] [--json]",
  "  jam e2e                  [--reset] [--theme <id|theme>] [--json]",
  "  jam list                 [--projects] [--json]",
  "  jam index                [--json]",
  "  harness setup            --project <dir> [--headless]",
  "  harness coverage         --round <dir> [--trace <path>] [--json]",
  "  harness digest           --round <dir> [--json]",
  "  tui",
].join("\n");

class UsageError extends Error {}

function expectNoArgs(command: string, args: string[]): void {
  if (args.length > 0) throw new UsageError(`${command}: unrecognized argument: ${args[0]}`);
}

function parseBuild(args: string[]): BuildOptions {
  const options: BuildOptions = { release: false, debug: false };
  for (const arg of args) {
    if (arg === "--release") options.release = true;
    else if (arg === "--debug") options.debug = true;
    else throw new UsageError(`build: unrecognized argument: ${arg}`);
  }
  return options;
}

function parseProjectArgs(command: string, args: string[]): string[] {
  const projects: string[] = [];
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg !== "--project") throw new UsageError(`${command}: unrecognized argument: ${arg}`);
    const value = args[index + 1];
    if (value === undefined || value.startsWith("-")) {
      throw new UsageError(`${command}: argument --project: expected one argument`);
    }
    projects.push(value);
    index += 1;
  }
  return projects;
}

function parseDeploy(args: string[]): DeployOptions {
  const options: DeployOptions = { release: false, debug: false, projects: [] };
  const remaining: string[] = [];
  for (const arg of args) {
    if (arg === "--release") options.release = true;
    else if (arg === "--debug") options.debug = true;
    else remaining.push(arg);
  }
  options.projects = parseProjectArgs("deploy", remaining);
  return options;
}

function parsePackage(args: string[]): { libsDir: string | null; preset: "debug" | "release" } {
  let libsDir: string | null = null;
  let preset: "debug" | "release" = "debug";
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg !== "--libs-dir" && arg !== "--preset") {
      throw new UsageError(`package: unrecognized argument: ${arg}`);
    }
    const value = args[index + 1];
    if (value === undefined || value.startsWith("-")) {
      throw new UsageError(`package: argument ${arg}: expected one argument`);
    }
    if (arg === "--libs-dir") libsDir = value;
    else if (value === "debug" || value === "release") preset = value;
    else {
      throw new UsageError(
        `package: argument --preset: invalid choice: '${value}' (choose from 'debug', 'release')`,
      );
    }
    index += 1;
  }
  if (libsDir !== null && args.includes("--preset")) {
    throw new UsageError("package: --preset cannot be combined with --libs-dir");
  }
  return { libsDir, preset };
}

function parseToolAudit(args: string[]): ToolAuditOptions {
  const options: ToolAuditOptions = {
    json: false,
    baseline: false,
    target: false,
    report: false,
    out: null,
  };
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--json") options.json = true;
    else if (arg === "--baseline") options.baseline = true;
    else if (arg === "--target") options.target = true;
    else if (arg === "--report") options.report = true;
    else if (arg === "--out") {
      const value = args[index + 1];
      if (value === undefined || value.startsWith("-")) {
        throw new UsageError("audit-tools: argument --out: expected one argument");
      }
      options.out = value;
      index += 1;
    } else throw new UsageError(`audit-tools: unrecognized argument: ${arg}`);
  }
  if (options.report) {
    if (options.baseline || options.target) {
      throw new UsageError("audit-tools: --report cannot be combined with --baseline or --target");
    }
  } else {
    const selected = [options.json, options.baseline, options.target].filter(Boolean).length;
    if (selected > 1) {
      throw new UsageError("audit-tools: choose only one of --json, --baseline, --target");
    }
    if (options.out !== null) {
      throw new UsageError("audit-tools: --out requires --report");
    }
  }
  return options;
}

function parseTest(args: string[]): TestOptions {
  const options: TestOptions = { preset: "debug", single: null };
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--preset") {
      const value = args[index + 1];
      if (value === undefined) {
        throw new UsageError("test: argument --preset: expected one argument");
      }
      if (value !== "debug" && value !== "release") {
        throw new UsageError(
          `test: argument --preset: invalid choice: '${value}' (choose from 'debug', 'release')`,
        );
      }
      options.preset = value;
      index += 1;
    } else if (arg === "--single") {
      const value = args[index + 1];
      if (value === undefined) {
        throw new UsageError("test: argument --single: expected one argument");
      }
      options.single = value;
      index += 1;
    } else throw new UsageError(`test: unrecognized argument: ${arg}`);
  }
  return options;
}

function parseJam(args: string[]): JamOptions {
  const options: JamOptions = {
    action: "draw",
    theme: null,
    name: null,
    slug: null,
    out: null,
    json: false,
    reset: false,
  };
  let cursor = 0;
  const subcommand = args[0];
  if (
    subcommand === "create" ||
    subcommand === "e2e" ||
    subcommand === "list" ||
    subcommand === "index"
  ) {
    options.action = subcommand;
    cursor = 1;
  }
  let projects = false;
  for (let index = cursor; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--json") options.json = true;
    else if (arg === "--reset") {
      if (options.action !== "e2e") {
        throw new UsageError("jam: --reset is only valid for jam e2e");
      }
      options.reset = true;
    } else if (arg === "--list") {
      if (options.action !== "draw") throw new UsageError("jam: --list is only valid for jam");
      options.action = "list";
    } else if (arg === "--projects") {
      if (options.action !== "list") {
        throw new UsageError("jam: --projects requires jam list");
      }
      projects = true;
    } else if (
      arg === "--theme" ||
      arg === "--name" ||
      arg === "--slug" ||
      arg === "--out"
    ) {
      const value = args[index + 1];
      if (value === undefined || value === "" || value.startsWith("-")) {
        throw new UsageError(`jam: argument ${arg}: expected one argument`);
      }
      if (arg === "--theme") options.theme = value;
      else if (arg === "--name") options.name = value;
      else if (arg === "--slug") options.slug = value;
      else options.out = value;
      index += 1;
    } else throw new UsageError(`jam: unrecognized argument: ${arg}`);
  }
  if (projects) options.action = "projects";
  if (options.action === "draw") {
    if (options.name !== null || options.slug !== null || options.out !== null) {
      throw new UsageError("jam: only --theme and --json are valid when drawing a theme");
    }
  } else if (options.action === "create") {
    if (options.theme === null || options.name === null) {
      throw new UsageError("jam create: --theme and --name are required");
    }
  } else if (options.action === "e2e") {
    if (options.name !== null || options.slug !== null || options.out !== null) {
      throw new UsageError("jam e2e: --name, --slug and --out are not valid");
    }
  } else if (options.theme !== null || options.name !== null || options.slug !== null || options.out !== null) {
    throw new UsageError(`jam ${options.action}: --theme, --name, --slug and --out are not valid`);
  }
  return options;
}

function readOptionValue(args: string[], index: number, flag: string, label: string): string {
  const value = args[index + 1];
  if (value === undefined || value === "" || value.startsWith("-")) {
    throw new UsageError(`${label}: argument ${flag}: expected one argument`);
  }
  return value;
}

export function parseHarnessSetup(args: string[]): HarnessSetupOptions {
  let project: string | null = null;
  let headless = false;
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--headless") headless = true;
    else if (arg === "--project") {
      if (project !== null) throw new UsageError("harness setup: --project may be given only once");
      project = readOptionValue(args, index, arg, "harness setup");
      index += 1;
    } else throw new UsageError(`harness setup: unrecognized argument: ${arg}`);
  }
  if (project === null) throw new UsageError("harness setup: --project <dir> is required");
  return { project, headless };
}

export function parseHarnessCoverage(args: string[]): HarnessCoverageOptions {
  let round: string | null = null;
  let trace: string | null = null;
  let json = false;
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--json") json = true;
    else if (arg === "--round") {
      if (round !== null) throw new UsageError("harness coverage: --round may be given only once");
      round = readOptionValue(args, index, arg, "harness coverage");
      index += 1;
    } else if (arg === "--trace") {
      if (trace !== null) throw new UsageError("harness coverage: --trace may be given only once");
      trace = readOptionValue(args, index, arg, "harness coverage");
      index += 1;
    } else throw new UsageError(`harness coverage: unrecognized argument: ${arg}`);
  }
  if (round === null) throw new UsageError("harness coverage: --round <dir> is required");
  return { round, trace, json };
}

export function parseHarnessDigest(args: string[]): HarnessDigestOptions {
  let round: string | null = null;
  let json = false;
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--json") json = true;
    else if (arg === "--round") {
      if (round !== null) throw new UsageError("harness digest: --round may be given only once");
      round = readOptionValue(args, index, arg, "harness digest");
      index += 1;
    } else throw new UsageError(`harness digest: unrecognized argument: ${arg}`);
  }
  if (round === null) throw new UsageError("harness digest: --round <dir> is required");
  return { round, json };
}

export async function main(argv: string[]): Promise<number> {
  const [command, ...rest] = argv;
  if (command === undefined) {
    console.error(USAGE);
    return 2;
  }
  if (command === "--help" || command === "-h") {
    console.log(USAGE);
    return 0;
  }
  if (rest.includes("--help") || rest.includes("-h")) {
    console.log(USAGE);
    return 0;
  }
  try {
    switch (command) {
      case "build":
        return await runBuild(parseBuild(rest));
      case "deploy":
        return await runDeploy(parseDeploy(rest));
      case "clean":
        return runClean(parseProjectArgs("clean", rest));
      case "package": {
        const options = parsePackage(rest);
        return runPackage(options.libsDir, options.preset);
      }
      case "package-check":
        if (rest.length !== 1 || rest[0].startsWith("-")) {
          throw new UsageError("package-check: expected exactly one zip path");
        }
        return runPackageCheck(rest[0]);
      case "test":
        return runTests(parseTest(rest));
      case "audit-tools":
        return runToolAudit(parseToolAudit(rest));
      case "jam":
        return runJam(parseJam(rest));
      case "harness": {
        const action = rest[0];
        const actionArgs = rest.slice(1);
        if (action === "setup") return await runHarnessSetup(parseHarnessSetup(actionArgs));
        if (action === "coverage") return runHarnessCoverage(parseHarnessCoverage(actionArgs));
        if (action === "digest") return runHarnessDigest(parseHarnessDigest(actionArgs));
        throw new UsageError(`harness: unknown subcommand: ${action ?? "(none)"}`);
      }
      case "tui":
        expectNoArgs("tui", rest);
        return runTui();
      default:
        throw new UsageError(`unknown command: ${command}`);
    }
  } catch (error) {
    if (error instanceof UsageError) {
      console.error(`[ERROR] ${error.message}`);
      console.error(USAGE);
      return 2;
    }
    throw error;
  }
}

if (import.meta.main) {
  try {
    process.exitCode = await main(process.argv.slice(2));
  } catch (error) {
    console.error(`[ERROR] ${error instanceof Error ? error.message : String(error)}`);
    process.exitCode = 1;
  }
}
