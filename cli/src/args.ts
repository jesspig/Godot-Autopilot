import { CliError, ExitCode } from "./errors";
import { CLI_VERSION } from "./version";

export interface GlobalOptions {
  portText?: string;
  id?: string;
  project?: string;
  pretty?: boolean;
  raw?: boolean;
  timeoutText?: string;
  category?: string;
  tagsText?: string;
  argsJson?: string;
  argsFile?: string;
  saveImages?: string;
  help?: boolean;
  version?: boolean;
  godot?: string;
  waitText?: string;
  tailText?: string;
  headless?: boolean;
  play?: boolean;
  noImport?: boolean;
  force?: boolean;
  query?: string;
  probe?: string;
}

const STRING_FLAGS: Record<string, keyof GlobalOptions> = {
  "--port": "portText",
  "--id": "id",
  "--project": "project",
  "--timeout": "timeoutText",
  "--category": "category",
  "--tags": "tagsText",
  "--args": "argsJson",
  "--args-file": "argsFile",
  "--save-images": "saveImages",
  "--godot": "godot",
  "--wait": "waitText",
  "--tail": "tailText",
  "--query": "query",
  "--probe": "probe",
};

const BOOLEAN_FLAGS: Record<string, keyof GlobalOptions> = {
  "--pretty": "pretty",
  "--raw": "raw",
  "--help": "help",
  "--version": "version",
  "--headless": "headless",
  "--play": "play",
  "--no-import": "noImport",
  "--force": "force",
};

export function parseArgv(argv: string[]): { opts: GlobalOptions; positionals: string[] } {
  const opts: Record<string, string | boolean | undefined> = {};
  const positionals: string[] = [];
  let onlyPositionals = false;
  let i = 0;
  while (i < argv.length) {
    const token = argv[i];
    if (onlyPositionals) {
      positionals.push(token);
      i += 1;
    } else if (token === "--") {
      onlyPositionals = true;
      i += 1;
    } else if (token.startsWith("-")) {
      const eq = token.indexOf("=");
      const name = eq === -1 ? token : token.slice(0, eq);
      const stringKey = STRING_FLAGS[name];
      const boolKey = BOOLEAN_FLAGS[name];
      if (stringKey !== undefined) {
        let value: string;
        if (eq !== -1) {
          value = token.slice(eq + 1);
        } else {
          const next = argv[i + 1];
          if (next === undefined || next.startsWith("-")) {
            throw new CliError(ExitCode.Usage, `option ${name} requires a value`);
          }
          value = next;
          i += 1;
        }
        opts[stringKey] = value;
      } else if (boolKey !== undefined) {
        if (eq !== -1) {
          throw new CliError(ExitCode.Usage, `boolean option ${name} does not take a value`);
        }
        opts[boolKey] = true;
      } else {
        throw new CliError(ExitCode.Usage, `unknown option: ${name}`);
      }
      i += 1;
    } else {
      positionals.push(token);
      i += 1;
    }
  }
  return { opts: opts as GlobalOptions, positionals };
}

export function usageText(): string {
  return `gda ${CLI_VERSION} — Godot Autopilot CLI

Usage:
  gda [global options] <command> [args...]

Commands:
  ping                Check connectivity to the MCP server
  status              Show server status and project info
  categories          List tool categories
  search <query>      Search tools
  describe <tool>     Show one tool's schema and side effects
  call <tool>         Call a tool
  list                List running Godot instances
  start               Start a Godot editor
  stop                Stop a Godot editor (<id> | --id | --port | --project)
  logs                Show an instance log
  api list            List tools through the REST endpoint
  api describe <tool> Show one tool's schema through the REST endpoint
  api call <tool>     Call a tool through the REST endpoint
  endpoints           Show the discovered MCP and REST endpoints

Global options:
  --port <text>       MCP server port (default 9527)
  --id <text>         Target instance id from the registry
  --project <text>    Project directory
  --pretty            Pretty-print JSON output (2-space indent)
  --raw               Raw JSON envelope output
  --timeout <text>    Call timeout in seconds
  --category <text>   Filter search results by category
  --tags <text>       Filter search results by tags (comma separated)
  --args <json>       Tool arguments as a JSON string
  --args-file <path>  Tool arguments from a JSON file
  --save-images <dir> Write returned images to a local directory
  --godot <path>      Explicit Godot executable (start)
  --headless          Start the editor headless (start)
  --play              Play the current scene once ready (start)
  --no-import         Skip the asset import pre-step (start)
  --wait <sec>        Readiness/stop wait in seconds, 0 = do not wait (start/stop)
  --force             Force-stop the process (stop)
  --tail <n>          Number of trailing log lines (logs)
  --query <text>      Keyword filter for the api list subcommand
  --probe <level>     Liveness probe level: light (default) or strict
  --help              Show this help
  --version           Show the version

Exit codes:
  0  success
  1  tool failure
  2  transport or environment failure
  3  usage error`;
}
