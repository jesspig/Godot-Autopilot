import fs from "node:fs";
import path from "node:path";

type Tree = Record<string, string>;

export interface Violation {
  file: string;
  line: number;
  rule: string;
  message: string;
  token?: string;
}

export interface Contract {
  commands: Set<string>;
  commandPaths: Set<string>;
  groups: Set<string>;
  aliases: Map<string, string>;
  flags: Set<string>;
  globalFlags: Set<string>;
  localFlags: Map<string, Set<string>>;
  statusFields: Set<string>;
  migratedFlags: Map<string, { to: string; reason: string }>;
  envelopeKeys: Set<string>;
  jsonArgKeys: Set<string>;
  actions: Set<string>;
}

interface WhitelistEntry {
  file: string;
  rule: string;
  contains: string;
  reason: string;
}

const BOOKS = [
  "godot-autopilot",
];

const RECIPE_IDS = [
  "bootstrap",
  "scene-script",
  "asset-pipeline",
  "playtest-debug",
  "safe-refactor",
  "error-recovery",
  "skills-sync",
  "long-task",
];

const SPECIAL_COMMANDS = new Set(["commands"]);
const MAX_FILE_TOKENS = 2000;
const MAX_TOTAL_TOKENS = 24000;

const JSON_ARG_KEYS = new Set([
  "action", "path", "name", "value", "args", "params", "tags", "category",
  "description", "type", "required", "dynamic", "error", "file", "confirm",
  "overwrite", "discard_unsaved", "node_path", "source_code", "target", "mode",
  "limit", "tail", "level", "source", "since", "interval", "export", "timeout",
  "after_frames", "when", "progress", "detach", "id", "port", "project",
  "socket", "args_file", "parent", "property", "props", "script", "signal",
  "extends", "directory", "pattern", "label", "shard", "retries", "junit",
  "coverage", "generation", "cursor", "slow", "event_type", "report_file",
  "skill_file", "input_file", "export_format", "godot", "wait", "headless",
  "play", "no_import", "force_stop", "force_clear", "force_install",
]);

const TOOL_ACTIONS = new Set([
  "status", "errors", "output", "log", "get", "set", "create", "delete",
  "list", "play", "stop", "validate_scene", "sample", "sequence", "object",
  "node", "memory", "catalog", "monitors", "replace", "trash", "status",
  "install", "save", "read", "write", "query", "describe", "call", "batch",
]);

