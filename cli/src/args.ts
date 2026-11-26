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
  tag?: string[];
  include?: string[];
  artifactText?: string;
  pathText?: string;
  sourceText?: string;
  offline?: boolean;
  withCsharpRuntime?: boolean;
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
  forceStop?: boolean;
  forceClear?: boolean;
  forceInstall?: boolean;
  probe?: string;
  file?: string;
  reportFile?: string;
  skillFile?: string;
  inputFile?: string;
  all?: boolean;
  allProjects?: boolean;
  allBuckets?: boolean;
  projectsView?: boolean;
  stale?: boolean;
  socketText?: string;
  formatText?: string;
  exportFormat?: string;
  slowText?: string;
  limitText?: string;
  saveText?: string;
  intervalText?: string;
  templates?: boolean;
  progress?: boolean;
  docsFile?: string;
  section?: string;
  member?: string;
  kind?: string;
  engine?: string;
  classFilter?: string;
  full?: boolean;
  prune?: boolean;
  legacyOutput?: boolean;
}

export interface ValueAlias {
  key: keyof GlobalOptions;
  value: string;
}

export const VALUE_ALIASES: Readonly<Record<string, ValueAlias>> = {
  "--json": { key: "formatText", value: "json" },
};

const STRING_FLAGS: Record<string, keyof GlobalOptions> = {
  "--port": "portText",
  "--id": "id",
  "--project": "project",
  "--timeout": "timeoutText",
  "--category": "category",
  "--tags": "tagsText",
  "--tag": "tag",
  "--include": "include",
  "--artifact": "artifactText",
  "--path": "pathText",
  "--source": "sourceText",
  "--args": "argsJson",
  "--args-file": "argsFile",
  "--save-images": "saveImages",
  "--godot": "godot",
  "--wait": "waitText",
  "--tail": "tailText",
  "--probe-level": "probe",
  "--probe-port": "probe",
  "--report-file": "file",
  "--skill-file": "file",
  "--input-file": "file",
  "--socket": "socketText",
  "--format": "formatText",
  "--export-format": "exportFormat",
  "--slow": "slowText",
  "--limit": "limitText",
  "--save": "saveText",
  "--interval": "intervalText",
  "--docs-file": "docsFile",
  "--section": "section",
  "--member": "member",
  "--kind": "kind",
  "--engine": "engine",
  "--class": "classFilter",
};

const BOOLEAN_FLAGS: Record<string, keyof GlobalOptions> = {
  "--pretty": "pretty",
  "--raw": "raw",
  "--help": "help",
  "--version": "version",
  "--headless": "headless",
  "--play": "play",
  "--no-import": "noImport",
  "--force-stop": "force",
  "--force-clear": "force",
  "--force-install": "force",
  "--all-projects": "all",
  "--all-buckets": "all",
  "--projects": "projectsView",
  "--stale": "stale",
  "--templates": "templates",
  "--progress": "progress",
  "--offline": "offline",
  "--with-csharp-runtime": "withCsharpRuntime",
  "--zh": "zh",
  "--check": "check",
  "--confirm": "confirm",
  "--full": "full",
  "--prune": "prune",
  "--legacy-output": "legacyOutput",
};

const REPEATABLE_FLAGS: ReadonlySet<keyof GlobalOptions> = new Set(["tag", "include"]);

export const GLOBAL_STRING_FLAGS: Readonly<Record<string, keyof GlobalOptions>> = STRING_FLAGS;
export const GLOBAL_BOOLEAN_FLAGS: Readonly<Record<string, keyof GlobalOptions>> = BOOLEAN_FLAGS;
export const REPEATABLE_OPTIONS: ReadonlySet<keyof GlobalOptions> = REPEATABLE_FLAGS;

export const GLOBAL_SCOPE_FLAGS: ReadonlySet<string> = new Set([
  "--help",
  "--version",
  "--format",
  "--json",
  "--pretty",
  "--raw",
  "--confirm",
  "--timeout",
  "--port",
  "--id",
  "--socket",
  "--project",
  "--args-file",
  "--tag",
  "--include",
]);

