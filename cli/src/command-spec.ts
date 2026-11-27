import {
  GLOBAL_BOOLEAN_FLAGS,
  GLOBAL_SCOPE_FLAGS,
  GLOBAL_STRING_FLAGS,
  repeatableFlag,
  VALUE_ALIASES,
} from "./args";
import { CliError, ExitCode } from "./errors";

export type FlagType = "string" | "number" | "boolean";
export type RiskLevel = "none" | "low" | "high";

export interface CommandFlag {
  name: string;
  type: FlagType;
  scope: "global" | "command";
  description: string;
  required?: boolean;
  conflictsWith?: string[];
  repeatable?: boolean;
}

export interface CommandPositional {
  name: string;
  type: "string" | "number";
  required: boolean;
  description: string;
  repeatable?: boolean;
}

export interface CommandOutput {
  schema: string;
  envelope: boolean;
  description: string;
}

export interface CommandExample {
  argv: string;
  description: string;
}

export interface CommandSpec {
  group: string;
  subcommand: string;
  name: string;
  path: string;
  description: string;
  tools: string[];
  action: string;
  positional: CommandPositional[];
  flags: CommandFlag[];
  output: CommandOutput;
  risk: RiskLevel;
  long_running: boolean;
  examples: CommandExample[];
}

export interface CommandGroup {
  name: string;
  description: string;
}

export interface ParsedCommandArgs {
  spec: CommandSpec;
  options: Record<string, string | number | boolean | string[]>;
  positionals: string[];
}

export interface CommandTokens {
  tokens: string[];
  indexes: number[];
}

interface SpecInput {
  group: string;
  subcommand: string;
  description: string;
  tools?: string[];
  action?: string;
  positional?: CommandPositional[];
  flags?: CommandFlag[];
  outputSchema?: string;
  risk?: RiskLevel;
  long_running?: boolean;
  examples?: Array<string | CommandExample>;
}

function positional(name: string, description: string, required = true): CommandPositional {
  return { name, type: "string", required, description };
}

function stringFlag(name: string, description: string, conflictsWith?: string[]): CommandFlag {
  return conflictsWith === undefined
    ? { name, type: "string", scope: "command", description }
    : { name, type: "string", scope: "command", description, conflictsWith };
}

function numberFlag(name: string, description: string, conflictsWith?: string[]): CommandFlag {
  return conflictsWith === undefined
    ? { name, type: "number", scope: "command", description }
    : { name, type: "number", scope: "command", description, conflictsWith };
}

function boolFlag(name: string, description: string, conflictsWith?: string[]): CommandFlag {
  return conflictsWith === undefined
    ? { name, type: "boolean", scope: "command", description }
    : { name, type: "boolean", scope: "command", description, conflictsWith };
}

function repeatableStringFlag(name: string, description: string): CommandFlag {
  return { name, type: "string", scope: "command", description, repeatable: true };
}

function targetFlags(): CommandFlag[] {
  return [
    stringFlag("--id", "Target instance id"),
    stringFlag("--socket", "Target native IPC socket"),
    stringFlag("--port", "Target connection port"),
    stringFlag("--project", "Target project directory"),
  ];
}

function define(input: SpecInput): CommandSpec {
  const path = `${input.group}.${input.subcommand}`;
  const examples = (input.examples ?? [`gda ${input.group} ${input.subcommand}`]).map((example) =>
    typeof example === "string" ? { argv: example, description: "Example invocation" } : example,
  );
  return {
    group: input.group,
    subcommand: input.subcommand,
    name: input.subcommand,
    path,
    description: input.description,
    tools: input.tools ?? [],
    action: input.action ?? `gcp.${path}`,
    positional: input.positional ?? [],
    flags: input.flags ?? [],
    output: {
      schema: input.outputSchema ?? "gda.result.v1",
      envelope: true,
      description: "CLI envelope v1",
    },
    risk: input.risk ?? "none",
    long_running: input.long_running ?? false,
    examples,
  };
}

export const COMMAND_GROUPS: ReadonlyArray<CommandGroup> = [
  { name: "editor", description: "Editor and instance lifecycle" },
  { name: "project", description: "Project discovery and metadata" },
  { name: "tool", description: "Tool discovery and generic tool calls" },
  { name: "workflow", description: "Saved multi-step workflows" },
  { name: "docs", description: "Engine documentation cache and lookup" },
  { name: "debug", description: "Live editor inspection and evaluation" },
  { name: "logs", description: "Instance logs and log streams" },
  { name: "scene", description: "Scene opening, editing, and saving" },
  { name: "script", description: "Script reading and editing" },
  { name: "resource", description: "Project resources, templates, and prompts" },
  { name: "game", description: "Game run lifecycle" },
  { name: "test", description: "Test suites and runs" },
  { name: "job", description: "Asynchronous jobs" },
  { name: "trace", description: "Trace events and latency reports" },
  { name: "monitor", description: "Game monitor sessions and reports" },
  { name: "setup", description: "Environment and installation setup" },
  { name: "skill", description: "Bundled skill books" },
  { name: "plugin", description: "Godot addon management" },
  { name: "cache", description: "Local cache manifests and entries" },
  { name: "update", description: "CLI, skills, and plugin updates" },
  { name: "doctor", description: "Environment diagnostics" },
  { name: "config", description: "CLI configuration" },
  { name: "shell", description: "Shell-level execution" },
  { name: "completion", description: "Shell completion generation" },
];