const BANNED: Array<{ rule: string; pattern: RegExp; message: string }> = [
  { rule: "version-pin", pattern: /\b0\.3\.1\b/g, message: "skills must not pin the retired 0.3.1 version" },
  { rule: "version-number", pattern: /(?<![\d.])\d+\.\d+\.\d+(?![\d.])/g, message: "skills must not contain a literal version; use a placeholder" },
  { rule: "git-ref-pin", pattern: /github:jesspig\/Godot-Autopilot#[^\s"')]+/g, message: "skills must not pin a Git ref" },
  { rule: "bunx", pattern: /\bbunx\b/gi, message: "skills must not tell users to run bunx" },
  { rule: "port-9527", pattern: /\b9527\b/g, message: "the old fixed port 9527 must not appear" },
  { rule: "gda-serve", pattern: /\bgda\s+serve\b/gi, message: "gda serve was removed" },
  { rule: "gda-api", pattern: /\bgda\s+api\b/gi, message: "gda api was removed" },
  { rule: "allow-write", pattern: /Allow write/gi, message: "the Allow write checkbox was removed" },
  { rule: "authorization-gate", pattern: /authorization|GODOT_AUTOPILOT_ALLOW|auth_mode/gi, message: "the allow-list gate was replaced by confirm and side_effect/annotations" },
  { rule: "rest-api", pattern: /\bREST\b/g, message: "the REST face was removed" },
  { rule: "config-panel", pattern: /MCP Config dock|配置面板|控制面板|授权开关|allow-list|allow list/gi, message: "the config panel and allow-list are gone; use confirm and annotations" },
  { rule: "mcp", pattern: /\bMCP\b/gi, message: "MCP protocol content must not appear in CLI-only skills" },
  { rule: "mcp", pattern: /Model Context Protocol/gi, message: "Model Context Protocol must not be described in skills" },
  { rule: "mcp", pattern: /\bmcp_url\b/gi, message: "mcp_url is a retired MCP connection record" },
  { rule: "mcp", pattern: /\bmcpServers\b/g, message: "mcpServers is a retired MCP client configuration" },
  { rule: "mcp", pattern: /godot:\/\/skills/gi, message: "godot://skills is a retired MCP resource URI" },
  { rule: "mcp", pattern: /\bsearch_tools\b/gi, message: "search_tools is a retired MCP meta tool; use gda tool search" },
  { rule: "mcp", pattern: /\bget_tool_detail\b/gi, message: "get_tool_detail is a retired MCP meta tool; use gda tool describe" },
  { rule: "mcp", pattern: /\bcall_tool\b/gi, message: "call_tool is a retired MCP meta tool; use gda tool call" },
  { rule: "mcp", pattern: /\blist_categories\b/gi, message: "list_categories is a retired MCP meta tool; use gda tool categories" },
  { rule: "mcp", pattern: /\bresources\/list\b/gi, message: "resources/list is a retired MCP resource method" },
  { rule: "mcp", pattern: /\bresources\/read\b/gi, message: "resources/read is a retired MCP resource method" },
  { rule: "mcp", pattern: /\bprompts\/list\b/gi, message: "prompts/list is a retired MCP prompt method" },
  { rule: "mcp", pattern: /\bprompts\/get\b/gi, message: "prompts/get is a retired MCP prompt method" },
  { rule: "mcp", pattern: /\btasks\/get\b/gi, message: "tasks/get is a retired MCP task method; use gda job" },
  { rule: "mcp", pattern: /\btasks\/cancel\b/gi, message: "tasks/cancel is a retired MCP task method; use gda job cancel" },
  { rule: "mcp", pattern: /\bresultType\b/g, message: "resultType is a retired MCP task field" },
  { rule: "mcp", pattern: /\btaskId\b/g, message: "taskId is a retired MCP task field" },
  { rule: "mcp", pattern: /--direct\b/g, message: "--direct is a retired MCP-era flag; use gda tool call" },
  { rule: "mcp", pattern: /--transport\b/g, message: "--transport is a retired MCP-era flag; native IPC only" },
  { rule: "mcp", pattern: /--no-fallback\b/g, message: "--no-fallback is a retired MCP-era flag; no HTTP fallback exists" },
  { rule: "mcp", pattern: /--input\b/g, message: "--input is a retired MCP-era flag; use --confirm" },
  { rule: "mcp", pattern: /--async\b/g, message: "--async is a retired MCP-era flag; use gda job" },
  { rule: "drift-count", pattern: /\b(?:252|253|325)\b/g, message: "tool and alias counts are drift-prone; discover them with the CLI instead of writing numbers" },
  { rule: "drift-version", pattern: /\b0\.[46]\.(?:x|0)\b/gi, message: "version windows are drift-prone; describe current behaviour instead of version history" },
];

const WHITELIST: WhitelistEntry[] = [];

export function collectTree(root: string): Tree {
  if (!fs.existsSync(root)) {
    throw new Error(`missing directory: ${root}`);
  }
  const files: Tree = {};
  const walk = (dir: string, prefix: string): void => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
      const full = path.join(dir, entry.name);
      const relative = prefix === "" ? entry.name : `${prefix}/${entry.name}`;
      if (entry.isDirectory()) {
        walk(full, relative);
      } else if (entry.isFile()) {
        files[relative] = fs.readFileSync(full, "utf8").replace(/\r\n/g, "\n");
      }
    }
  };
  walk(root, "");
  return files;
}

function section(source: string, startMarker: string, endMarker: string): string {
  const start = source.indexOf(startMarker);
  if (start === -1) {
    throw new Error(`could not derive the CLI contract: missing ${startMarker}`);
  }
  const end = endMarker === "" ? source.length : source.indexOf(endMarker, start);
  return end === -1 ? source.slice(start) : source.slice(start, end);
}

