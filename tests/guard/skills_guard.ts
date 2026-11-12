import fs from "node:fs";
import path from "node:path";

type Tree = Record<string, string>;

export interface Violation {
  file: string;
  line: number;
  rule: string;
  message: string;
}

export interface Contract {
  commands: Set<string>;
  flags: Set<string>;
  statusFields: Set<string>;
}

interface WhitelistEntry {
  file: string;
  rule: string;
  contains: string;
  reason: string;
}

const BOOKS = [
  "godot-autopilot",
  "godot-autopilot-content",
  "godot-autopilot-csharp",
  "godot-autopilot-resources",
  "godot-autopilot-runtime",
  "godot-autopilot-scene-system",
  "godot-autopilot-scripting",
  "godot-autopilot-servers",
  "godot-autopilot-setup",
  "godot-autopilot-tools",
];

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
        files[relative] = fs.readFileSync(full, "utf8");
      }
    }
  };
  walk(root, "");
  return files;
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

export function readContract(root: string): Contract {
  const index = fs.readFileSync(path.join(root, "cli", "src", "commands", "index.ts"), "utf8");
  const args = fs.readFileSync(path.join(root, "cli", "src", "args.ts"), "utf8");
  const status = fs.readFileSync(path.join(root, "cli", "src", "commands", "status.ts"), "utf8");

  const commands = new Set<string>();
  for (const match of index.matchAll(/^\s{2}([a-z][a-z0-9-]*):\s*\{\s*run:/gm)) {
    commands.add(match[1]);
  }
  const flags = new Set<string>();
  for (const match of args.matchAll(/"(?:--[a-z0-9-]+)":/g)) {
    flags.add(match[0].slice(1, -2));
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
  if (commands.size === 0 || flags.size === 0 || statusFields.size <= 1) {
    throw new Error("could not derive the CLI contract from cli/src");
  }
  return { commands, flags, statusFields };
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
        if (banned.pattern.test(line) && !isWhitelisted(file, banned.rule, line)) {
          violations.push({ file, line: lineInfo[0] + 1, rule: banned.rule, message: banned.message });
        }
      }
    }
  }
}

function checkCommands(file: string, content: string, contract: Contract, violations: Violation[]): void {
  const commandPatterns = [
    /^[ \t]*gda[ \t]+([a-z][a-z0-9-]*)/gm,
    /`gda[ \t]+([a-z][a-z0-9-]*)[^`]*`/g,
  ];
  for (const pattern of commandPatterns) {
    for (const match of content.matchAll(pattern)) {
      const command = match[1];
      if (!contract.commands.has(command)) {
        violations.push({
          file,
          line: lineOf(content, match.index ?? 0),
          rule: "unknown-command",
          message: `gda ${command} is not registered in cli/src/commands/index.ts`,
        });
      }
    }
  }
  for (const lineInfo of content.split(/\r?\n/).entries()) {
    const line = lineInfo[1];
    if (!/^[ \t]*gda\b/.test(line) && !/`gda[^`]*`/.test(line)) {
      continue;
    }
    for (const match of line.matchAll(/--[a-z][a-z0-9-]*/g)) {
      const flag = match[0];
      if (!contract.flags.has(flag)) {
        violations.push({
          file,
          line: lineInfo[0] + 1,
          rule: "unknown-flag",
          message: `${flag} is not registered in cli/src/args.ts`,
        });
      }
    }
  }
}

function checkStatusFields(file: string, content: string, contract: Contract, violations: Violation[]): void {
  for (const block of content.matchAll(/```json\r?\n([\s\S]*?)```/g)) {
    const body = block[1];
    if (!body.includes("\"mcp_url\"")) {
      continue;
    }
    const baseLine = lineOf(content, block.index ?? 0) + 1;
    for (const field of body.matchAll(/"([a-z][a-z0-9_]*)"\s*:/g)) {
      const key = field[1];
      if (!contract.statusFields.has(key)) {
        violations.push({
          file,
          line: baseLine + lineOf(body, field.index ?? 0) - 1,
          rule: "unknown-status-field",
          message: `status field ${key} is not produced by cli/src/commands/status.ts`,
        });
      }
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
    if (parts.length !== 2 || parts[1] !== "SKILL.md") {
      violations.push({
        file: `${label}/${relative}`,
        line: 1,
        rule: "unexpected-file",
        message: "each book must contain exactly SKILL.md",
      });
      continue;
    }
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
    checkKeywords({ [`${label}/${relative}`]: content }, violations);
    checkCommands(`${label}/${relative}`, content, contract, violations);
    checkStatusFields(`${label}/${relative}`, content, contract, violations);
  }
  for (const book of BOOKS) {
    if (!roots.has(book)) {
      violations.push({ file: `${label}/${book}`, line: 1, rule: "missing-book", message: `missing book ${book}` });
    }
  }
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

function checkParity(en: Tree, zh: Tree, violations: Violation[]): void {
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
  checkParity(en, zh, violations);
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
  const en = sampleTree({
    "godot-autopilot/SKILL.md": "---\nname: godot-autopilot\ndescription: \"sample\"\n---\n\nbunx 9527 gda serve gda api Allow write REST authorization MCP Config dock\n0.3.1\ngda nope\n```json\n{\"mcp_url\": \"x\", \"wrong_field\": 1}\n```\n",
    "godot-autopilot-bad/SKILL.md": "---\nname: bad\ndescription: \"bad\"\n---\n\n# bad\n",
  });
  delete en["godot-autopilot-setup/SKILL.md"];
  const zh = sampleTree({
    "godot-autopilot/OTHER.md": "---\nname: godot-autopilot\ndescription: \"sample\"\n---\n",
  });
  const violations = checkAll(en, zh, contract);
  const rules = new Set(violations.map((violation) => violation.rule));
  for (const expected of ["unknown-book", "bunx", "port-9527", "gda-serve", "gda-api", "allow-write", "authorization-gate", "config-panel", "rest-api", "version-pin", "version-number", "unknown-command", "unknown-status-field", "unexpected-file", "missing-book"]) {
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
