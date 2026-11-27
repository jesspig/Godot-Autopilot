import { parseArgv, usageText } from "./args";
import { COMMANDS } from "./commands/index";
import { CliError, ExitCode, errorMessage } from "./errors";
import { err, setActiveCommand, writeFailure, writeJson } from "./output";
import { applyUpdate } from "./update";
import { CLI_VERSION } from "./version";

function reportUsage(message: string, command = ""): number {
  const error = new CliError(ExitCode.Usage, message);
  writeFailure(command, error, {});
  process.stderr.write(`${usageText()}\n`);
  return ExitCode.Usage;
}

export async function main(argv: string[]): Promise<number> {
  if (argv[0] === "--apply-update") {
    return applyUpdate(argv.slice(1));
  }
  let parsed: ReturnType<typeof parseArgv>;
  try {
    parsed = parseArgv(argv);
  } catch (error) {
    if (error instanceof CliError) {
      return reportUsage(error.message);
    }
    throw error;
  }

  const { opts, positionals } = parsed;

  if (opts.help === true) {
    process.stdout.write(`${usageText()}\n`);
    return ExitCode.Success;
  }
  if (opts.version === true) {
    writeJson({ version: CLI_VERSION }, opts, "--version");
    return ExitCode.Success;
  }

  const commandName = positionals[0];
  if (commandName === undefined) {
    return reportUsage("missing command");
  }

  const command = COMMANDS[commandName];
  if (command === undefined) {
    return reportUsage(`unknown command: ${commandName}`);
  }

  setActiveCommand(commandName);
  try {
    return await command.run(positionals.slice(1), opts);
  } catch (error) {
    if (error instanceof CliError) {
      return writeFailure(commandName, error, opts);
    }
    err(errorMessage(error));
    return writeFailure(commandName, new CliError(ExitCode.ToolFailure, errorMessage(error)), opts);
  }
}
