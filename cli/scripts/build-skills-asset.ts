import fs from "node:fs";
import path from "node:path";

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

interface SkillFile {
  relative: string;
  content: string;
}

function walk(root: string, prefix = ""): SkillFile[] {
  const files: SkillFile[] = [];
  for (const entry of fs.readdirSync(root, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
    const full = path.join(root, entry.name);
    const relative = prefix === "" ? entry.name : `${prefix}/${entry.name}`;
    if (entry.isDirectory()) {
      files.push(...walk(full, relative));
    } else if (entry.isFile() && entry.name.endsWith(".md")) {
      files.push({ relative, content: fs.readFileSync(full, "utf8").replace(/\r\n/g, "\n") });
    }
  }
  return files;
}

function readTree(root: string, label: string): Record<string, string> {
  if (!fs.existsSync(root)) {
    throw new Error(`${label} root does not exist: ${root}`);
  }
  const output: Record<string, string> = {};
  for (const file of walk(root)) {
    if (output[file.relative] !== undefined) {
      throw new Error(`${label}: duplicate file ${file.relative}`);
    }
    output[file.relative] = file.content;
  }
  return output;
}

function parseFrontmatter(content: string, label: string): { name: string; description: string } {
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
  const name = fields.get("name");
  const description = fields.get("description");
  if (name === undefined || description === undefined) {
    throw new Error(`${label}: frontmatter needs name and description`);
  }
  if (!/^[a-z0-9]+(-[a-z0-9]+)*$/.test(name)) {
    throw new Error(`${label}: invalid skill name ${name}`);
  }
  if (!(description.startsWith("\"") && description.endsWith("\""))) {
    throw new Error(`${label}: description must be a double-quoted string`);
  }
  return { name, description };
}

function validateTree(tree: Record<string, string>, label: string): void {
  const roots = new Set<string>();
  for (const [relative, content] of Object.entries(tree)) {
    const parts = relative.split("/");
    const book = parts[0];
    if (book === undefined || !BOOKS.includes(book)) {
      throw new Error(`${label}: unknown book in ${relative}`);
    }
    roots.add(book);
    if (parts.length === 2 && parts[1] === "SKILL.md") {
      const parsed = parseFrontmatter(content, `${label}/${relative}`);
      if (parsed.name !== book) {
        throw new Error(`${label}/${relative}: name ${parsed.name} does not match directory ${book}`);
      }
    } else if (parts.length < 2 || !parts[1]?.endsWith(".md")) {
      throw new Error(`${label}: unexpected file ${relative}`);
    }
  }
  const missing = BOOKS.filter((book) => !roots.has(book));
  if (missing.length > 0) {
    throw new Error(`${label}: missing books: ${missing.join(", ")}`);
  }
  for (const book of BOOKS) {
    if (tree[`${book}/SKILL.md`] === undefined) {
      throw new Error(`${label}: missing ${book}/SKILL.md`);
    }
  }
}

const root = path.resolve(import.meta.dir, "..", "..");
const version = fs.readFileSync(path.join(root, "VERSION"), "utf8").trim();
const en = readTree(path.join(root, "skills"), "skills");
const zh = readTree(path.join(root, "skills-zh"), "skills-zh");
validateTree(en, "skills");
validateTree(zh, "skills-zh");

const enPaths = Object.keys(en).sort();
const zhPaths = Object.keys(zh).sort();
if (enPaths.join("\n") !== zhPaths.join("\n")) {
  throw new Error("skills and skills-zh do not have the same relative paths");
}

const output = {
  version,
  files: en,
  files_zh: zh,
};
const target = path.join(root, "cli", "assets", "skills.json");
fs.writeFileSync(target, `${JSON.stringify(output, null, 2)}\n`, "utf8");
process.stdout.write(`embedded ${enPaths.length} EN files and ${zhPaths.length} ZH files (version ${version})\n`);