export function readContract(root: string): Contract {
  const args = fs.readFileSync(path.join(root, "cli", "src", "args.ts"), "utf8");
  const spec = fs.readFileSync(path.join(root, "cli", "src", "command-spec.ts"), "utf8");
  const status = fs.readFileSync(path.join(root, "cli", "src", "commands", "status.ts"), "utf8");
  const output = fs.readFileSync(path.join(root, "cli", "src", "output.ts"), "utf8");

  const globalFlags = new Set<string>();
  const argsSection = section(args, "const STRING_FLAGS", "export function parseArgv");
  for (const match of argsSection.matchAll(/"(--[a-z][a-z0-9-]*)"/g)) {
    globalFlags.add(match[1]);
  }

  const groups = new Set<string>();
  const groupSection = section(spec, "export const COMMAND_GROUPS", "export const COMMAND_SPECS");
  for (const match of groupSection.matchAll(/name:\s*"([a-z][a-z0-9-]*)"/g)) {
    groups.add(match[1]);
  }

  const commands = new Set<string>();
  const commandPaths = new Set<string>();
  const localFlags = new Map<string, Set<string>>();
  const actions = new Set<string>(TOOL_ACTIONS);
  const specsSection = section(spec, "export const COMMAND_SPECS", "export const LEGACY_ALIASES");
  for (const block of specsSection.split(/\bdefine\(\{/).slice(1)) {
    const group = /group:\s*"([^"]+)"/.exec(block)?.[1];
    const subcommand = /subcommand:\s*"([^"]+)"/.exec(block)?.[1];
    if (group === undefined || subcommand === undefined) {
      continue;
    }
    commands.add(`${group} ${subcommand}`);
    const commandPath = `${group}.${subcommand}`;
    commandPaths.add(commandPath);
    actions.add(`gcp.${commandPath}`);
    const flags = new Set<string>();
    for (const match of block.matchAll(/(--[a-z][a-z0-9-]*)/g)) {
      flags.add(match[0]);
    }
    localFlags.set(commandPath, flags);
    for (const match of block.matchAll(/action:\s*"([^"]+)"/g)) {
      actions.add(match[1]);
    }
  }

  const aliases = new Map<string, string>();
  const aliasSection = section(spec, "export const LEGACY_ALIASES", "export interface FlagMigration");
  for (const match of aliasSection.matchAll(/\[\s*"([^"]+)",\s*"([^"]+)"\s*\]/g)) {
    aliases.set(match[1], match[2]);
  }

  const migratedFlags = new Map<string, { to: string; reason: string }>();
  const migrationSection = section(spec, "export const FLAG_MIGRATIONS", "export const EXIT_CODE_HELP");
  for (const match of migrationSection.matchAll(/\{\s*from:\s*"([^"]+)",\s*to:\s*"([^"]*)",\s*reason:\s*"([^"]*)"\s*\}/g)) {
    if (!match[1].startsWith("--")) {
      continue;
    }
    migratedFlags.set(match[1], { to: match[2], reason: match[3] });
  }

  const flags = new Set<string>(globalFlags);
  for (const value of localFlags.values()) {
    for (const flag of value) {
      flags.add(flag);
    }
  }

  const statusFields = new Set<string>(["error"]);
  const start = status.indexOf("function buildStatus");
  const end = status.indexOf("export async function run", start);
  const body = start === -1 || end === -1 ? "" : status.slice(start, end);
  for (const match of body.matchAll(/^\s{4}([a-z][a-z0-9_]*):/gm)) {
    statusFields.add(match[1]);
  }
  for (const match of body.matchAll(/^\s{4}([a-z][a-z0-9_]*),$/gm)) {
    statusFields.add(match[1]);
  }

  const envelopeKeys = new Set<string>();
  const controlKeysMatch = /const controlKeys = new Set\(\[([^\]]*)\]\)/.exec(output);
  if (controlKeysMatch !== null) {
    for (const match of controlKeysMatch[1].matchAll(/"([^"]+)"/g)) {
      envelopeKeys.add(match[1]);
    }
  }

  if (commands.size === 0 || flags.size === 0 || statusFields.size <= 1 || groups.size === 0 || migratedFlags.size === 0 || envelopeKeys.size === 0) {
    throw new Error("could not derive the CLI contract from cli/src");
  }
  return {
    commands,
    commandPaths,
    groups,
    aliases,
    flags,
    globalFlags,
    localFlags,
    statusFields,
    migratedFlags,
    envelopeKeys,
    jsonArgKeys: JSON_ARG_KEYS,
    actions,
  };
}

function lineOf(text: string, index: number): number {
  return text.slice(0, index).split("\n").length;
}

function isWhitelisted(file: string, rule: string, line: string): boolean {
  return WHITELIST.some(
    (entry) => entry.file === file && entry.rule === rule && line.includes(entry.contains),
  );
}

function checkKeywords(files: Tree, violations: Violation[]): void {
  for (const [file, content] of Object.entries(files)) {
    for (const lineInfo of content.split(/\r?\n/).entries()) {
      const line = lineInfo[1];
      for (const banned of BANNED) {
        banned.pattern.lastIndex = 0;
        let match: RegExpExecArray | null;
        while ((match = banned.pattern.exec(line)) !== null) {
          if (!isWhitelisted(file, banned.rule, line)) {
            violations.push({
              file,
              line: lineInfo[0] + 1,
              rule: banned.rule,
              message: banned.message,
              token: match[0],
            });
          }
        }
      }
    }
  }
}

function checkDriftTables(file: string, content: string, violations: Violation[]): void {
  const lines = content.split(/\r?\n/);
  for (let i = 0; i + 1 < lines.length; i += 1) {
    const header = lines[i];
    if (!/^\s*\|.*\|\s*$/.test(header) || !/^\s*\|[\s:\-|]+\|\s*$/.test(lines[i + 1])) {
      continue;
    }
    if (!/\b(?:alias|old|legacy|deprecated|canonical|migration|迁移|旧名)\b/i.test(header)) {
      continue;
    }
    violations.push({
      file,
      line: i + 1,
      rule: "drift-alias-table",
      message: "alias and canonical migration tables are drift-prone; use the CLI alias resolution and discovery commands",
    });
  }
}