export const COMMAND_SPECS: ReadonlyArray<CommandSpec> = [
  define({
    group: "editor",
    subcommand: "status",
    description: "Show native editor status",
    tools: ["system_status"],
    flags: targetFlags(),
  }),
  define({
    group: "editor",
    subcommand: "list",
    description: "List running Godot instances",
    tools: ["instance_list"],
    flags: [
      boolFlag("--all-projects", "Include projects with no running instance"),
      boolFlag("--projects", "Aggregate entries by project"),
      boolFlag("--stale", "Keep only non-live entries"),
      stringFlag("--probe-port", "Probe a specific port 1-65535"),
    ],
  }),
  define({
    group: "editor",
    subcommand: "start",
    description: "Start a Godot editor",
    tools: ["editor_start"],
    flags: [
      stringFlag("--godot", "Explicit Godot executable"),
      boolFlag("--headless", "Start headless"),
      boolFlag("--play", "Play the current scene when ready"),
      boolFlag("--no-import", "Skip the asset import pre-step"),
      stringFlag("--wait", "Readiness wait in seconds"),
      stringFlag("--probe-level", "Liveness level: light or strict"),
    ],
  }),
  define({
    group: "editor",
    subcommand: "stop",
    description: "Stop a Godot editor",
    tools: ["editor_stop"],
    positional: [positional("id", "Instance id", false)],
    flags: [
      stringFlag("--id", "Target instance id"),
      stringFlag("--port", "Target connection port"),
      stringFlag("--project", "Target project directory"),
      boolFlag("--force-stop", "Force-stop the process", ["--wait"]),
      stringFlag("--wait", "Stop wait in seconds", ["--force-stop"]),
      stringFlag("--socket", "Target native IPC socket"),
    ],
    risk: "high",
  }),
  define({
    group: "editor",
    subcommand: "restart",
    description: "Restart a Godot editor with the same settings",
    tools: ["editor_restart"],
    flags: [
      boolFlag("--force-stop", "Force-stop the process", ["--wait"]),
      stringFlag("--wait", "Stop wait in seconds", ["--force-stop"]),
      boolFlag("--no-import", "Skip the asset import pre-step"),
      ...targetFlags(),
    ],
    risk: "high",
  }),
  define({
    group: "editor",
    subcommand: "ping",
    description: "Check native connectivity to a running editor",
    tools: ["ping"],
    flags: targetFlags(),
  }),

  define({
    group: "project",
    subcommand: "list",
    description: "List known Godot projects",
    tools: ["project_list"],
    flags: [boolFlag("--all-projects", "Include projects without instances")],
  }),
  define({
    group: "project",
    subcommand: "info",
    description: "Show project metadata",
    tools: ["project_info"],
    positional: [positional("dir", "Project directory")],
  }),
  define({
    group: "project",
    subcommand: "open",
    description: "Open a project in an editor",
    tools: ["project_open"],
    positional: [positional("dir", "Project directory")],
    flags: [stringFlag("--godot", "Explicit Godot executable")],
    risk: "low",
  }),

  define({ group: "tool", subcommand: "ping", description: "Check connectivity to the control plane", tools: ["ping"] }),
  define({
    group: "tool",
    subcommand: "list",
    description: "List canonical tools from the native catalog",
    tools: ["tool_list"],
    flags: [
      stringFlag("--category", "Restrict by category"),
      stringFlag("--tags", "Restrict by tags"),
      boolFlag("--offline", "Use the hash-verified offline catalog only"),
      ...targetFlags(),
    ],
  }),
  define({
    group: "tool",
    subcommand: "categories",
    description: "List tool categories",
    tools: ["tool_categories"],
    flags: [boolFlag("--offline", "Use the hash-verified offline catalog only"), ...targetFlags()],
  }),
  define({
    group: "tool",
    subcommand: "search",
    description: "Search canonical tools",
    tools: ["tool_search"],
    positional: [positional("query", "Search query")],
    flags: [
      stringFlag("--category", "Restrict by category"),
      stringFlag("--tags", "Restrict by tags"),
      boolFlag("--offline", "Use the hash-verified offline catalog only"),
      ...targetFlags(),
    ],
  }),
  define({
    group: "tool",
    subcommand: "describe",
    description: "Show one tool schema and side effects",
    tools: ["tool_describe"],
    positional: [positional("tool", "Tool name")],
    flags: [boolFlag("--offline", "Use the hash-verified offline catalog only"), ...targetFlags()],
  }),
  define({
    group: "tool",
    subcommand: "call",
    description: "Call any canonical tool",
    tools: ["tool_call"],
    positional: [positional("tool", "Tool name")],
    flags: [
      stringFlag("--args", "Arguments as JSON", ["--args-file"]),
      stringFlag("--args-file", "Arguments from a JSON file (- reads stdin)", ["--args"]),
      boolFlag("--progress", "Print progress diagnostics to stderr"),
      boolFlag("--confirm", "Confirm the shown tool/target/risk/argument summary"),
      stringFlag("--timeout", "Call timeout in seconds"),
    ],
    risk: "high",
  }),
  define({
    group: "tool",
    subcommand: "batch",
    description: "Run multiple tool calls in one process",
    tools: ["tool_batch"],
    flags: [stringFlag("--args-file", "Batch arguments from a JSON file")],
    risk: "high",
  }),

  define({
    group: "workflow",
    subcommand: "run",
    description: "Run a saved workflow",
    tools: ["workflow_run"],
    positional: [positional("name", "Workflow name")],
    flags: [stringFlag("--args", "Workflow arguments as JSON")],
    risk: "high",
    long_running: true,
  }),
  define({ group: "workflow", subcommand: "list", description: "List saved workflows", tools: ["workflow_list"] }),
  define({
    group: "workflow",
    subcommand: "status",
    description: "Show a workflow run",
    tools: ["workflow_status"],
    positional: [positional("id", "Workflow run id")],
  }),

  define({
    group: "docs",
    subcommand: "sync",
    description: "Export engine docs into the local cache",
    tools: ["docs_sync"],
    flags: [stringFlag("--engine", "Engine version bucket"), boolFlag("--offline", "Never use the network")],
  }),
  define({ group: "docs", subcommand: "status", description: "Show docs cache and engine pin state", tools: ["docs_status"] }),
  define({
    group: "docs",
    subcommand: "search",
    description: "Search classes, members, and utilities",
    tools: ["docs_search"],
    positional: [positional("query", "Search query")],
    flags: [stringFlag("--kind", "Result kind"), stringFlag("--class", "Restrict to one class"), stringFlag("--engine", "Engine version bucket")],
  }),
  define({
    group: "docs",
    subcommand: "class",
    description: "Show one engine class",
    tools: ["docs_class"],
    positional: [positional("name", "Class name")],
    flags: [stringFlag("--section", "Section to show"), stringFlag("--member", "Member filter"), boolFlag("--full", "Include every section")],
  }),
  define({
    group: "docs",
    subcommand: "member",
    description: "Show one class member",
    tools: ["docs_member"],
    positional: [positional("class", "Class name"), positional("name", "Member name")],
    flags: [stringFlag("--kind", "Member kind")],
  }),
  define({
    group: "docs",
    subcommand: "clear",
    description: "Remove cached docs",
    tools: ["docs_clear"],
    flags: [
      stringFlag("--engine", "Remove one engine bucket", ["--all-buckets"]),
      boolFlag("--all-buckets", "Remove every bucket", ["--engine"]),
      boolFlag("--force-clear", "Confirm removal"),
    ],
    risk: "high",
  }),

  define({
    group: "debug",
    subcommand: "status",
    description: "Assemble editor, game, and trace status into one incident snapshot",
    tools: ["system_status", "get_editor_state", "get_game_state", "trace.get"],
    flags: targetFlags(),
  }),
  define({
    group: "debug",
    subcommand: "errors",
    description: "Read debugger errors and cross-check the on-disk game log",
    tools: ["get_debugger_stream", "get_game_log_entries"],
    flags: [numberFlag("--limit", "Maximum number of entries"), stringFlag("--level", "Minimum log level"), ...targetFlags()],
  }),
  define({
    group: "debug",
    subcommand: "output",
    description: "Read captured debugger stdout and stderr",
    tools: ["get_debugger_stream"],
    flags: [numberFlag("--limit", "Maximum number of entries"), ...targetFlags()],
  }),
  define({
    group: "debug",
    subcommand: "stack",
    description: "Read the debug stack and the running scene tree",
    tools: ["get_debug_stack", "get_debugger_scene_tree"],
    flags: targetFlags(),
  }),
  define({
    group: "debug",
    subcommand: "inspect",
    description: "Inspect live editor state",
    tools: ["get_debug_info", "get_debug_object_info"],
    flags: [
      stringFlag("--action", "Debug info action: object, node, memory, catalog, or monitors"),
      stringFlag("--path", "Object or node path"),
      ...targetFlags(),
    ],
  }),
  define({
    group: "debug",
    subcommand: "trace",
    description: "Read the live trace context and local trace summary",
    tools: ["trace.get"],
    flags: [stringFlag("--traceparent", "Explicit traceparent"), ...targetFlags()],
  }),
  define({
    group: "debug",
    subcommand: "repro",
    description: "Orchestrate play, input, sampling, capture, and error collection",
    tools: ["editor_scene_control", "game_input", "get_game_evidence", "capture_editor_viewport", "get_debugger_stream"],
    flags: [
      stringFlag("--path", "Scene path to play"),
      stringFlag("--target", "Capture target: editor or game"),
      boolFlag("--confirm", "Confirm the game runtime change"),
      boolFlag("--detach", "Return the task id immediately and stream progress to stderr"),
      numberFlag("--after-frames", "Wait for N rendered frames before capture"),
      stringFlag("--when", "GDScript condition for capture"),
      numberFlag("--timeout", "Step timeout in seconds"),
    ],
    risk: "high",
    long_running: true,
  }),
  define({
    group: "debug",
    subcommand: "report",
    description: "Bundle evidence from status, errors, trace, and monitor into one report",
    tools: ["get_debugger_stream", "get_game_log_entries", "trace.get", "game_monitor"],
    flags: [
      stringFlag("--label", "Incident label"),
      stringFlag("--report-file", "Write the report as JSON to this path"),
      stringFlag("--image", "Path of an existing PNG evidence image"),
      ...targetFlags(),
    ],
  }),
  define({
    group: "debug",
    subcommand: "attach",
    description: "Attach to a running instance",
    tools: ["get_debugger_stream"],
    positional: [positional("id", "Instance id")],
  }),
  define({
    group: "debug",
    subcommand: "eval",
    description: "Evaluate an expression in the editor",
    tools: ["execute_script"],
    positional: [positional("expression", "Expression to evaluate")],
    flags: [stringFlag("--timeout", "Evaluation timeout in seconds")],
    risk: "high",
  }),

  define({
    group: "logs",
    subcommand: "show",
    description: "Show an instance log",
    tools: ["logs_read"],
    flags: [
      numberFlag("--tail", "Number of trailing lines"),
      stringFlag("--id", "Target instance id"),
      stringFlag("--port", "Target connection port"),
      stringFlag("--project", "Target project directory"),
    ],
  }),
  define({ group: "logs", subcommand: "list", description: "List log streams", tools: ["logs_list"] }),
  define({
    group: "logs",
    subcommand: "editor",
    description: "Read the editor engine log with filtering and export",
    tools: ["os_file_text"],
    positional: [positional("id", "Instance id", false)],
    flags: [
      numberFlag("--tail", "Number of trailing lines (0 = none)"),
      stringFlag("--level", "Minimum level: debug, info, warning, error"),
      stringFlag("--source", "Source substring filter"),
      stringFlag("--since", "ISO time or epoch-ms lower bound"),
      boolFlag("--follow", "Keep polling for new lines"),
      numberFlag("--interval", "Follow polling interval in seconds"),
      stringFlag("--export", "Write the selected lines to a file"),
      stringFlag("--id", "Target instance id"),
      stringFlag("--project", "Target project directory"),
    ],
  }),
  define({
    group: "logs",
    subcommand: "game",
    description: "Read the on-disk game log through the native tool",
    tools: ["get_game_log_entries"],
    flags: [
      numberFlag("--limit", "Maximum number of entries (0 = none)"),
      numberFlag("--tail", "Number of trailing lines (0 = none)"),
      stringFlag("--level", "Minimum level"),
      stringFlag("--source", "Source substring filter"),
      stringFlag("--since", "ISO time or epoch-ms lower bound"),
      boolFlag("--follow", "Keep polling for new lines"),
      numberFlag("--interval", "Follow polling interval in seconds"),
      stringFlag("--export", "Write the selected lines to a file"),
      ...targetFlags(),
    ],
  }),
  define({
    group: "logs",
    subcommand: "plugin",
    description: "Read plugin diagnostics",
    tools: ["get_plugin_log"],
    flags: [
      numberFlag("--limit", "Maximum number of entries (0 = none)"),
      numberFlag("--tail", "Number of trailing lines (0 = none)"),
      stringFlag("--level", "Minimum level"),
      stringFlag("--source", "Source substring filter"),
      stringFlag("--since", "ISO time or epoch-ms lower bound"),
      boolFlag("--follow", "Keep polling for new lines"),
      numberFlag("--interval", "Follow polling interval in seconds"),
      stringFlag("--export", "Write the selected lines to a file"),
      ...targetFlags(),
    ],
  }),
  define({
    group: "logs",
    subcommand: "cli",
    description: "Read CLI-side trace and log records",
    tools: ["os_file_text"],
    flags: [
      numberFlag("--tail", "Number of trailing lines (0 = none)"),
      stringFlag("--level", "Minimum level"),
      stringFlag("--source", "Source substring filter"),
      stringFlag("--since", "ISO time or epoch-ms lower bound"),
      boolFlag("--follow", "Keep polling for new lines"),
      numberFlag("--interval", "Follow polling interval in seconds"),
      stringFlag("--export", "Write the selected lines to a file"),
    ],
  }),

  define({ group: "scene", subcommand: "list", description: "List open scenes", tools: ["get_editor_state"] }),
  define({
    group: "scene",
    subcommand: "open",
    description: "Open a scene in the editor",
    tools: ["editor_scene_control"],
    positional: [positional("path", "Scene path", false)],
    flags: [stringFlag("--path", "Scene path")],
    risk: "low",
  }),
  define({
    group: "scene",
    subcommand: "add",
    description: "Add a node to a scene",
    tools: ["create_scene_node"],
    positional: [positional("node", "Node type", false)],
    flags: [stringFlag("--type", "Node type"), stringFlag("--parent", "Parent node path")],
    risk: "low",
  }),
  define({ group: "scene", subcommand: "save", description: "Save the current scene", tools: ["save_editor_scene"], flags: [stringFlag("--path", "Explicit save path")], risk: "low" }),
  define({
    group: "scene",
    subcommand: "tree",
    description: "Read the edited scene tree",
    tools: ["get_scene_tree"],
    flags: [
      numberFlag("--max-depth", "Maximum tree depth (-1 for unlimited)"),
      stringFlag("--pattern", "Filter descendants by path pattern"),
      stringFlag("--type", "Filter descendants by node type"),
      stringFlag("--group", "Filter descendants by group"),
      boolFlag("--include-properties", "Include filtered properties per node"),
      boolFlag("--include-groups", "Include group membership"),
      boolFlag("--include-hash", "Include FNV-1a hashes of node paths"),
    ],
  }),
  define({
    group: "scene",
    subcommand: "node",
    description: "Create a node in the edited scene",
    tools: ["create_scene_node"],
    flags: [
      stringFlag("--type", "Node type"),
      stringFlag("--name", "Node name"),
      stringFlag("--parent", "Parent node path"),
      stringFlag("--props", "Node properties as JSON"),
      stringFlag("--args-file", "Node properties from a JSON file (use - for stdin)", ["--props"]),
    ],
    risk: "low",
  }),
  define({
    group: "scene",
    subcommand: "property",
    description: "Set one property on a scene node",
    tools: ["property_set"],
    flags: [
      stringFlag("--path", "Node path"),
      stringFlag("--property", "Property name"),
      stringFlag("--value", "Property value as JSON", ["--args-file"]),
      stringFlag("--type-hint", "Explicit Variant type hint"),
      stringFlag("--args-file", "Properties from a JSON file (use - for stdin)", ["--value"]),
    ],
    risk: "low",
  }),
  define({
    group: "scene",
    subcommand: "signal",
    description: "Connect or disconnect a scene signal",
    tools: ["signal_connect", "signal_disconnect"],
    flags: [
      stringFlag("--from", "Source node path"),
      stringFlag("--signal", "Signal name"),
      stringFlag("--to", "Target node path"),
      stringFlag("--method", "Target method"),
      boolFlag("--disconnect", "Disconnect instead of connect"),
      boolFlag("--no-persist", "Do not persist the connection"),
    ],
    risk: "low",
  }),
  define({
    group: "scene",
    subcommand: "verify",
    description: "Compare the edited scene tree with its saved file",
    tools: ["get_scene_tree"],
    flags: [numberFlag("--max-depth", "Maximum tree depth (-1 for unlimited)"), stringFlag("--pattern", "Filter by path pattern")],
  }),
  define({
    group: "scene",
    subcommand: "diff",
    description: "Report scene tree differences with path hashes",
    tools: ["get_scene_tree"],
    flags: [numberFlag("--max-depth", "Maximum tree depth (-1 for unlimited)"), stringFlag("--args-file", "Extra arguments from a JSON file (use - for stdin)")],
  }),

  define({ group: "script", subcommand: "list", description: "List project scripts", tools: ["get_editor_files"] }),
  define({
    group: "script",
    subcommand: "read",
    description: "Read a project script",
    tools: ["os_file_text"],
    positional: [positional("path", "Script path")],
  }),
  define({
    group: "script",
    subcommand: "create",
    description: "Create a project script",
    tools: ["create_script"],
    positional: [positional("path", "Script path")],
    flags: [stringFlag("--extends", "Base class"), stringFlag("--source", "Script source")],
    risk: "low",
  }),
  define({
    group: "script",
    subcommand: "delete",
    description: "Delete a project script",
    tools: ["resource_file"],
    positional: [positional("path", "Script path")],
    flags: [boolFlag("--confirm", "Confirm deletion")],
    risk: "high",
  }),
  define({
    group: "script",
    subcommand: "patch",
    description: "Patch a project script and compile it",
    tools: ["patch_script"],
    flags: [
      stringFlag("--path", "Script path"),
      boolFlag("--dry-run", "Validate and compile without writing"),
      stringFlag("--args-file", "Patch arguments from a JSON file (use - for stdin)"),
    ],
    risk: "low",
  }),
  define({
    group: "script",
    subcommand: "compile",
    description: "Compile-check a script without writing it",
    tools: ["patch_script"],
    flags: [stringFlag("--path", "Script path"), stringFlag("--args-file", "Patch arguments from a JSON file (use - for stdin)")],
  }),
  define({
    group: "script",
    subcommand: "attach",
    description: "Attach a script to a scene node",
    tools: ["attach_script_to_node"],
    flags: [stringFlag("--node", "Node path"), stringFlag("--script", "Script path")],
    risk: "low",
  }),
  define({
    group: "script",
    subcommand: "exec",
    description: "Run GDScript synchronously in the editor",
    tools: ["execute_script"],
    flags: [
      stringFlag("--source", "GDScript source", ["--args-file"]),
      stringFlag("--args-file", "Execution arguments from a JSON file (use - for stdin)", ["--source"]),
      boolFlag("--confirm", "Confirm this high-risk execution"),
    ],
    risk: "high",
  }),
  define({
    group: "script",
    subcommand: "reload",
    description: "Reload a script from disk",
    tools: ["reload_script"],
    flags: [stringFlag("--path", "Script path"), boolFlag("--keep-state", "Keep static state")],
    risk: "low",
  }),
  define({
    group: "script",
    subcommand: "test",
    description: "Run inline or file-based GDScript tests",
    tools: ["run_gdscript_tests"],
    flags: [
      stringFlag("--directory", "Test directory"),
      stringFlag("--pattern", "File pattern"),
      stringFlag("--timeout", "Test timeout in seconds"),
      stringFlag("--args-file", "Test cases from a JSON file (use - for stdin)"),
    ],
  }),

  define({
    group: "resource",
    subcommand: "list",
    description: "List project resources",
    tools: ["resource_query"],
  }),
  define({ group: "resource", subcommand: "templates", description: "List resource templates", tools: ["resource_query"] }),
  define({
    group: "resource",
    subcommand: "read",
    description: "Read one resource",
    tools: ["resource_load"],
    positional: [positional("uri", "Resource uri")],
    flags: [stringFlag("--save", "Write blobs to a directory")],
  }),
  define({
    group: "resource",
    subcommand: "watch",
    description: "Poll a resource and print changes",
    tools: ["resource_query"],
    positional: [positional("uri", "Resource uri")],
    flags: [numberFlag("--interval", "Polling interval in seconds")],
    long_running: true,
  }),
  define({
    group: "resource",
    subcommand: "prompts",
    description: "List or render prompt templates",
    tools: ["os_file_text"],
    positional: [positional("name", "Prompt name", false)],
    flags: [stringFlag("--args", "Prompt arguments as JSON")],
  }),
  define({
    group: "resource",
    subcommand: "inspect",
    description: "Inspect a resource type and metadata",
    tools: ["resource_query"],
    flags: [stringFlag("--path", "Resource path"), stringFlag("--uid", "Resource uid")],
  }),
  define({
    group: "resource",
    subcommand: "get",
    description: "Read one resource property",
    tools: ["resource_property"],
    flags: [stringFlag("--path", "Resource path"), stringFlag("--name", "Registered memory name"), stringFlag("--property", "Property name")],
  }),
  define({
    group: "resource",
    subcommand: "set",
    description: "Write one resource property",
    tools: ["resource_property"],
    flags: [
      stringFlag("--path", "Resource path"),
      stringFlag("--name", "Registered memory name"),
      stringFlag("--property", "Property name"),
      stringFlag("--value", "Value as JSON", ["--args-file"]),
      stringFlag("--args-file", "Arguments from a JSON file (use - for stdin)", ["--value"]),
    ],
    risk: "low",
  }),
  define({
    group: "resource",
    subcommand: "save",
    description: "Save a resource to disk",
    tools: ["resource_save"],
    flags: [stringFlag("--path", "Resource path"), stringFlag("--name", "Registered memory name"), stringFlag("--object-id", "Object id"), stringFlag("--dest", "Destination path")],
    risk: "low",
  }),
  define({
    group: "resource",
    subcommand: "import",
    description: "Reimport one or more assets",
    tools: ["resource_import"],
    flags: [stringFlag("--path", "Asset path"), stringFlag("--args-file", "Asset paths from a JSON file (use - for stdin)")],
    risk: "low",
  }),
  define({
    group: "resource",
    subcommand: "rename",
    description: "Rename a resource and rewrite references",
    tools: ["resource_file"],
    flags: [stringFlag("--path", "Resource path"), stringFlag("--to", "New path"), boolFlag("--confirm", "Confirm the rename")],
    risk: "high",
  }),
  define({
    group: "resource",
    subcommand: "move",
    description: "Move a resource and rewrite references",
    tools: ["resource_file"],
    flags: [stringFlag("--path", "Resource path"), stringFlag("--to", "New directory"), boolFlag("--confirm", "Confirm the move")],
    risk: "high",
  }),
  define({
    group: "resource",
    subcommand: "references",
    description: "Find references to a resource",
    tools: ["resource_query"],
    flags: [stringFlag("--path", "Resource path"), stringFlag("--uid", "Resource uid"), stringFlag("--class", "Resource class name")],
  }),

  define({
    group: "game",
    subcommand: "start",
    description: "Run the game",
    tools: ["editor_scene_control"],
    flags: [boolFlag("--headless", "Run headless"), boolFlag("--play", "Play the current scene")],
  }),
  define({ group: "game", subcommand: "stop", description: "Stop the game", tools: ["editor_scene_control"], flags: [boolFlag("--confirm", "Confirm the game runtime change")], risk: "high" }),
  define({ group: "game", subcommand: "status", description: "Show game run status", tools: ["get_game_state"] }),
  define({
    group: "game",
    subcommand: "play",
    description: "Play the edited scene",
    tools: ["editor_scene_control"],
    flags: [stringFlag("--path", "Scene path to open first"), boolFlag("--confirm", "Confirm the game runtime change")],
    risk: "high",
  }),
  define({
    group: "game",
    subcommand: "input",
    description: "Queue one input event in the running game",
    tools: ["game_input"],
    flags: [
      stringFlag("--type", "Input type: key, mouse_button, wheel, mouse_motion, action"),
      stringFlag("--mode", "Input mode: event, api, or hold"),
      stringFlag("--event", "Input event as JSON"),
      numberFlag("--duration", "Hold duration in milliseconds"),
      stringFlag("--args-file", "Input arguments from a JSON file (use - for stdin)"),
      boolFlag("--confirm", "Confirm the game runtime change"),
    ],
    risk: "high",
  }),
  define({
    group: "game",
    subcommand: "capture",
    description: "Capture an editor or game viewport image",
    tools: ["capture_editor_viewport"],
    flags: [
      stringFlag("--target", "Capture target: editor or game"),
      boolFlag("--save", "Write the PNG to the capture folder"),
      stringFlag("--dir", "Save directory"),
      numberFlag("--after-frames", "Wait for N rendered frames"),
      stringFlag("--when", "GDScript condition expression"),
      numberFlag("--scale", "Nearest-neighbour upscale factor"),
      stringFlag("--timeout", "Capture timeout in seconds"),
    ],
    risk: "low",
  }),
  define({
    group: "game",
    subcommand: "eval",
    description: "Run GDScript inside the running game",
    tools: ["game_script"],
    flags: [
      stringFlag("--source", "GDScript source", ["--args-file"]),
      stringFlag("--args-file", "Execution arguments from a JSON file (use - for stdin)", ["--source"]),
      stringFlag("--timeout", "Execution timeout in seconds"),
      boolFlag("--confirm", "Confirm the game runtime change"),
    ],
    risk: "high",
  }),
  define({
    group: "game",
    subcommand: "scenario",
    description: "Fire an input timeline in the running game",
    tools: ["game_input"],
    flags: [
      stringFlag("--args-file", "Timeline arguments from a JSON file (use - for stdin)"),
      stringFlag("--timeout", "Execution timeout in seconds"),
      boolFlag("--confirm", "Confirm the game runtime change"),
    ],
    risk: "high",
  }),

  define({ group: "test", subcommand: "list", description: "List test suites", tools: ["resource_query"] }),
  define({
    group: "test",
    subcommand: "run",
    description: "Run a test suite with labels, sharding, retries, and JUnit output",
    tools: ["run_gdscript_tests"],
    positional: [positional("suite", "Suite name", false)],
    flags: [
      stringFlag("--label", "Only run cases carrying this label"),
      boolFlag("--changed", "Only run cases touching changed files"),
      boolFlag("--failed-first", "Run previously failed cases first"),
      stringFlag("--shard", "Shard selector: index/count"),
      numberFlag("--retries", "Retry count for failed cases"),
      stringFlag("--junit", "Write a JUnit XML report to this path"),
      boolFlag("--coverage", "Include coverage data in the report"),
      boolFlag("--detach", "Return the task id immediately and stream progress to stderr"),
      stringFlag("--directory", "Test directory"),
      stringFlag("--pattern", "File pattern"),
      stringFlag("--timeout", "Run timeout in seconds"),
    ],
    long_running: true,
  }),
  define({
    group: "test",
    subcommand: "script",
    description: "Run inline GDScript test cases",
    tools: ["run_gdscript_tests"],
    flags: [
      stringFlag("--source", "Inline test source", ["--args-file"]),
      stringFlag("--args-file", "Test cases from a JSON file (use - for stdin)", ["--source"]),
      stringFlag("--label", "Only run cases carrying this label"),
      boolFlag("--changed", "Only run cases touching changed files"),
      boolFlag("--failed-first", "Run previously failed cases first"),
      stringFlag("--shard", "Shard selector: index/count"),
      numberFlag("--retries", "Retry count for failed cases"),
      stringFlag("--junit", "Write a JUnit XML report to this path"),
      boolFlag("--coverage", "Include coverage data in the report"),
      boolFlag("--detach", "Return the task id immediately and stream progress to stderr"),
      stringFlag("--timeout", "Run timeout in seconds"),
    ],
    long_running: true,
  }),
  define({
    group: "test",
    subcommand: "scenario",
    description: "Fire an input timeline and collect the resulting test evidence",
    tools: ["game_input", "get_debugger_stream"],
    flags: [
      stringFlag("--args-file", "Timeline arguments from a JSON file (use - for stdin)"),
      stringFlag("--label", "Scenario label"),
      numberFlag("--retries", "Retry count for the scenario"),
      stringFlag("--junit", "Write a JUnit XML report to this path"),
      boolFlag("--coverage", "Include coverage data in the report"),
      boolFlag("--confirm", "Confirm the game runtime change"),
      boolFlag("--detach", "Return the task id immediately and stream progress to stderr"),
      stringFlag("--timeout", "Execution timeout in seconds"),
    ],
    risk: "high",
    long_running: true,
  }),
  define({
    group: "test",
    subcommand: "watch",
    description: "Poll game state and test results until the run finishes",
    tools: ["get_game_state", "run_gdscript_tests"],
    flags: [
      numberFlag("--interval", "Polling interval in seconds"),
      stringFlag("--junit", "Write a JUnit XML report to this path"),
      boolFlag("--coverage", "Include coverage data in the report"),
      boolFlag("--detach", "Return the task id immediately and stream progress to stderr"),
      stringFlag("--timeout", "Watch timeout in seconds"),
    ],
    long_running: true,
  }),
  define({
    group: "test",
    subcommand: "status",
    description: "Show a test run",
    tools: ["run_gdscript_tests"],
    positional: [positional("id", "Run id")],
  }),

  define({ group: "job", subcommand: "list", description: "List jobs", tools: ["task.list"], flags: [numberFlag("--generation", "Only show one generation")] }),
  define({ group: "job", subcommand: "get", description: "Get one job", tools: ["task.get"], positional: [positional("id", "Job id")], flags: [numberFlag("--generation", "Binding generation")] }),
  define({
    group: "job",
    subcommand: "status",
    description: "Show one job with instance and generation binding",
    tools: ["task.get"],
    positional: [positional("id", "Job id")],
    flags: [numberFlag("--generation", "Binding generation"), ...targetFlags()],
  }),
  define({
    group: "job",
    subcommand: "wait",
    description: "Wait for a job to finish with reconnect and preserved terminal state",
    tools: ["task.get"],
    positional: [positional("id", "Job id")],
    flags: [
      stringFlag("--timeout", "Wait timeout in seconds"),
      numberFlag("--generation", "Binding generation"),
      numberFlag("--retries", "Reconnect attempts after a dropped read"),
      boolFlag("--progress", "Write NDJSON progress lines to stderr"),
      boolFlag("--detach", "Return the task id immediately and stream progress to stderr"),
      ...targetFlags(),
    ],
    long_running: true,
  }),
  define({
    group: "job",
    subcommand: "cancel",
    description: "Cancel a job and report the actual cancel stage",
    tools: ["task.cancel"],
    positional: [positional("id", "Job id")],
    flags: [
      boolFlag("--confirm", "Confirm cancellation"),
      numberFlag("--generation", "Binding generation"),
      stringFlag("--reason", "Cancellation reason"),
      ...targetFlags(),
    ],
    risk: "high",
  }),
  define({
    group: "job",
    subcommand: "logs",
    description: "Resume job events from an event cursor",
    tools: ["events.subscribe", "events.resume"],
    positional: [positional("id", "Job id", false)],
    flags: [
      stringFlag("--task-id", "Task id filter"),
      numberFlag("--cursor", "Event cursor (0 = from the beginning)"),
      numberFlag("--limit", "Maximum events per page (0 = none)"),
      stringFlag("--event-type", "Event type filter"),
      boolFlag("--detach", "Fetch one page and return immediately"),
      ...targetFlags(),
    ],
  }),
  define({
    group: "job",
    subcommand: "clean",
    description: "Remove terminal jobs",
    tools: ["job_clean"],
    flags: [boolFlag("--force-clear", "Confirm removal")],
    risk: "high",
  }),

  define({
    group: "trace",
    subcommand: "list",
    description: "List recent trace events",
    tools: ["trace_list"],
    flags: [
      numberFlag("--limit", "Page size (0 = none)"),
      numberFlag("--cursor", "Event cursor offset"),
    ],
  }),
  define({
    group: "trace",
    subcommand: "show",
    description: "Show all events of one trace",
    tools: ["trace_show"],
    positional: [positional("id", "Trace id")],
    flags: [numberFlag("--limit", "Page size (0 = none)"), numberFlag("--cursor", "Event cursor offset")],
  }),
  define({ group: "trace", subcommand: "summary", description: "Summarize latency and failures", tools: ["trace_summary"] }),
  define({
    group: "trace",
    subcommand: "slow",
    description: "List slow calls",
    tools: ["trace_slow"],
    flags: [
      numberFlag("--slow", "Threshold in milliseconds"),
      numberFlag("--limit", "Page size (0 = none)"),
      numberFlag("--cursor", "Event cursor offset"),
    ],
  }),
  define({
    group: "trace",
    subcommand: "failures",
    description: "List failed calls",
    tools: ["trace_failures"],
    flags: [numberFlag("--limit", "Page size (0 = none)"), numberFlag("--cursor", "Event cursor offset")],
  }),
  define({
    group: "trace",
    subcommand: "export",
    description: "Export traces",
    tools: ["trace_export"],
    flags: [stringFlag("--export-format", "Export format: jsonl or otlp")],
  }),

  define({
    group: "monitor",
    subcommand: "status",
    description: "Show monitor scope and recording directory",
    tools: ["game_monitor"],
    flags: [...targetFlags()],
  }),
  define({
    group: "monitor",
    subcommand: "sessions",
    description: "List recorded monitor sessions",
    tools: ["game_monitor"],
    flags: [numberFlag("--limit", "Page size (0 = none)"), numberFlag("--cursor", "Session cursor offset"), ...targetFlags()],
  }),
  define({
    group: "monitor",
    subcommand: "report",
    description: "Analyze one monitor session",
    tools: ["game_monitor"],
    positional: [positional("id", "Session id", false)],
    flags: [stringFlag("--input-file", "Offline JSONL path"), numberFlag("--slow", "Slow frame threshold in ms"), ...targetFlags()],
  }),
  define({
    group: "monitor",
    subcommand: "export",
    description: "Copy a monitor session into GDA home",
    tools: ["game_monitor"],
    positional: [positional("id", "Session id")],
    flags: [stringFlag("--image", "Attach a PNG evidence image"), ...targetFlags()],
  }),

  define({
    group: "setup",
    subcommand: "init",
    description: "Initialize GDA in a project",
    tools: ["setup_init"],
    positional: [positional("dir", "Project directory", false)],
    flags: [boolFlag("--with-csharp-runtime", "Add the C# runtime autoload")],
    risk: "low",
  }),
  define({
    group: "setup",
    subcommand: "install",
    description: "Install skills or the Godot plugin",
    tools: ["setup_install"],
    flags: [
      stringFlag("--artifact", "Artifact to install: cli, plugin, or skills"),
      stringFlag("--path", "Install target directory"),
      boolFlag("--zh", "Use the Chinese skills archive"),
      boolFlag("--prune", "Remove retired skill directories"),
      boolFlag("--with-csharp-runtime", "Add the C# runtime autoload"),
    ],
    risk: "high",
  }),
  define({ group: "setup", subcommand: "status", description: "Show setup state", tools: ["setup_status"] }),

  define({
    group: "skill",
    subcommand: "list",
    description: "List bundled skill books",
    tools: ["skill_list"],
    flags: [boolFlag("--zh", "Use the Chinese archive"), boolFlag("--offline", "Never use the network")],
  }),
  define({
    group: "skill",
    subcommand: "show",
    description: "Print one skill book",
    tools: ["skill_show"],
    positional: [positional("name", "Skill book name")],
    flags: [stringFlag("--skill-file", "Auxiliary file path"), boolFlag("--zh", "Use the Chinese archive")],
  }),
  define({
    group: "skill",
    subcommand: "install",
    description: "Install skill books",
    tools: ["skill_install"],
    flags: [stringFlag("--path", "Install target directory"), boolFlag("--zh", "Use the Chinese archive"), boolFlag("--prune", "Remove retired books")],
    risk: "high",
  }),
  define({
    group: "skill",
    subcommand: "sync",
    description: "Preview or apply managed skill-tree upgrades",
    tools: ["skill_sync"],
    flags: [
      stringFlag("--path", "Install target directory"),
      boolFlag("--zh", "Use the Chinese archive"),
      boolFlag("--dry-run", "Preview changes without writing"),
      boolFlag("--force-install", "Force a managed repair"),
      boolFlag("--prune", "Remove retired books"),
    ],
    risk: "high",
  }),

  define({ group: "plugin", subcommand: "list", description: "List installed plugins", tools: ["plugin_list"] }),
  define({
    group: "plugin",
    subcommand: "install",
    description: "Install the Godot addon",
    tools: ["plugin_install"],
    positional: [positional("dir", "Project directory", false)],
    flags: [boolFlag("--with-csharp-runtime", "Add the C# runtime autoload")],
    risk: "high",
  }),
  define({
    group: "plugin",
    subcommand: "enable",
    description: "Enable a plugin",
    tools: ["plugin_enable"],
    positional: [positional("name", "Plugin name")],
    risk: "low",
  }),

  define({ group: "cache", subcommand: "list", description: "List cache manifests", tools: ["cache_list"] }),
  define({ group: "cache", subcommand: "info", description: "Show cache root, count, and size", tools: ["cache_info"] }),
  define({
    group: "cache",
    subcommand: "clear",
    description: "Remove old cache entries",
    tools: ["cache_clear"],
    flags: [boolFlag("--force-clear", "Confirm removal")],
    risk: "high",
  }),

  define({
    group: "update",
    subcommand: "check",
    description: "Check for available updates",
    tools: ["update_check"],
    flags: [boolFlag("--offline", "Never use the network")],
  }),
  define({
    group: "update",
    subcommand: "run",
    description: "Update cli, skills, plugin, or all",
    tools: ["update_run"],
    positional: [positional("target", "Update target: cli, skills, plugin, or all")],
    flags: [boolFlag("--check", "Report status without applying"), boolFlag("--force-install", "Force the update")],
    risk: "high",
  }),

  define({
    group: "doctor",
    subcommand: "run",
    description: "Run environment diagnostics",
    tools: ["doctor_run"],
    flags: [boolFlag("--offline", "Never use the network"), boolFlag("--ci", "Stable machine-readable checks with codes and remediation")],
  }),
  define({
    group: "doctor",
    subcommand: "diagnose",
    description: "Diagnose one target instance",
    tools: ["doctor_diagnose"],
    flags: [boolFlag("--ci", "Stable machine-readable checks with codes and remediation"), ...targetFlags()],
  }),

  define({ group: "config", subcommand: "list", description: "List CLI configuration", tools: ["config_list"] }),
  define({
    group: "config",
    subcommand: "get",
    description: "Read one configuration key",
    tools: ["config_get"],
    positional: [positional("key", "Configuration key")],
  }),
  define({
    group: "config",
    subcommand: "set",
    description: "Write one configuration key",
    tools: ["config_set"],
    positional: [positional("key", "Configuration key"), positional("value", "New value")],
    risk: "low",
  }),

  define({
    group: "shell",
    subcommand: "run",
    description: "Run a shell session",
    tools: ["shell_run"],
    positional: [positional("command", "Command to execute", false)],
    flags: [stringFlag("--timeout", "Command timeout in seconds"), stringFlag("--protocol", "Shell protocol: ndjson")],
    risk: "high",
    long_running: true,
  }),

  define({ group: "completion", subcommand: "bash", description: "Print bash completion from the command spec", tools: ["completion_bash"] }),
  define({ group: "completion", subcommand: "zsh", description: "Print zsh completion from the command spec", tools: ["completion_zsh"] }),
  define({ group: "completion", subcommand: "fish", description: "Print fish completion from the command spec", tools: ["completion_fish"] }),
];

