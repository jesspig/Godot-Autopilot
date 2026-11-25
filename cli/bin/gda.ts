#!/usr/bin/env bun
import { parseArgv } from "../src/args";
import {
  extractCommandTokens,
  helpTextForTokens,
  isGroup,
  parseCommandArgs,
  resolveCommand,
} from "../src/command-spec";
import { run as runManifest } from "../src/commands/manifest";
import { CliError, ExitCode } from "../src/errors";
import { main } from "../src/main";

const argv = process.argv.slice(2);
const { tokens, indexes } = extractCommandTokens(argv);
const rest = argv.filter((_, index) => !indexes.includes(index));

if (tokens[0] === "commands") {
  const { opts } = parseArgv(rest);
  process.exit(await runManifest([], opts));
}

if (argv.includes("--help")) {
  const help = helpTextForTokens(tokens);
  if (help !== undefined) {
    process.stdout.write(`${help}\n`);
    process.exit(ExitCode.Success);
  }
}

if (isGroup(tokens[0])) {
  const spec = resolveCommand(tokens.slice(0, 2));
  if (spec !== undefined) {
    try {
      parseCommandArgs(spec, rest);
    } catch (error) {
      if (error instanceof CliError) {
        process.stderr.write(`[gda] ${error.message}\n`);
        if (error.suggestion !== undefined) {
          process.stderr.write(`[gda] suggestion: ${error.suggestion}\n`);
        }
        const help = helpTextForTokens(tokens.slice(0, 2));
        if (help !== undefined) process.stderr.write(`${help}\n`);
        process.exit(error.code);
      }
      throw error;
    }
  }
}

process.exit(await main(argv));