function commandSpans(content: string): Array<{ text: string; line: number }> {
  const spans: Array<{ text: string; line: number }> = [];
  for (const match of content.matchAll(/`gda[ \t]+([^`\n]*)`/g)) {
    spans.push({ text: `gda ${match[1]}`, line: lineOf(content, match.index ?? 0) });
  }
  for (const match of content.matchAll(/(?:^|\n)[ \t]*gda[ \t]+([^\n`]*)/g)) {
    const text = `gda ${match[1]}`;
    if (!spans.some((span) => span.text === text && Math.abs(span.line - lineOf(content, (match.index ?? 0) + 1)) <= 1)) {
      spans.push({ text, line: lineOf(content, (match.index ?? 0) + 1) });
    }
  }
  for (const block of content.matchAll(/```(?:bash|sh|shell)\r?\n([\s\S]*?)```/g)) {
    for (const line of block[1].split(/\r?\n/)) {
      const match = /^\s*gda[ \t]+(\S.*)$/.exec(line);
      if (match !== null) {
        spans.push({ text: `gda ${match[1]}`, line: lineOf(content, block.index ?? 0) + lineOf(block[1], line.indexOf(match[1])) });
      }
    }
  }
  return spans;
}

function resolveCommand(tokens: string[], contract: Contract): { path: string; localFlags: Set<string> } | undefined {
  if (tokens.length === 0) {
    return undefined;
  }
  const head = tokens[0];
  if (tokens.length >= 2) {
    const pair = `${head} ${tokens[1]}`;
    if (contract.commands.has(pair)) {
      const resolved = `${head}.${tokens[1]}`;
      return { path: resolved, localFlags: contract.localFlags.get(resolved) ?? new Set() };
    }
    const pairAlias = contract.aliases.get(pair);
    if (pairAlias !== undefined) {
      return { path: pairAlias, localFlags: contract.localFlags.get(pairAlias) ?? new Set() };
    }
  }
  const singleAlias = contract.aliases.get(head);
  if (singleAlias !== undefined) {
    return { path: singleAlias, localFlags: contract.localFlags.get(singleAlias) ?? new Set() };
  }
  if (contract.groups.has(head) || SPECIAL_COMMANDS.has(head)) {
    return { path: head, localFlags: new Set() };
  }
  return undefined;
}

function checkCommands(file: string, content: string, contract: Contract, violations: Violation[]): void {
  for (const span of commandSpans(content)) {
    const tokens = span.text.trim().split(/\s+/).filter((token) => token !== "");
    if (tokens[0] !== "gda") {
      continue;
    }
    const afterGda = tokens.slice(1);
    if (afterGda.length > 0 && afterGda[0].startsWith("--") && contract.globalFlags.has(afterGda[0])) {
      continue;
    }
    const resolved = resolveCommand(afterGda, contract);
    if (resolved === undefined) {
      violations.push({
        file,
        line: span.line,
        rule: "unknown-command",
        message: `gda ${tokens.slice(1).join(" ")} is not a registered command or legacy alias`,
      });
    }
    const allowed = new Set<string>(contract.globalFlags);
    for (const flag of resolved?.localFlags ?? []) {
      allowed.add(flag);
    }
    for (const match of span.text.matchAll(/--[a-z][a-z0-9-]*/g)) {
      const flag = match[0];
      if (allowed.has(flag)) {
        continue;
      }
      const migrated = contract.migratedFlags.get(flag);
      if (migrated !== undefined) {
        violations.push({
          file,
          line: span.line,
          rule: "migrated-flag",
          message: migrated.to === ""
            ? `${flag} is retired: ${migrated.reason}`
            : `${flag} is retired; use ${migrated.to}: ${migrated.reason}`,
          token: flag,
        });
        continue;
      }
      violations.push({
        file,
        line: span.line,
        rule: "unknown-flag",
        message: `${flag} is not registered in cli/src/args.ts or the matched command spec`,
        token: flag,
      });
    }
  }
}