export const LEGACY_ALIASES: ReadonlyArray<readonly [string, string]> = [
  ["ping", "editor.ping"],
  ["diagnose", "doctor.diagnose"],
  ["status", "editor.status"],
  ["categories", "tool.categories"],
  ["search", "tool.search"],
  ["describe", "tool.describe"],
  ["call", "tool.call"],
  ["batch", "tool.batch"],
  ["list", "editor.list"],
  ["start", "editor.start"],
  ["stop", "editor.stop"],
  ["restart", "editor.restart"],
  ["logs", "logs.show"],
  ["docs", "docs.status"],
  ["skills", "skill.list"],
  ["skills list", "skill.list"],
  ["skills show", "skill.show"],
  ["tools", "tool.list"],
  ["projects", "project.list"],
  ["cache", "cache.list"],
  ["install", "setup.install"],
  ["install skills", "skill.install"],
  ["install plugin", "plugin.install"],
  ["update", "update.run"],
  ["trace", "trace.list"],
  ["monitor", "monitor.status"],
];

export interface FlagMigration {
  from: string;
  to: string;
  reason: string;
}

export const FLAG_MIGRATIONS: ReadonlyArray<FlagMigration> = [
  { from: "--direct", to: "gda tool call", reason: "one generic call surface" },
  { from: "--input", to: "--confirm", reason: "business confirmation replaces interactive input responses" },
  { from: "--async", to: "gda job", reason: "jobs are a first-class command group" },
  { from: "--json", to: "--format json", reason: "one output format flag with a compatibility alias" },
  { from: "--force", to: "--force-stop / --force-clear / --force-install", reason: "one meaning per command" },
  { from: "--all", to: "--all-projects / --all-buckets", reason: "one meaning per command" },
  { from: "--file", to: "--report-file / --skill-file / --input-file", reason: "one meaning per command" },
  { from: "--probe", to: "--probe-level / --probe-port", reason: "one meaning per command" },
  { from: "--format (trace export)", to: "--export-format", reason: "output format and export format separated" },
];