export function repeatableFlag(name: string): boolean {
  const key = STRING_FLAGS[name];
  return key !== undefined && REPEATABLE_FLAGS.has(key);
}

export function parseArgv(argv: string[]): { opts: GlobalOptions; positionals: string[] } {
  const opts: Record<string, string | boolean | string[] | undefined> = {};
  const positionals: string[] = [];
  const seen = new Set<string>();
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
      const alias = VALUE_ALIASES[name];
      const stringKey = STRING_FLAGS[name];
      const boolKey = BOOLEAN_FLAGS[name];
      if (alias === undefined && stringKey === undefined && boolKey === undefined) {
        throw new CliError(ExitCode.Usage, `unknown option: ${name}`);
      }
      const repeatable = alias === undefined && repeatableFlag(name);
      if (!repeatable && seen.has(name)) {
        throw new CliError(ExitCode.Usage, `option ${name} specified more than once`);
      }
      seen.add(name);
      if (alias !== undefined) {
        if (eq !== -1) {
          throw new CliError(ExitCode.Usage, `option ${name} does not take a value`);
        }
        (opts as Record<string, unknown>)[alias.key] = alias.value;
      } else if (stringKey !== undefined) {
        let value: string;
        if (eq !== -1) {
          value = token.slice(eq + 1);
        } else {
          const next = argv[i + 1];
          if (next === undefined || (next.startsWith("-") && next !== "-")) {
            throw new CliError(ExitCode.Usage, `option ${name} requires a value`);
          }
          value = next;
          i += 1;
        }
        if (repeatable) {
          const existing = opts[stringKey];
          opts[stringKey] = Array.isArray(existing) ? [...existing, value] : [value];
        } else {
          opts[stringKey] = value;
        }
      } else if (boolKey !== undefined) {
        if (eq !== -1) {
          throw new CliError(ExitCode.Usage, `boolean option ${name} does not take a value`);
        }
        opts[boolKey] = true;
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
    throw new CliError(ExitCode.Usage, `--probe-port for list must be a port in 1-65535, got: ${text}`);
  }
  return port;
}

export function probeLevel(text: string | undefined): "light" | "strict" {
  if (text === undefined || text === "light") return "light";
  if (text === "strict") return "strict";
  throw new CliError(ExitCode.Usage, `--probe-level must be light or strict, got: ${text}`);
}

