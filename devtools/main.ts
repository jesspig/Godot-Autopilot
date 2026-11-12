#!/usr/bin/env bun
import { runBuild, type BuildOptions } from "./commands/build";
import { runClean } from "./commands/clean";
import { runDeploy, type DeployOptions } from "./commands/deploy";
import { runPackage } from "./commands/package";
import { runPackageCheck } from "./commands/package-check";
import { runTests, type TestOptions } from "./commands/test";

const USAGE = [
  "Usage: bun run gda-dev-tools <command> [options]",
  "",
  "Commands:",
  "  build                    [--release|--debug]",
  "  deploy                   [--release|--debug] [--demos 名称...]",
  "  clean",
  "  package                  [--libs-dir <dir>]",
  "  package-check <zip>",
  "  test                     [--preset debug|release] [--single <用例名>]",
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

function parseDeploy(args: string[]): DeployOptions {
  const options: DeployOptions = { release: false, debug: false, demos: null };
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--release") options.release = true;
    else if (arg === "--debug") options.debug = true;
    else if (arg === "--demos") {
      const names: string[] = [];
      while (index + 1 < args.length && !args[index + 1].startsWith("-")) {
        names.push(args[index + 1]);
        index += 1;
      }
      options.demos = names;
    } else throw new UsageError(`deploy: unrecognized argument: ${arg}`);
  }
  return options;
}

function parsePackage(args: string[]): { libsDir: string | null } {
  let libsDir: string | null = null;
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg !== "--libs-dir") throw new UsageError(`package: unrecognized argument: ${arg}`);
    const value = args[index + 1];
    if (value === undefined) {
      throw new UsageError("package: argument --libs-dir: expected one argument");
    }
    libsDir = value;
    index += 1;
  }
  return { libsDir };
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
        expectNoArgs("clean", rest);
        return runClean();
      case "package":
        return runPackage(parsePackage(rest).libsDir);
      case "package-check":
        if (rest.length !== 1 || rest[0].startsWith("-")) {
          throw new UsageError("package-check: expected exactly one zip path");
        }
        return runPackageCheck(rest[0]);
      case "test":
        return runTests(parseTest(rest));
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