export const EXIT_CODE_HELP: ReadonlyArray<readonly [number, string]> = [
  [0, "success"],
  [1, "tool or business failure"],
  [2, "transport or environment failure"],
  [3, "usage error"],
  [4, "confirmation required"],
  [5, "user declined confirmation"],
  [130, "interrupted"],
];

const SPEC_BY_PATH = new Map<string, CommandSpec>(COMMAND_SPECS.map((spec) => [spec.path, spec]));
const GROUP_NAMES = new Set<string>(COMMAND_GROUPS.map((group) => group.name));

export function findSpec(path: string): CommandSpec | undefined {
  return SPEC_BY_PATH.get(path);
}

export function isGroup(name: string | undefined): boolean {
  return name !== undefined && GROUP_NAMES.has(name);
}

export function toolRisk(tool: string): RiskLevel {
  for (const spec of COMMAND_SPECS) {
    if (spec.tools.includes(tool)) return spec.risk;
  }
  return "none";
}

export function buildAliasMap(entries: ReadonlyArray<readonly [string, string]>): Map<string, string> {
  const map = new Map<string, string>();
  for (const [from, to] of entries) {
    if (map.has(from)) {
      throw new Error(`duplicate command alias: ${from}`);
    }
    if (findSpec(to) === undefined) {
      throw new Error(`command alias target is not a spec: ${from} -> ${to}`);
    }
    map.set(from, to);
  }
  return map;
}

