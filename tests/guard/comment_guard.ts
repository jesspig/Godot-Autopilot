import path from "node:path";

import { ROOT, comparePosix, decodeUtf8Strict, isDirectory, readBytes, relOf, walkFiles } from "./guard_support";

const SCAN_DIRS = [path.join(ROOT, "src"), path.join(ROOT, "tests")];
const RAW_START = /(?:u8|u|U|L)?R"([^()\s\\]{0,16})\(/y;

function matchRawStart(text: string, index: number): RegExpExecArray | null {
  RAW_START.lastIndex = index;
  return RAW_START.exec(text);
}

function snippet(lines: string[], lineno: number): string {
  if (lineno < 1 || lineno > lines.length) return "";
  const line = lines[lineno - 1].replace(/\r+$/, "").trim();
  return Array.from(line).slice(0, 120).join("");
}

function isNamespaceMark(body: string): boolean {
  const text = body.trim();
  if (!text.startsWith("namespace")) return false;
  const rest = text.slice("namespace".length);
  return rest === "" || rest[0] === " " || rest[0] === "\t";
}

function countNewlines(text: string, start: number, end: number): number {
  let count = 0;
  for (let index = start; index < end; index += 1) {
    if (text.charCodeAt(index) === 10) count += 1;
  }
  return count;
}

type Hit = [number, string];

function scanText(text: string): Hit[] {
  const lines = text.split("\n");
  const out: Hit[] = [];
  const n = text.length;
  let index = 0;
  let lineno = 1;
  while (index < n) {
    const ch = text[index];
    if (ch === "\n") {
      lineno += 1;
      index += 1;
      continue;
    }
    if (ch === "/") {
      const next = index + 1 < n ? text[index + 1] : "";
      if (next === "/") {
        let end = text.indexOf("\n", index + 2);
        if (end === -1) end = n;
        if (!isNamespaceMark(text.slice(index + 2, end))) out.push([lineno, snippet(lines, lineno)]);
        index = end;
        continue;
      }
      if (next === "*") {
        out.push([lineno, snippet(lines, lineno)]);
        const end = text.indexOf("*/", index + 2);
        if (end === -1) break;
        lineno += countNewlines(text, index, end + 2);
        index = end + 2;
        continue;
      }
    }
    if (ch === "R" || ch === "U" || ch === "L" || ch === "u") {
      const match = matchRawStart(text, index);
      if (match !== null) {
        const closer = ")" + match[1] + '"';
        const end = text.indexOf(closer, match.index + match[0].length);
        if (end === -1) break;
        lineno += countNewlines(text, index, end + closer.length);
        index = end + closer.length;
        continue;
      }
    }
    if (ch === '"') {
      index += 1;
      while (index < n) {
        const current = text[index];
        if (current === "\\") {
          index += 2;
          continue;
        }
        if (current === '"') {
          index += 1;
          break;
        }
        if (current === "\n") {
          lineno += 1;
          index += 1;
          break;
        }
        index += 1;
      }
      continue;
    }
    if (ch === "'") {
      index += 1;
      while (index < n) {
        const current = text[index];
        if (current === "\\") {
          index += 2;
          continue;
        }
        if (current === "'") {
          index += 1;
          break;
        }
        if (current === "\n") {
          lineno += 1;
          index += 1;
          break;
        }
        index += 1;
      }
      continue;
    }
    index += 1;
  }
  return out;
}

function collectFiles(): string[] {
  const found = new Set<string>();
  for (const dir of SCAN_DIRS) {
    if (!isDirectory(dir)) continue;
    for (const file of walkFiles(dir)) {
      if (file.endsWith(".cpp") || file.endsWith(".hpp")) found.add(file);
    }
  }
  return [...found].sort((a, b) => comparePosix(relOf(a), relOf(b)));
}

function runSelfTests(): number {
  const negativeCases: Array<[string, number]> = [
    ["int x = 1; // trailing", 1],
    ["/* block\nspan */", 1],
    ["// full line", 1],
  ];
  for (const [text, wantLine] of negativeCases) {
    const hits = scanText(text);
    if (hits.length !== 1) {
      console.error(`self-test FAIL: 1 violation/${hits.length} hits: ${JSON.stringify(text)}`);
      return 1;
    }
    if (hits[0][0] !== wantLine) {
      console.error(`self-test FAIL: line ${wantLine}/line ${hits[0][0]}: ${JSON.stringify(text)}`);
      return 1;
    }
  }
  const positiveCases = [
    "// namespace",
    'const char* s = "// not a comment";',
    "char c = '/';",
    'R"(// raw)"',
    'R"TAG(// x)TAG"',
    'std::string url = "http://x";',
  ];
  for (const text of positiveCases) {
    const hits = scanText(text);
    if (hits.length > 0) {
      console.error(`self-test FAIL: no violation/${hits.length} hits: ${JSON.stringify(text)}`);
      return 1;
    }
  }
  console.log("comment-guard self-test OK");
  return 0;
}

function main(): number {
  const args = process.argv.slice(2);
  if (args.length === 1 && args[0] === "--self-test") return runSelfTests();
  const decodeErrors: string[] = [];
  const violations: string[] = [];
  for (const file of collectFiles()) {
    const rel = relOf(file);
    let data: Buffer;
    try {
      data = readBytes(file);
    } catch (error) {
      decodeErrors.push(`${rel}: read error: ${(error as Error).message}`);
      continue;
    }
    let text: string;
    try {
      text = decodeUtf8Strict(data);
    } catch (error) {
      decodeErrors.push(`${rel}: decode error: ${(error as Error).message}`);
      continue;
    }
    for (const [lineno, line] of scanText(text)) {
      violations.push(`${rel}:${lineno}: ${line}`);
    }
  }
  if (decodeErrors.length > 0) {
    for (const item of decodeErrors) console.error(item);
    return 2;
  }
  if (violations.length > 0) {
    for (const item of violations) console.log(item);
    return 1;
  }
  console.log("comment-guard OK");
  return 0;
}

process.exitCode = main();
