import { mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import path from "node:path";

const SKILL_COUNT = 10;
const NAME_PATTERN = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const MAX_NAME_LEN = 64;
const MAX_DESCRIPTION_LEN = 1024;
const BASE_DELIM = "gda_s";
const DELIM_CANDIDATES = [BASE_DELIM, ...Array.from({ length: 9 }, (_, index) => `${BASE_DELIM}${index + 1}`)];
const BOM = new Uint8Array([0xef, 0xbb, 0xbf]);
const UTF8_DECODER = new TextDecoder("utf-8", { fatal: true, ignoreBOM: true });
const PY_WHITESPACE = /[\t\n\v\f\r \u001c-\u001f\u0085\u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000]/;

type EmbeddedFile = [pathText: string, body: string];
type EmbeddedSkill = [name: string, description: string, files: EmbeddedFile[]];
type ParsedSkill = [name: string, description: string, body: string];
type FileStat = ReturnType<typeof statSync>;
interface CliOptions {
  skills: string;
  output: string;
}

function die(errors: string[]): never {
  for (const error of errors) {
    console.error(`[embed_skills] 错误: ${error}`);
  }
  process.exit(1);
}

function usageError(message: string): never {
  console.error(`[embed_skills] 参数错误: ${message}`);
  console.error("用法: bun tools/embed_skills.ts --skills <dir> --output <path>");
  process.exit(2);
}

function parseArgs(argv: string[]): CliOptions {
  let skills: string | undefined;
  let output: string | undefined;
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--skills") {
      index += 1;
      if (index >= argv.length) usageError("--skills 缺少值");
      skills = argv[index];
    } else if (arg.startsWith("--skills=")) {
      skills = arg.slice("--skills=".length);
    } else if (arg === "--output") {
      index += 1;
      if (index >= argv.length) usageError("--output 缺少值");
      output = argv[index];
    } else if (arg.startsWith("--output=")) {
      output = arg.slice("--output=".length);
    } else if (arg === "-h" || arg === "--help") {
      console.log("用法: bun tools/embed_skills.ts --skills <dir> --output <path>");
      process.exit(0);
    } else {
      usageError(`未知参数: ${arg}`);
    }
  }
  if (skills === undefined) usageError("缺少必填参数 --skills");
  if (output === undefined) usageError("缺少必填参数 --output");
  return { skills, output };
}

function readBytes(pathText: string): Buffer {
  return readFileSync(pathText);
}

function statPath(pathText: string): FileStat | undefined {
  try {
    return statSync(pathText);
  } catch {
    return undefined;
  }
}

function isDirectory(pathText: string): boolean {
  return statPath(pathText)?.isDirectory() === true;
}

function isFile(pathText: string): boolean {
  return statPath(pathText)?.isFile() === true;
}

function startsWithBytes(data: Uint8Array, prefix: Uint8Array): boolean {
  if (data.length < prefix.length) return false;
  for (let index = 0; index < prefix.length; index += 1) {
    if (data[index] !== prefix[index]) return false;
  }
  return true;
}

function bufferEquals(left: Uint8Array, right: Uint8Array): boolean {
  if (left.length !== right.length) return false;
  for (let index = 0; index < left.length; index += 1) {
    if (left[index] !== right[index]) return false;
  }
  return true;
}

function splitBytes(data: Buffer, separator: Buffer): Buffer[] {
  const parts: Buffer[] = [];
  let start = 0;
  let found = data.indexOf(separator, start);
  while (found !== -1) {
    parts.push(data.subarray(start, found));
    start = found + separator.length;
    found = data.indexOf(separator, start);
  }
  parts.push(data.subarray(start));
  return parts;
}

function decodeUtf8(data: Uint8Array): string | undefined {
  try {
    return UTF8_DECODER.decode(data);
  } catch {
    return undefined;
  }
}

function pyStrip(text: string): string {
  let start = 0;
  let end = text.length;
  while (start < end && PY_WHITESPACE.test(text[start])) start += 1;
  while (end > start && PY_WHITESPACE.test(text[end - 1])) end -= 1;
  return text.slice(start, end);
}

function pyRepr(value: string): string {
  let result = "'";
  for (const char of value) {
    const code = char.codePointAt(0) ?? 0;
    if (char === "\\") result += "\\\\";
    else if (char === "'") result += "\\'";
    else if (char === "\n") result += "\\n";
    else if (char === "\r") result += "\\r";
    else if (char === "\t") result += "\\t";
    else if (char === "\b") result += "\\b";
    else if (char === "\f") result += "\\f";
    else if (code < 0x20 || code === 0x7f) result += `\\x${code.toString(16).padStart(2, "0")}`;
    else result += char;
  }
  return result + "'";
}