export const COMMAND_ALIASES: ReadonlyMap<string, string> = buildAliasMap(LEGACY_ALIASES);

export function resolveCommand(tokens: string[]): CommandSpec | undefined {
  if (tokens.length === 0) return undefined;
  const head = tokens[0];
  if (tokens.length >= 2 && isGroup(head)) {
    const direct = findSpec(`${head}.${tokens[1]}`);
    if (direct !== undefined) return direct;
  }
  for (const count of [2, 1]) {
    if (tokens.length < count) continue;
    const alias = COMMAND_ALIASES.get(tokens.slice(0, count).join(" "));
    if (alias !== undefined) return findSpec(alias);
  }
  return undefined;
}

export function extractCommandTokens(argv: string[]): CommandTokens {
  const tokens: string[] = [];
  const indexes: number[] = [];
  let i = 0;
  while (i < argv.length) {
    const token = argv[i];
    if (token === "--") break;
    if (token.startsWith("-")) {
      const name = token.split("=", 1)[0];
      const takesValue = GLOBAL_STRING_FLAGS[name] !== undefined && !token.includes("=");
      i += takesValue ? 2 : 1;
      continue;
    }
    tokens.push(token);
    indexes.push(i);
    if (tokens.length >= 2) break;
    i += 1;
  }
  return { tokens, indexes };
}