function checkFencedJson(file: string, content: string, contract: Contract, violations: Violation[]): void {
  for (const block of content.matchAll(/```json\r?\n([\s\S]*?)```/g)) {
    const body = block[1];
    const baseLine = lineOf(content, block.index ?? 0) + 1;
    let parsed: unknown;
    try {
      parsed = JSON.parse(body);
    } catch (error) {
      violations.push({
        file,
        line: baseLine,
        rule: "json-parse",
        message: `fenced JSON is not valid: ${error instanceof Error ? error.message : String(error)}`,
      });
      continue;
    }
    if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) {
      violations.push({ file, line: baseLine, rule: "json-schema", message: "fenced JSON must be an object" });
      continue;
    }
    const record = parsed as Record<string, unknown>;
    for (const key of Object.keys(record)) {
      if (!contract.envelopeKeys.has(key) && !contract.jsonArgKeys.has(key)) {
        violations.push({
          file,
          line: baseLine,
          rule: "json-key",
          message: `JSON key ${key} is not in the CLI envelope or tool-argument schema`,
          token: key,
        });
      }
    }
    if (Object.prototype.hasOwnProperty.call(record, "action")) {
      const action = record.action;
      if (typeof action !== "string" || action.length === 0) {
        violations.push({ file, line: baseLine, rule: "json-action", message: "action must be a non-empty string" });
      } else if (!contract.actions.has(action)) {
        violations.push({
          file,
          line: baseLine,
          rule: "json-action",
          message: `action ${action} is not declared in the CLI command manifest or tool action schema`,
          token: action,
        });
      }
    }
  }
}

function checkBudgets(label: string, files: Tree, violations: Violation[]): void {
  let total = 0;
  for (const [relative, content] of Object.entries(files)) {
    const tokens = content.split(/\s+/).filter((token) => token.length > 0).length;
    total += tokens;
    if (tokens > MAX_FILE_TOKENS) {
      violations.push({
        file: `${label}/${relative}`,
        line: 1,
        rule: "budget-file",
        message: `${tokens} load tokens exceed the per-file limit of ${MAX_FILE_TOKENS}`,
      });
    }
  }
  if (total > MAX_TOTAL_TOKENS) {
    violations.push({
      file: label,
      line: 1,
      rule: "budget-total",
      message: `${total} load tokens exceed the total limit of ${MAX_TOTAL_TOKENS}`,
    });
  }
}