function unescapeQuoted(inner: string, label: string, errors: string[]): string | undefined {
  const output: string[] = [];
  let index = 0;
  while (index < inner.length) {
    const char = inner[index];
    if (char === "\\") {
      if (index + 1 >= inner.length) {
        errors.push(`${label}: 字符串尾部悬空转义`);
        return undefined;
      }
      const next = inner[index + 1];
      if (next === '"' || next === "\\") {
        output.push(next);
        index += 2;
      } else {
        errors.push(`${label}: 非法转义 \\${next}`);
        return undefined;
      }
    } else {
      output.push(char);
      index += 1;
    }
  }
  return output.join("");
}

function parseSkillMd(data: Buffer, label: string, errors: string[]): ParsedSkill | undefined {
  if (startsWithBytes(data, BOM)) {
    errors.push(`${label}: 含 UTF-8 BOM`);
    return undefined;
  }
  const newline = startsWithBytes(data, Buffer.from("---\r\n", "utf8")) ? "\r\n" : "\n";
  if (!startsWithBytes(data, Buffer.from(`---${newline}`, "utf8"))) {
    errors.push(`${label}: 首行须为 ---`);
    return undefined;
  }
  const lines = splitBytes(data, Buffer.from(newline, "utf8"));
  const endMarker = Buffer.from("---", "utf8");
  let end = -1;
  for (let index = 1; index < lines.length; index += 1) {
    if (bufferEquals(lines[index], endMarker)) {
      end = index;
      break;
    }
  }
  if (end === -1) {
    errors.push(`${label}: frontmatter 缺少结束 ---`);
    return undefined;
  }
  const fields = new Map<string, string>();
  for (const raw of lines.slice(1, end)) {
    const text = decodeUtf8(raw);
    if (text === undefined) {
      errors.push(`${label}: frontmatter 非 UTF-8`);
      return undefined;
    }
    if (pyStrip(text) === "") {
      errors.push(`${label}: frontmatter 内不允许空行`);
      return undefined;
    }
    const colon = text.indexOf(":");
    if (colon === -1) {
      errors.push(`${label}: frontmatter 行缺键: ${pyRepr(text)}`);
      return undefined;
    }
    const key = pyStrip(text.slice(0, colon));
    const value = pyStrip(text.slice(colon + 1));
    if (fields.has(key)) {
      errors.push(`${label}: frontmatter 键重复: ${pyRepr(key)}`);
      return undefined;
    }
    if (key !== "name" && key !== "description") {
      errors.push(`${label}: frontmatter 未知键: ${pyRepr(key)}`);
      return undefined;
    }
    fields.set(key, value);
  }
  for (const key of ["name", "description"]) {
    if (!fields.has(key)) {
      errors.push(`${label}: frontmatter 缺键: ${pyRepr(key)}`);
      return undefined;
    }
  }
  const name = fields.get("name") ?? "";
  if (name.length >= 2 && name[0] === '"' && name[name.length - 1] === '"') {
    errors.push(`${label}: name 须为裸字符串`);
    return undefined;
  }
  const descriptionRaw = fields.get("description") ?? "";
  if (!(descriptionRaw.length >= 2 && descriptionRaw[0] === '"' && descriptionRaw[descriptionRaw.length - 1] === '"')) {
    errors.push(`${label}: description 须为双引号字符串`);
    return undefined;
  }
  const description = unescapeQuoted(descriptionRaw.slice(1, -1), label, errors);
  if (description === undefined) {
    return undefined;
  }
  let rest = lines.slice(end + 1);
  while (rest.length > 0 && rest[0].length === 0) {
    rest = rest.slice(1);
  }
  const joined = rest.length === 0
    ? Buffer.alloc(0)
    : Buffer.concat(rest.flatMap((line, index) => (index === 0 ? [line] : [Buffer.from(newline, "utf8"), line])));
  const body = decodeUtf8(joined);
  if (body === undefined) {
    errors.push(`${label}: 正文非 UTF-8`);
    return undefined;
  }
  return [name, description, body];
}

function compareNames(left: string, right: string): number {
  if (left < right) return -1;
  if (left > right) return 1;
  return 0;
}