function globalFlagTypes(): Map<string, FlagType> {
  const types = new Map<string, FlagType>();
  for (const name of GLOBAL_SCOPE_FLAGS) {
    if (GLOBAL_STRING_FLAGS[name] !== undefined) types.set(name, "string");
    else if (GLOBAL_BOOLEAN_FLAGS[name] !== undefined) types.set(name, "boolean");
  }
  return types;
}

export function globalFlags(): CommandFlag[] {
  const types = globalFlagTypes();
  for (const name of Object.keys(VALUE_ALIASES)) types.set(name, "boolean");
  return [...types.entries()]
    .sort(([a], [b]) => a.localeCompare(b))
    .map(([name, type]) => ({
      name,
      type,
      scope: "global" as const,
      description: name === "--json" ? "Alias for --format json" : "Global option",
      repeatable: repeatableFlag(name),
    }));
}

function usageError(spec: CommandSpec, message: string): CliError {
  return new CliError(ExitCode.Usage, `gda ${spec.path}: ${message}`, `Run gda ${spec.path} --help for valid arguments.`);
}

export function parseCommandArgs(spec: CommandSpec, argv: string[]): ParsedCommandArgs {
  const allowed = globalFlagTypes();
  const repeatable = new Set<string>();
  for (const flag of spec.flags) {
    if (flag.scope === "command") {
      allowed.set(flag.name, flag.type);
      if (flag.repeatable === true || repeatableFlag(flag.name)) repeatable.add(flag.name);
    }
  }
  for (const name of GLOBAL_SCOPE_FLAGS) {
    if (repeatableFlag(name)) repeatable.add(name);
  }
  const options: Record<string, string | number | boolean | string[]> = {};
  const seen = new Set<string>();
  const positionals: string[] = [];
  let onlyPositionals = false;
  let i = 0;
  while (i < argv.length) {
    const token = argv[i];
    if (onlyPositionals) {
      positionals.push(token);
      i += 1;
      continue;
    }
    if (token === "--") {
      onlyPositionals = true;
      i += 1;
      continue;
    }
    if (!token.startsWith("-")) {
      positionals.push(token);
      i += 1;
      continue;
    }
    const eq = token.indexOf("=");
    const name = eq === -1 ? token : token.slice(0, eq);
    const alias = VALUE_ALIASES[name];
    const type = allowed.get(name);
    if (alias === undefined && type === undefined) throw usageError(spec, `unknown option ${name}`);
    if (!repeatable.has(name) && seen.has(name)) {
      throw usageError(spec, `option ${name} specified more than once`);
    }
    seen.add(name);
    const flag = spec.flags.find((entry) => entry.name === name);
    if (alias !== undefined) {
      if (eq !== -1) throw usageError(spec, `option ${name} does not take a value`);
      (options as Record<string, unknown>)[alias.key] = alias.value;
    } else if (type === "boolean") {
      if (eq !== -1) throw usageError(spec, `boolean option ${name} does not take a value`);
      options[name] = true;
    } else {
      let value: string;
      if (eq !== -1) {
        value = token.slice(eq + 1);
      } else {
        const next = argv[i + 1];
        if (next === undefined || (next.startsWith("-") && next !== "-")) throw usageError(spec, `option ${name} requires a value`);
        value = next;
        i += 1;
      }
      if (repeatable.has(name)) {
        const existing = options[name];
        options[name] = Array.isArray(existing) ? [...existing, value] : [value];
      } else if (type === "number") {
        const parsed = Number(value);
        if (!Number.isFinite(parsed)) throw usageError(spec, `option ${name} requires a number, got: ${value}`);
        options[name] = parsed;
      } else {
        options[name] = value;
      }
    }
    if (flag?.conflictsWith !== undefined) {
      for (const other of flag.conflictsWith) {
        if (options[other] !== undefined) throw usageError(spec, `option ${name} conflicts with ${other}`);
      }
    }
    i += 1;
  }
  for (const flag of spec.flags) {
    if (flag.required === true && options[flag.name] === undefined) {
      throw usageError(spec, `missing required option ${flag.name}`);
    }
  }
  const maximum = spec.positional.reduce((sum, entry) => sum + (entry.repeatable === true ? Number.MAX_SAFE_INTEGER : 1), 0);
  if (positionals.length > maximum) {
    throw usageError(spec, `too many positional arguments (expected at most ${maximum}, got ${positionals.length})`);
  }
  for (let index = 0; index < spec.positional.length; index += 1) {
    const entry = spec.positional[index];
    if (entry.required && positionals[index] === undefined) {
      throw usageError(spec, `missing required argument <${entry.name}>`);
    }
  }
  return { spec, options, positionals };
}

