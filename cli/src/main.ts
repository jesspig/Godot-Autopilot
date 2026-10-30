import { parseArgv, usageText } from "./args";
import { COMMANDS } from "./commands/index";
import { CliError, ExitCode } from "./errors";
import { err } from "./output";
import { CLI_VERSION } from "./version";

function reportCliError(error: CliError): number {
  err(error.message);
  if (error.suggestion !== undefined) {
    process.stderr.write(`[gda] suggestion: ${error.suggestion}\n`);
  }
  return error.code;
}

export async function main(argv: string[]): Promise<number> {
  let parsed: ReturnType<typeof parseArgv>;
  try {
    parsed = parseArgv(argv);
  } catch (error) {
    if (error instanceof CliError) {
      const code = reportCliError(error);
      process.stderr.write(`${usageText()}\n`);
      return code;
    }
    throw error;
  }

  const { opts, positionals } = parsed;

  if (opts.help === true) {
    process.stdout.write(`${usageText()}\n`);
    return ExitCode.Success;
  }
  if (opts.version === true) {
    process.stdout.write(`gda ${CLI_VERSION}\n`);
    return ExitCode.Success;
  }

  const commandName = positionals[0];
  if (commandName === undefined) {
    err("missing command");
    process.stderr.write(`${usageText()}\n`);
    return ExitCode.Usage;
  }

  const command = COMMANDS[commandName];
  if (command === undefined) {
    err(`unknown command: ${commandName}`);
    process.stderr.write(`${usageText()}\n`);
    return ExitCode.Usage;
  }

  try {
    return await command.run(positionals.slice(1), opts);
  } catch (error) {
    if (error instanceof CliError) {
      return reportCliError(error);
    }
    throw error;
  }
}