function comparePaths(left: string, right: string): number {
  const leftParts = left.split(path.sep);
  const rightParts = right.split(path.sep);
  const shared = Math.min(leftParts.length, rightParts.length);
  for (let index = 0; index < shared; index += 1) {
    const leftPart = process.platform === "win32" ? leftParts[index].toLowerCase() : leftParts[index];
    const rightPart = process.platform === "win32" ? rightParts[index].toLowerCase() : rightParts[index];
    const compared = compareNames(leftPart, rightPart);
    if (compared !== 0) return compared;
  }
  return leftParts.length - rightParts.length;
}

function matchesFull(pattern: RegExp, value: string): boolean {
  const match = pattern.exec(value);
  return match !== null && match[0] === value;
}

function collectFiles(directory: string): string[] {
  const files: string[] = [];
  for (const name of readdirSync(directory).sort(compareNames)) {
    const full = path.join(directory, name);
    const stat = statPath(full);
    if (stat === undefined) continue;
    if (stat.isDirectory()) {
      files.push(...collectFiles(full));
    } else if (stat.isFile()) {
      files.push(full);
    }
  }
  return files;
}

function validateAndLoad(skillsDir: string): { entries: EmbeddedSkill[]; errors: string[] } {
  const errors: string[] = [];
  if (!isDirectory(skillsDir)) {
    die([`skills 目录不存在: ${skillsDir}`]);
  }

  const entries: EmbeddedSkill[] = [];
  const seenNames = new Set<string>();
  const referenced = new Set<string>();
  for (const childName of readdirSync(skillsDir).sort(compareNames)) {
    const child = path.join(skillsDir, childName);
    if (!isDirectory(child)) {
      errors.push(`skills/ 根下不允许零散文件: ${pyRepr(childName)}`);
      continue;
    }
    const name = childName;
    const label = `skills/${name}`;
    if (!matchesFull(NAME_PATTERN, name)) {
      errors.push(`${label}: 目录名非法（须匹配 ^[a-z0-9]+(-[a-z0-9]+)*$）`);
    } else if ([...name].length > MAX_NAME_LEN) {
      errors.push(`${label}: 目录名超长: ${[...name].length} > ${MAX_NAME_LEN}`);
    } else if (seenNames.has(name)) {
      errors.push(`${label}: skill 重复`);
    } else {
      seenNames.add(name);
    }

    const skillMd = path.join(child, "SKILL.md");
    if (!isFile(skillMd)) {
      errors.push(`${label}: 缺 SKILL.md`);
      continue;
    }
    const parsed = parseSkillMd(readBytes(skillMd), `${label}/SKILL.md`, errors);
    if (parsed === undefined) {
      continue;
    }
    const [parsedName, description, body] = parsed;
    if (parsedName !== name) {
      errors.push(`${label}/SKILL.md: frontmatter name ${pyRepr(parsedName)} 与目录名不一致`);
      continue;
    }
    if (description === "") {
      errors.push(`${label}/SKILL.md: description 须为非空字符串`);
      continue;
    }
    if ([...description].length > MAX_DESCRIPTION_LEN) {
      errors.push(`${label}/SKILL.md: description 超长: ${[...description].length} > ${MAX_DESCRIPTION_LEN}`);
      continue;
    }
    referenced.add(skillMd);

    const files: EmbeddedFile[] = [["SKILL.md", body]];
    let filesOk = true;
    for (const subName of readdirSync(child).sort(compareNames)) {
      const sub = path.join(child, subName);
      if (sub === skillMd) {
        continue;
      }
      if (isDirectory(sub)) {
        if (subName !== "references" && subName !== "scripts") {
          errors.push(`${label}: 未知子目录: ${pyRepr(subName)}`);
          filesOk = false;
          continue;
        }
        for (const itemName of readdirSync(sub).sort(compareNames)) {
          const item = path.join(sub, itemName);
          if (!isFile(item)) {
            errors.push(`${label}/${subName}: 不允许嵌套目录: ${pyRepr(itemName)}`);
            filesOk = false;
            continue;
          }
          const relative = `${subName}/${itemName}`;
          if (subName === "references") {
            if (!itemName.endsWith(".md")) {
              errors.push(`${label}: ${relative} 须为 .md 文件`);
              filesOk = false;
              continue;
            }
          } else if (!itemName.endsWith(".mjs")) {
            errors.push(`${label}: ${relative} 须为 .mjs 文件`);
            filesOk = false;
            continue;
          }
          const raw = readBytes(item);
          if (startsWithBytes(raw, BOM)) {
            errors.push(`${label}: ${relative} 含 UTF-8 BOM`);
            filesOk = false;
            continue;
          }
          const decoded = decodeUtf8(raw);
          if (decoded === undefined) {
            errors.push(`${label}: ${relative} 非 UTF-8`);
            filesOk = false;
            continue;
          }
          files.push([relative, decoded]);
          referenced.add(item);
        }
      } else {
        errors.push(`${label}: 根下不允许零散文件: ${pyRepr(subName)}`);
        filesOk = false;
      }
    }
    if (filesOk) {
      entries.push([name, description, files]);
    }
  }

  if (entries.length !== SKILL_COUNT) {
    errors.push(`skills 须恰 ${SKILL_COUNT} 个，实际 ${entries.length} 个`);
  }

  for (const file of collectFiles(skillsDir).sort(comparePaths)) {
    if (!referenced.has(file)) {
      const relative = path.relative(skillsDir, file).split(path.sep).join("/");
      errors.push(`存在未被引用的孤儿文件: ${pyRepr(relative)}`);
    }
  }

  return { entries, errors };
}