function flagLine(flag: CommandFlag): string {
  const value = flag.type === "boolean" ? "" : flag.type === "number" ? " <n>" : " <text>";
  const repeat = flag.repeatable === true ? " (repeatable)" : "";
  const conflicts = flag.conflictsWith === undefined ? "" : ` (conflicts: ${flag.conflictsWith.join(", ")})`;
  return `  ${flag.name}${value}  ${flag.description}${repeat}${conflicts}`;
}

export function leafHelpText(spec: CommandSpec): string {
  const usage = [...spec.positional.map((entry) => (entry.required ? `<${entry.name}>` : `[${entry.name}]`)), "[flags]"].join(" ");
  const lines = [
    `gda ${spec.path} — ${spec.description}`,
    "",
    "Usage:",
    `  gda ${spec.path.replace(".", " ")} ${usage}`,
    "",
    `Subcommand: ${spec.subcommand}`,
    `Group: ${spec.group}`,
    `Invoke: gda ${spec.path.replace(".", " ")}`,
    `Tools: ${spec.tools.length === 0 ? "-" : spec.tools.join(", ")}`,
    `Action: ${spec.action}`,
    "",
  ];
  if (spec.positional.length > 0) {
    lines.push("Arguments:", ...spec.positional.map((entry) => `  <${entry.name}>  ${entry.description}${entry.required ? "" : " (optional)"}`), "");
  }
  lines.push("Flags:", ...globalFlags().map(flagLine));
  if (spec.flags.length > 0) {
    lines.push("", `Flags for ${spec.path}:`, ...spec.flags.map((flag) => `  ${flagLine(flag).slice(2)}`));
  }
  lines.push(
    "",
    `Output: ${spec.output.schema} (${spec.output.envelope ? "envelope" : "raw"})`,
    `Risk: ${spec.risk}`,
    `Long running: ${spec.long_running ? "yes" : "no"}`,
    "",
    "Exit codes:",
    ...EXIT_CODE_HELP.map(([code, label]) => `  ${code}  ${label}`),
    "",
    "Examples:",
    ...spec.examples.map((example) => `  ${example.argv}${example.description === "" ? "" : `  ${example.description}`}`),
  );
  return lines.join("\n");
}