export function usageText(): string {
  return `gda ${CLI_VERSION} — Godot Autopilot CLI

Usage:
  gda [global options] <command> [args...]

Commands:
  ping                Check native control-plane connectivity
  status              Show instance status and project info
  categories          List tool categories
  search <query>      Search tools
  describe <tool>     Show one tool's schema and side effects
  call <tool>         Call a tool
  batch --args-file <path>  Run multiple tool calls over one native IPC session (use - for stdin)
  list                List running Godot instances
  docs sync           Export engine docs into the local cache (always)
  docs status         Show the resolved engine, project pin, and cache state
  docs search <query> Search classes, members, and global utilities
  docs class <name>   Show one class (--section/--member/--full)
  docs member <class> <name> Show one member by name (--kind)
  docs clear          Remove cached docs (--engine <x.y> or --all-buckets --force-clear)
  skills list         List the bundled skill books (offline, --zh for Chinese)
  skills show <name>  Print one skill book's SKILL.md; --skill-file <rel> reads an auxiliary file
  tools list          List canonical tools from a live instance (--category/--tag)
  resources list      List project resources (--templates)
  resources templates List resource templates
  resources read <uri> Read one resource (--pretty/--raw/--save <dir>)
  resources watch <uri> Poll a resource and print changes (--interval sec)
  prompts list        List prompt templates
  prompts get <name>  Render one prompt (--args JSON)
  tasks list          List locally recorded tasks
  tasks get <id>      Get one task
  tasks wait <id>     Wait for a task to finish (--timeout)
  tasks cancel <id>   Cancel a task
  tasks clean         Remove terminal tasks from the local index
  projects            List known Godot projects (offline)
  start               Start a Godot editor
  stop                Stop a Godot editor (<id> | --id | --port | --project)
  restart             Restart a Godot editor with the same port and settings
  logs                Show an instance log
  cache list          List cache manifests
  cache info          Show cache root, entry count, and size
  cache clear         Remove old cache entries (requires --force-clear)
  trace list          List recent trace events (plugin + CLI)
  trace show <id>     Show all events with one trace id
  trace summary       Summarize latency, failures, retries, and top tools
  trace slow          List calls slower than --slow <ms> (default 1000)
  trace failures      List failed tool calls
  trace export        Export traces as jsonl or otlp (--export-format)
  monitor status      Show game monitor scope and recording directory
  monitor sessions    List recorded game monitor sessions
  monitor report <id> Analyze one session (or --input-file <path> for offline JSONL)
  monitor export <id> Copy one session JSONL into GDA home
  install skills      Install skill books to ~/.agents/skills (--prune removes retired books)
  install plugin      Install the Godot addon into a project
  update <target>     Update cli, skills, plugin, or all (--check for status)

Global options:
  --port <text>       Target connection port (never a listen port)
  --id <text>         Target instance id from the registry
  --project <text>    Project directory
  --socket <path>     Connect through the plugin's local IPC socket
  --format <json>     Machine output format; --json is the compatibility alias
  --json              Alias for --format json
  --pretty            Pretty-print JSON output (2-space indent)
  --raw               Raw business payload inside the envelope
  --timeout <text>    Call timeout in seconds
  --category <text>   Filter tool list/search results by category
  --tags <text>       Filter tool list/search results by tags (comma separated)
  --tag <text>        Repeatable tag filter
  --include <text>    Repeatable include filter
  --artifact <name>   Cache artifact: cli, plugin, or skills
  --path <dir>        Install target directory (install skills)
  --source <mode>     auto (default), online, cache, or builtin
  --offline           Never touch the network; use cache, then builtin
  --with-csharp-runtime  Install time: add the GdaCSharpRuntime autoload in C# projects
  --zh                Use the Chinese skills archive (install, skills list/show)
  --args <json>       Tool arguments as a JSON string
  --args-file <path>  Tool arguments from a JSON file (- reads stdin)
  --save-images <dir> Write returned images to a local directory
  --save <dir>        Write blobs to a directory
  --templates         resources list: include resource templates
  --interval <sec>    resources watch polling interval (default 1)
  --godot <path>      Explicit Godot executable (start)
  --headless          Start the editor headless (start)
  --play              Play the current scene once ready (start)
  --no-import         Skip the asset import pre-step (start/restart)
  --wait <sec>        Readiness/stop wait in seconds, 0 = do not wait (start/stop/restart)
  --force-stop        Force-stop the process (stop/restart)
  --force-clear       Confirm an explicit clear (docs clear, cache clear)
  --force-install     Overwrite existing managed files (install)
  --all-projects      Include projects with no running instance (list)
  --all-buckets       Remove every docs cache bucket (docs clear)
  --probe-level <level>  Liveness level light/strict (start)
  --probe-port <port> Probe a specific port (list)
  --report-file <path>  Write a debug report (debug report)
  --skill-file <rel>    Read an auxiliary skill file (skills show)
  --input-file <path>   Read offline monitor JSONL (monitor report)
  --export-format <fmt> Trace export format jsonl/otlp (trace export)
  --projects          list: aggregate entries by project instead of per instance
  --stale             list: keep only non-live entries
  --progress          call: print progress diagnostics to stderr
  --docs-file <path>  Read an external extension_api.json (bypasses the cache)
  --section <name>    docs class: section to show
  --member <text>     docs class: filter members by name or signature
  --kind <kind>       docs search/member: result kind
  --class <name>      docs search: restrict results to one class
  --engine <x.y>      docs clear: remove one cache bucket (stable uses x.y)
  --full              docs class: include every section
  --prune             install skills: remove retired skill directories
  --confirm           Approve the shown tool/target/risk/argument summary
  --help              Show this help
  --version           Show the version

Exit codes:
  0    success
  1    tool or business failure
  2    transport or environment failure
  3    usage error
  4    confirmation required (nothing was sent)
  5    user declined confirmation
  130  interrupted`;
}