function cppStringLiteral(text: string): string {
  let result = '"';
  for (let index = 0; index < text.length; index += 1) {
    const char = text[index];
    const code = text.charCodeAt(index);
    if (char === '"') result += '\\"';
    else if (char === "\\") result += "\\\\";
    else if (char === "\b") result += "\\b";
    else if (char === "\f") result += "\\f";
    else if (char === "\n") result += "\\n";
    else if (char === "\r") result += "\\r";
    else if (char === "\t") result += "\\t";
    else if (code < 0x20) result += `\\u${code.toString(16).padStart(4, "0")}`;
    else result += char;
  }
  return result + '"';
}

function chooseDelimiter(entries: EmbeddedSkill[]): string {
  const texts = entries.map(([, description]) => description);
  texts.push(...entries.flatMap(([, , files]) => files.map(([, body]) => body)));
  for (const candidate of DELIM_CANDIDATES) {
    const closer = `)${candidate}"`;
    if (!texts.some((text) => text.includes(closer))) {
      return candidate;
    }
  }
  die([`定界符候选 [${DELIM_CANDIDATES.map(pyRepr).join(", ")}] 全部与待嵌入内容碰撞，请人工处理`]);
}

function renderHeader(entries: EmbeddedSkill[], delimiter: string): string {
  const lines = [
    "// 由 tools/embed_skills.py 生成，勿手改。",
    "// 源：skills/ 的各册目录（SKILL.md 与 references/、scripts/ 下的文件）",
    "#pragma once",
    "#include <string>",
    "#include <vector>",
    "",
    "namespace godot_autopilot::skill_gen::embedded {",
    "",
    "struct EmbeddedFile {",
    "  const char *path; // 相对路径：SKILL.md 或 references/、scripts/ 下的文件",
    "  const char *body;",
    "};",
    "",
    "struct EmbeddedSkill {",
    "  const char *name; // 与目标目录名一致",
    "  const char *description;",
    "  std::vector<EmbeddedFile> files; // files[0] 必为 SKILL.md",
    "};",
    "",
    "inline const std::vector<EmbeddedSkill> &all() {",
    "  static const std::vector<EmbeddedSkill> skills = {",
  ];
  for (const [name, description, files] of entries) {
    lines.push(`      {${cppStringLiteral(name)}, R"${delimiter}(${description})${delimiter}",`);
    const parts = files.map(
      ([filePath, body]) => `{${cppStringLiteral(filePath)}, R"${delimiter}(${body})${delimiter}"}`,
    );
    lines.push("       {" + parts.join(",\n        ") + "}},");
  }
  lines.push(
    "  };",
    "  return skills;",
    "}",
    "",
    "} // namespace godot_autopilot::skill_gen::embedded",
  );
  return lines.join("\n") + "\n";
}

function main(): void {
  const options = parseArgs(process.argv.slice(2));
  const skillsDir = path.normalize(options.skills);
  const outputPath = path.normalize(options.output);
  const { entries, errors } = validateAndLoad(skillsDir);
  if (errors.length > 0) {
    die(errors);
  }

  const delimiter = chooseDelimiter(entries);
  const content = renderHeader(entries, delimiter);
  mkdirSync(path.dirname(outputPath), { recursive: true });
  writeFileSync(outputPath, content, "utf8");

  const fileCount = entries.reduce((total, [, , files]) => total + files.length, 0);
  console.log(`[embed_skills] 嵌入 ${entries.length} 册 skill / ${fileCount} 个文件，定界符 '${delimiter}' -> ${outputPath}`);
}

main();
