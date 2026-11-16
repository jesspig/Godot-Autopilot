#!/usr/bin/env bun
import { runBuild, type BuildOptions } from "./commands/build";
import { runClean } from "./commands/clean";
import { runDeploy, type DeployOptions } from "./commands/deploy";
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
  "  jam                      [--theme <id> | --list] [--out <dir>] [--json]",
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
  const options: JamOptions = { theme: null, out: null, list: false, json: false };
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--json") options.json = true;
    else if (arg === "--list") options.list = true;
    else if (arg === "--theme" || arg === "--out") {
      const value = args[index + 1];
      if (value === undefined || value === "" || value.startsWith("-")) {
        throw new UsageError(`jam: argument ${arg}: expected one argument`);
      }
      if (arg === "--theme") options.theme = value;
      else options.out = value;
      index += 1;
    } else throw new UsageError(`jam: unrecognized argument: ${arg}`);
  }
  if (options.list && (options.theme !== null || options.out !== null)) {
    throw new UsageError("jam: --list cannot be combined with --theme or --out");
  }
  return options;
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

try {
  process.exitCode = await main(process.argv.slice(2));
} catch (error) {
  console.error(`[ERROR] ${error instanceof Error ? error.message : String(error)}`);
  process.exitCode = 1;
}