function checkEntryCard(file: string, content: string, violations: Violation[]): void {
  const required: Array<{ pattern: RegExp; message: string }> = [
    { pattern: /gda\s+(?:tool\s+)?ping\b/, message: "entry card must show the CLI-only health probe gda ping" },
    { pattern: /gda\s+tool\s+list\b/, message: "entry card must show dynamic catalog discovery with gda tool list" },
    { pattern: /gda\s+tool\s+search\b/, message: "entry card must show dynamic catalog discovery with gda tool search" },
    { pattern: /gda\s+tool\s+describe\b/, message: "entry card must show schema discovery with gda tool describe" },
    { pattern: /gda\s+tool\s+call\b/, message: "entry card must show the generic tool call escape hatch" },
    { pattern: /--confirm\b/, message: "entry card must show the --confirm gate" },
    { pattern: /confirmation_required\b/, message: "entry card must describe the confirmation_required response" },
    { pattern: /gda\s+doctor\b/, message: "entry card must show error recovery with gda doctor" },
    { pattern: /gda\s+job\b/, message: "entry card must show long-task recovery with gda job" },
  ];
  for (const entry of required) {
    if (!entry.pattern.test(content)) {
      violations.push({ file, line: 1, rule: "entry-cli-only", message: entry.message });
    }
  }
  const routingRows = content.split(/\r?\n/).filter((line) => /^\s*\|.*references\//.test(line)).length;
  if (routingRows < 3) {
    violations.push({
      file,
      line: 1,
      rule: "entry-routing",
      message: "entry card must route first-level domains to references in a table",
    });
  }
}

function checkRecipes(label: string, files: Tree, violations: Violation[]): void {
  for (const id of RECIPE_IDS) {
    const relative = `godot-autopilot/references/recipes/${id}.md`;
    const content = files[relative];
    if (content === undefined) {
      violations.push({
        file: `${label}/${relative}`,
        line: 1,
        rule: "recipe-missing",
        message: `missing recipe ${id}`,
      });
      continue;
    }
    const lines = content.split(/\r?\n/);
    const firstHeading = lines.find((line) => /^#\s+/.test(line));
    if (firstHeading === undefined || firstHeading.replace(/^#\s+/, "").trim() !== id) {
      violations.push({
        file: `${label}/${relative}`,
        line: 1,
        rule: "recipe-id",
        message: `first heading must be # ${id} and match the recipe path`,
      });
    }
    const sections: number[] = [];
    for (const line of lines) {
      const match = /^##\s+(\d+)\.\s+\S/.exec(line);
      if (match !== null) {
        sections.push(Number(match[1]));
      }
    }
    if (sections.join(",") !== RECIPE_IDS.map((_, index) => index + 1).join(",")) {
      violations.push({
        file: `${label}/${relative}`,
        line: 1,
        rule: "recipe-sections",
        message: "recipe must contain the eight numbered sections ## 1. through ## 8. in order",
      });
    }
  }
}

function checkTree(label: string, files: Tree, contract: Contract, violations: Violation[]): void {
  const roots = new Set<string>();
  for (const [relative, content] of Object.entries(files)) {
    const parts = relative.split("/");
    const book = parts[0];
    if (book === undefined || !BOOKS.includes(book)) {
      violations.push({ file: `${label}/${relative}`, line: 1, rule: "unknown-book", message: `unknown book ${book}` });
      continue;
    }
    roots.add(book);
    const isSkillDoc = parts.length === 2 && parts[1] === "SKILL.md";
    const isReference = parts.length >= 3 && parts[1] === "references" && parts[2] !== undefined && parts[parts.length - 1].endsWith(".md");
    if (!isSkillDoc && !isReference) {
      violations.push({
        file: `${label}/${relative}`,
        line: 1,
        rule: "unexpected-file",
        message: "a book may only contain SKILL.md and references/*.md",
      });
      continue;
    }
    if (isSkillDoc) {
      let frontmatter: { name: string; description: string } | null = null;
      try {
        frontmatter = parseFrontmatter(content, `${label}/${relative}`);
      } catch (error) {
        violations.push({
          file: `${label}/${relative}`,
          line: 1,
          rule: "frontmatter",
          message: error instanceof Error ? error.message : String(error),
        });
      }
      if (frontmatter !== null && frontmatter.name !== book) {
        violations.push({
          file: `${label}/${relative}`,
          line: 2,
          rule: "frontmatter",
          message: `name ${frontmatter.name} does not match directory ${book}`,
        });
      }
    }
    const fullName = `${label}/${relative}`;
    checkKeywords({ [fullName]: content }, violations);
    checkDriftTables(fullName, content, violations);
    checkCommands(fullName, content, contract, violations);
    checkFencedJson(fullName, content, contract, violations);
  }
  for (const book of BOOKS) {
    if (!roots.has(book)) {
      violations.push({ file: `${label}/${book}`, line: 1, rule: "missing-book", message: `missing book ${book}` });
    } else if (files[`${book}/SKILL.md`] === undefined) {
      violations.push({ file: `${label}/${book}`, line: 1, rule: "missing-skill", message: `missing ${book}/SKILL.md` });
    }
  }
  const card = files["godot-autopilot/SKILL.md"];
  if (card !== undefined) {
    checkEntryCard(`${label}/godot-autopilot/SKILL.md`, card, violations);
  }
  checkRecipes(label, files, violations);
  checkBudgets(label, files, violations);
}

function parseFrontmatter(content: string, label: string): { name: string; description: string } | null {
  if (content.startsWith("\uFEFF")) {
    throw new Error(`${label}: UTF-8 BOM is not allowed`);
  }
  const lines = content.split(/\r?\n/);
  if (lines[0] !== "---") {
    throw new Error(`${label}: first line must be ---`);
  }
  const end = lines.indexOf("---", 1);
  if (end === -1) {
    throw new Error(`${label}: frontmatter is not closed`);
  }
  const fields = new Map<string, string>();
  for (const line of lines.slice(1, end)) {
    if (line.trim() === "") {
      throw new Error(`${label}: blank line inside frontmatter`);
    }
    const separator = line.indexOf(":");
    if (separator === -1) {
      throw new Error(`${label}: frontmatter line has no key: ${line}`);
    }
    const key = line.slice(0, separator).trim();
    const value = line.slice(separator + 1).trim();
    if (fields.has(key)) {
      throw new Error(`${label}: duplicate frontmatter key ${key}`);
    }
    fields.set(key, value);
  }
  for (const key of fields.keys()) {
    if (key !== "name" && key !== "description") {
      throw new Error(`${label}: unexpected frontmatter key ${key}`);
    }
  }
  const name = fields.get("name");
  const description = fields.get("description");
  if (name === undefined || description === undefined) {
    throw new Error(`${label}: frontmatter needs name and description`);
  }
  if (!/^[a-z0-9]+(-[a-z0-9]+)*$/.test(name)) {
    throw new Error(`${label}: invalid skill name ${name}`);
  }
  if (!(description.startsWith("\"") && description.endsWith("\"")) || description.length < 3) {
    throw new Error(`${label}: description must be a non-empty double-quoted string`);
  }
  if (description.length > 1026) {
    throw new Error(`${label}: description exceeds 1024 characters`);
  }
  return { name, description };
}

function commandTokens(content: string): string[] {
  const tokens = new Set<string>();
  for (const match of content.matchAll(/(?:^[ \t]*|`)(gda[ \t]+[a-z][a-z0-9-]*)/gm)) {
    tokens.add(match[1].replace(/[ \t]+/g, " "));
  }
  for (const match of content.matchAll(/(?:^[ \t]*gda\b[^\n]*|`gda[^`]*`)/gm)) {
    for (const flag of match[0].matchAll(/--[a-z][a-z0-9-]*/g)) {
      tokens.add(flag[0]);
    }
  }
  return [...tokens].sort();
}

function orderedCodeCommands(content: string, contract: Contract): string[] {
  const commands: string[] = [];
  for (const span of commandSpans(content)) {
    const tokens = span.text.trim().split(/\s+/).filter((token) => token !== "");
    if (tokens[0] !== "gda") {
      continue;
    }
    const afterGda = tokens.slice(1);
    if (afterGda.length > 0 && afterGda[0].startsWith("--") && contract.globalFlags.has(afterGda[0])) {
      continue;
    }
    const resolved = resolveCommand(afterGda, contract);
    commands.push(resolved === undefined ? afterGda.join(" ") : resolved.path);
  }
  return commands;
}

function checkParity(en: Tree, zh: Tree, contract: Contract, violations: Violation[]): void {
  const enPaths = Object.keys(en).sort();
  const zhPaths = Object.keys(zh).sort();
  for (const file of enPaths) {
    if (zh[file] === undefined) {
      violations.push({ file: `skills-zh/${file}`, line: 1, rule: "parity", message: "missing Chinese counterpart" });
      continue;
    }
    const enTokens = commandTokens(en[file]);
    const zhTokens = commandTokens(zh[file]);
    const missing = enTokens.filter((token) => !zhTokens.includes(token));
    const extra = zhTokens.filter((token) => !enTokens.includes(token));
    if (missing.length > 0) {
      violations.push({
        file: `skills-zh/${file}`,
        line: 1,
        rule: "command-parity",
        message: `missing commands/flags from the English book: ${missing.join(", ")}`,
      });
    }
    if (extra.length > 0) {
      violations.push({
        file: `skills-zh/${file}`,
        line: 1,
        rule: "command-parity",
        message: `commands/flags not present in the English book: ${extra.join(", ")}`,
      });
    }
    const enOrder = orderedCodeCommands(en[file], contract);
    const zhOrder = orderedCodeCommands(zh[file], contract);
    if (enOrder.join("\n") !== zhOrder.join("\n")) {
      violations.push({
        file: `skills-zh/${file}`,
        line: 1,
        rule: "command-order-parity",
        message: "code-span commands are not in the same order as the English book",
      });
    }
  }
  for (const file of zhPaths) {
    if (en[file] === undefined) {
      violations.push({ file: `skills/${file}`, line: 1, rule: "parity", message: "missing English counterpart" });
    }
  }
}

export function checkAll(en: Tree, zh: Tree, contract: Contract): Violation[] {
  const violations: Violation[] = [];
  checkTree("skills", en, contract, violations);
  checkTree("skills-zh", zh, contract, violations);
  checkParity(en, zh, contract, violations);
  return violations.sort((a, b) => `${a.file}:${a.line}:${a.rule}`.localeCompare(`${b.file}:${b.line}:${b.rule}`));
}

function checkAsset(root: string, en: Tree, zh: Tree, violations: Violation[]): void {
  const assetPath = path.join(root, "cli", "assets", "skills.json");
  const version = fs.readFileSync(path.join(root, "VERSION"), "utf8").trim();
  let asset: { version?: unknown; files?: unknown; files_zh?: unknown };
  try {
    asset = JSON.parse(fs.readFileSync(assetPath, "utf8")) as typeof asset;
  } catch (error) {
    violations.push({
      file: "cli/assets/skills.json",
      line: 1,
      rule: "asset-drift",
      message: `cannot read the embedded skills asset: ${error instanceof Error ? error.message : String(error)}`,
    });
    return;
  }
  if (asset.version !== version) {
    violations.push({
      file: "cli/assets/skills.json",
      line: 1,
      rule: "asset-drift",
      message: `asset version ${String(asset.version)} does not match VERSION ${version}`,
    });
  }
  for (const [label, files, key] of [
    ["files", en, "files"],
    ["files_zh", zh, "files_zh"],
  ] as const) {
    const embedded = asset[key];
    if (embedded === null || typeof embedded !== "object") {
      violations.push({ file: "cli/assets/skills.json", line: 1, rule: "asset-drift", message: `missing ${label}` });
      continue;
    }
    const record = embedded as Record<string, unknown>;
    for (const [relative, content] of Object.entries(files)) {
      if (record[relative] !== content) {
        violations.push({
          file: "cli/assets/skills.json",
          line: 1,
          rule: "asset-drift",
          message: `${label} is stale for ${relative}; run bun cli/scripts/build-skills-asset.ts`,
        });
      }
    }
    for (const relative of Object.keys(record)) {
      if (files[relative] === undefined) {
        violations.push({
          file: "cli/assets/skills.json",
          line: 1,
          rule: "asset-drift",
          message: `${label} contains a removed file ${relative}; run bun cli/scripts/build-skills-asset.ts`,
        });
      }
    }
  }
}

function sampleTree(overrides: Record<string, string>): Tree {
  const tree: Tree = {};
  for (const book of BOOKS) {
    tree[`${book}/SKILL.md`] = `---\nname: ${book}\ndescription: "sample"\n---\n\n# ${book}\n\ngda status --json\n`;
  }
  return { ...tree, ...overrides };
}

function runSelfTest(contract: Contract): void {
  const failures: string[] = [];
  const badReference = [
    "bunx 9527 gda serve gda api Allow write REST authorization MCP Config dock",
    "0.3.1",
    "MCP search_tools get_tool_detail call_tool mcp_url mcpServers godot://skills resources/list prompts/get tasks/get resultType taskId",
    "--direct --transport --no-fallback --input --async",
    "The catalog has 253 tools and 325 aliases, deprecated from 0.4.x to 0.6.x.",
    "| old | canonical |",
    "| --- | --- |",
    "| foo | bar |",
    "```json",
    "{\"mcp_url\": \"x\", \"wrong_field\": 1, \"action\": \"bogus_action\"}",
    "```",
    "```json",
    "{\"action\": ",
    "```",
    "gda nope --all --bogus-flag",
  ].join("\n");
  const overrides: Record<string, string> = {
    "godot-autopilot/SKILL.md": "---\nname: godot-autopilot\ndescription: \"sample\"\n---\n\n# godot-autopilot\n",
    "godot-autopilot/references/bad.md": badReference,
    "godot-autopilot/references/recipes/bootstrap.md": "# wrong-id\n\n## Intro\n\nno sections\n",
    "godot-autopilot-bad/SKILL.md": "---\nname: bad\ndescription: \"bad\"\n---\n\n# bad\n",
  };
  const en = sampleTree(overrides);
  const zh = sampleTree({
    "godot-autopilot/OTHER.md": "---\nname: godot-autopilot\ndescription: \"sample\"\n---\n",
  });
  const withoutCard = { ...en };
  delete withoutCard["godot-autopilot/SKILL.md"];
  const violations = [
    ...checkAll(withoutCard, zh, contract),
    ...checkAll(en, zh, contract),
  ];
  const rules = new Set(violations.map((violation) => violation.rule));
  for (const expected of [
    "unknown-book", "bunx", "port-9527", "gda-serve", "gda-api", "allow-write",
    "authorization-gate", "config-panel", "rest-api", "version-pin",
    "version-number", "unknown-command", "unknown-flag", "unexpected-file",
    "missing-skill", "mcp", "drift-count", "drift-version", "drift-alias-table",
    "migrated-flag", "json-key", "json-action", "json-parse", "entry-cli-only",
    "entry-routing", "recipe-id", "recipe-sections",
  ]) {
    if (!rules.has(expected)) {
      failures.push(`self-test did not detect ${expected}`);
    }
  }
  if (failures.length > 0) {
    for (const failure of failures) {
      process.stderr.write(`${failure}\n`);
    }
    process.exit(1);
  }
  process.stdout.write(`skills-guard self-test OK (${violations.length} sample violations detected)\n`);
}

function main(): void {
  const rootArg = process.argv.indexOf("--root");
  const root = rootArg === -1 ? process.cwd() : path.resolve(process.argv[rootArg + 1] ?? ".");
  const contract = readContract(root);
  if (process.argv.includes("--self-test")) {
    runSelfTest(contract);
    return;
  }
  const en = collectTree(path.join(root, "skills"));
  const zh = collectTree(path.join(root, "skills-zh"));
  const violations = checkAll(en, zh, contract);
  checkAsset(root, en, zh, violations);
  if (violations.length > 0) {
    for (const violation of violations) {
      process.stderr.write(`${violation.file}:${violation.line}: [${violation.rule}] ${violation.message}\n`);
    }
    process.stderr.write(`skills-guard FAILED with ${violations.length} violation(s)\n`);
    process.exit(1);
  }
  process.stdout.write(`skills-guard OK (${Object.keys(en).length} EN files, ${Object.keys(zh).length} ZH files, ${contract.commands.size} commands)\n`);
}

if (import.meta.main) {
  main();
}
