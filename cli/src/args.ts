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
  artifactText?: string;
  pathText?: string;
  sourceText?: string;
  zh?: boolean;
  check?: boolean;
  confirm?: boolean;
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
  probe?: string;
  file?: string;
  refresh?: boolean;
  all?: boolean;
  projectsView?: boolean;
  stale?: boolean;
  socketText?: string;
  noFallback?: boolean;
  transportText?: string;
  json?: boolean;
}

const STRING_FLAGS: Record<string, keyof GlobalOptions> = {
  "--port": "portText",
  "--id": "id",
  "--project": "project",
  "--timeout": "timeoutText",
  "--category": "category",
  "--tags": "tagsText",
  "--artifact": "artifactText",
  "--path": "pathText",
  "--source": "sourceText",
  "--args": "argsJson",
  "--args-file": "argsFile",
  "--save-images": "saveImages",
  "--godot": "godot",
  "--wait": "waitText",
  "--tail": "tailText",
  "--probe": "probe",
  "--file": "file",
  "--socket": "socketText",
  "--transport": "transportText",
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
  "--refresh": "refresh",
  "--all": "all",
  "--projects": "projectsView",
  "--stale": "stale",
  "--no-fallback": "noFallback",
  "--json": "json",
  "--zh": "zh",
  "--check": "check",
  "--confirm": "confirm",
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

export function parseProbePort(text: string): number {
  const value = text.trim();
  const port = /^\d+$/.test(value) ? Number(value) : Number.NaN;
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new CliError(ExitCode.Usage, `--probe for list must be a port in 1-65535, got: ${text}`);
  }
  return port;
}

export function parseTransport(text: string | undefined): "auto" | "ipc" | "http" {
  if (text === undefined) return "auto";
  if (text === "auto" || text === "ipc" || text === "http") return text;
  throw new CliError(ExitCode.Usage, `--transport must be auto, ipc, or http, got: ${text}`);
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
  skills list         List the bundled skill books (offline)
  skills show <name>  Print one skill book's SKILL.md; --file <rel> reads a reference
  tools list          List the cached tool catalog (offline)
  tools search <query> Search the cached tool catalog
  tools describe <name> Show one cached tool's schema
  projects            List known Godot projects (offline)
  start               Start a Godot editor
  stop                Stop a Godot editor (<id> | --id | --port | --project)
  restart             Restart a Godot editor with the same port and settings
  logs                Show an instance log
  cache list          List cache manifests
  cache info          Show cache root, entry count, and size
  cache clear         Remove old cache entries (requires --force)
  install skills      Install skill books to ~/.agents/skills
  install plugin      Install the Godot addon into a project
  update <target>     Update cli, skills, plugin, or all (--check for status)

Global options:
  --port <text>       MCP server port for connection only (never a listen port)
  --id <text>         Target instance id from the registry
  --project <text>    Project directory
  --pretty            Pretty-print JSON output (2-space indent)
  --raw               Raw JSON envelope output
  --timeout <text>    Call timeout in seconds
  --category <text>   Filter search results by category
  --tags <text>       Filter search results by tags (comma separated)
  --artifact <name>   Cache artifact: cli, plugin, skills, or catalog
  --path <dir>        Install target directory (install skills)
  --source <mode>     auto (default), online, cache, or builtin
  --zh                Install the Chinese skills archive
  --args <json>       Tool arguments as a JSON string
  --args-file <path>  Tool arguments from a JSON file
  --save-images <dir> Write returned images to a local directory
  --godot <path>      Explicit Godot executable (start)
  --headless          Start the editor headless (start)
  --play              Play the current scene once ready (start)
  --no-import         Skip the asset import pre-step (start/restart)
  --wait <sec>        Readiness/stop wait in seconds, 0 = do not wait (start/stop/restart)
  --force             Force-stop the process (stop/restart)
  --tail <n>          Number of trailing log lines (logs)
  --probe <value>     Liveness level light/strict (start); list: probed port 1-65535
  --file <rel>        Reference file to read with skills show (relative to the skill dir)
  --refresh           Refresh the offline tool catalog from MCP tools/list
  --all               list: include projects with no running instance
  --projects          list: aggregate entries by project instead of per instance
  --stale             list: keep only non-live entries
  --socket <path>     Connect through the plugin's local IPC socket
  --transport <mode>  auto (default), ipc, or http
  --no-fallback       Do not fall back to HTTP when IPC fails
  --json              JSON output (default for most commands)
  --confirm           Confirm a high-risk tool call
  --help              Show this help
  --version           Show the version

Exit codes:
  0  success
  1  tool failure
  2  transport or environment failure
  3  usage error`;
}