export function groupHelpText(groupName: string): string | undefined {
  const group = COMMAND_GROUPS.find((entry) => entry.name === groupName);
  if (group === undefined) return undefined;
  const leaves = COMMAND_SPECS.filter((spec) => spec.group === groupName);
  return [
    `gda ${group.name} — ${group.description}`,
    "",
    "Usage:",
    `  gda ${group.name} <subcommand> [args...]`,
    "",
    "Subcommands:",
    ...leaves.map((spec) => `  ${spec.subcommand.padEnd(12)} ${spec.description}`),
    "",
    `Run gda ${group.name} <subcommand> --help for details.`,
  ].join("\n");
}

export function commandsHelpText(): string {
  return [
    "gda commands — Inspect the declarative command manifest",
    "",
    "Usage:",
    "  gda commands --json",
    "",
    "Outputs every command's name, path, positional arguments, flags, output schema, risk, and invoke target.",
    "The manifest is generated from the same CliCommandSpec used for help, completion, and tests.",
    "",
    "Domains:",
    ...COMMAND_GROUPS.map((group) => `  ${group.name.padEnd(12)} ${group.description}`),
  ].join("\n");
}

export function helpTextForTokens(tokens: string[]): string | undefined {
  if (tokens[0] === "commands") return commandsHelpText();
  if (tokens.length >= 2 && isGroup(tokens[0])) {
    const spec = findSpec(`${tokens[0]}.${tokens[1]}`);
    if (spec !== undefined) return leafHelpText(spec);
  }
  const resolved = resolveCommand(tokens);
  if (resolved !== undefined) return leafHelpText(resolved);
  if (tokens.length === 1 && isGroup(tokens[0])) return groupHelpText(tokens[0]);
  return undefined;
}

export function completionCandidates(argv: string[]): string[] {
  const current = argv[argv.length - 1] ?? "";
  const tokens = extractCommandTokens(argv).tokens.filter((token) => token.length > 0);
  const flags = [...globalFlags().map((flag) => flag.name), "--help"];
  const domains = [...COMMAND_GROUPS.map((group) => group.name), "commands"];
  const leaves = (group: string): string[] =>
    COMMAND_SPECS.filter((spec) => spec.group === group).map((spec) => spec.subcommand);
  if (current.startsWith("--")) {
    const spec = resolveCommand(tokens);
    const candidates = spec === undefined ? flags : [...spec.flags.map((flag) => flag.name), ...flags];
    return candidates.filter((name) => name.startsWith(current)).sort();
  }
  if (tokens.length === 0) {
    return domains.filter((name) => name.startsWith(current)).sort();
  }
  if (tokens.length === 1) {
    if (isGroup(tokens[0])) {
      const prefix = tokens[0] === current ? "" : current;
      return leaves(tokens[0]).filter((name) => name.startsWith(prefix)).sort();
    }
    if (tokens[0] === current) return domains.filter((name) => name.startsWith(current)).sort();
    return flags.filter((name) => name.startsWith(current)).sort();
  }
  if (isGroup(tokens[0])) {
    return leaves(tokens[0]).filter((name) => name.startsWith(current)).sort();
  }
  const spec = resolveCommand(tokens);
  if (spec === undefined) return [];
  return [...spec.flags.map((flag) => flag.name), ...flags].filter((name) => name.startsWith(current)).sort();
}

export interface CommandManifest {
  schema_version: number;
  generated_by: string;
  group_count: number;
  command_count: number;
  groups: Array<{ name: string; description: string }>;
  commands: Array<{
    name: string;
    path: string;
    group: string;
    subcommand: string;
    description: string;
    tools: string[];
    action: string;
    positional: CommandPositional[];
    flags: CommandFlag[];
    output: CommandOutput;
    risk: RiskLevel;
    long_running: boolean;
    examples: CommandExample[];
    invoke: string;
  }>;
  aliases: Array<{ from: string; to: string }>;
  flag_migrations: ReadonlyArray<FlagMigration>;
  exit_codes: ReadonlyArray<{ code: number; meaning: string }>;
}

export function buildManifest(): CommandManifest {
  return {
    schema_version: 1,
    generated_by: "cli/src/command-spec.ts",
    group_count: COMMAND_GROUPS.length,
    command_count: COMMAND_SPECS.length,
    groups: COMMAND_GROUPS.map((group) => ({ ...group })),
    commands: [...COMMAND_SPECS]
      .sort((a, b) => a.path.localeCompare(b.path))
      .map((spec) => ({
        name: spec.name,
        path: spec.path,
        group: spec.group,
        subcommand: spec.subcommand,
        description: spec.description,
        tools: [...spec.tools],
        action: spec.action,
        positional: spec.positional.map((entry) => ({ ...entry })),
        flags: spec.flags.map((flag) => ({ ...flag, conflictsWith: flag.conflictsWith === undefined ? undefined : [...flag.conflictsWith] })),
        output: { ...spec.output },
        risk: spec.risk,
        long_running: spec.long_running,
        examples: spec.examples.map((example) => ({ ...example })),
        invoke: `gda ${spec.path.replace(".", " ")}`,
      })),
    aliases: [...COMMAND_ALIASES.entries()].sort(([a], [b]) => a.localeCompare(b)).map(([from, to]) => ({ from, to })),
    flag_migrations: FLAG_MIGRATIONS,
    exit_codes: EXIT_CODE_HELP.map(([code, meaning]) => ({ code, meaning })),
  };
}
