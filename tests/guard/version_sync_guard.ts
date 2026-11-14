import fs from "node:fs";
import type { Dirent } from "node:fs";
import path from "node:path";

import {
  ROOT,
  decodeUtf8Strict,
  isDirectory,
  readBytes,
  relOf,
  walkFiles,
} from "./guard_support";

const VERSION_FILE = path.join(ROOT, "VERSION");
const CLI_VERSION_FILE = path.join(ROOT, "cli", "src", "version.ts");
const SKILLS_DIR = path.join(ROOT, "skills");
const REF_PATTERN = /github:jesspig\/Godot-Autopilot#([^\s"')]+)/g;
const CLI_VERSION_PATTERN = /CLI_VERSION\s*=\s*"([^"]+)"/;
const VERSION_SHAPE = /^\d+\.\d+\.\d+$/;
const ALLOWED_NON_VERSION_REFS = new Set(["develop", "<VERSION>"]);

function splitLines(text: string): string[] {
  const lines: string[] = [];
  const pattern = /\r\n|[\n\r\v\f\u001c-\u001e\u0085\u2028\u2029]/g;
  let start = 0;
  for (const match of text.matchAll(pattern)) {
    const index = match.index ?? 0;
    lines.push(text.slice(start, index));
    start = index + match[0].length;
  }
  if (start < text.length) lines.push(text.slice(start));
  return lines;
}

function readText(file: string, errors: string[], required: boolean): string | null {
  let data: Buffer;
  try {
    data = readBytes(file);
  } catch (error) {
    errors.push(`${relOf(file)}: read error: ${(error as Error).message}`);
    return null;
  }
  try {
    return decodeUtf8Strict(data);
  } catch (error) {
    if (required) errors.push(`${relOf(file)}: decode error: ${(error as Error).message}`);
    return null;
  }
}

function collectScanFiles(): Array<[string, string]> {
  const files = new Map<string, string>();
  if (isDirectory(SKILLS_DIR)) {
    for (const file of walkFiles(SKILLS_DIR)) files.set(relOf(file), file);
  }
  let rootEntries: Dirent[];
  try {
    rootEntries = fs.readdirSync(ROOT, { withFileTypes: true });
  } catch {
    rootEntries = [];
  }
  for (const entry of rootEntries) {
    if (entry.isFile() && /^(README|INSTALL).*\.md$/.test(entry.name)) {
      files.set(entry.name, path.join(ROOT, entry.name));
    }
  }
  return [...files.entries()].sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0));
}

function main(): number {
  const errors: string[] = [];
  const checked: Array<[string, number, string, string]> = [];
  let nonVersionRefs = 0;
  let version = "";

  const versionText = readText(VERSION_FILE, errors, true);
  if (versionText !== null) {
    version = versionText.trim();
    if (version === "") errors.push("VERSION: no version value found");
    else if (!VERSION_SHAPE.test(version)) errors.push(`VERSION: malformed version value: ${version}`);
    else checked.push(["VERSION", 1, version, "VERSION"]);
  }

  const cliText = readText(CLI_VERSION_FILE, errors, true);
  if (cliText !== null) {
    const match = CLI_VERSION_PATTERN.exec(cliText);
    if (match === null) {
      errors.push("cli/src/version.ts: no CLI_VERSION found");
    } else {
      const prefix = cliText.slice(0, match.index);
      const lineno = (prefix.match(/\n/g)?.length ?? 0) + 1;
      checked.push([relOf(CLI_VERSION_FILE), lineno, match[1], "CLI_VERSION"]);
    }
  }

  const scanFiles = collectScanFiles();
  for (const [rel, file] of scanFiles) {
    const text = readText(file, errors, false);
    if (text === null) continue;
    const lines = splitLines(text);
    for (let index = 0; index < lines.length; index += 1) {
      for (const match of lines[index].matchAll(REF_PATTERN)) {
        const ref = match[1];
        if (/^[0-9]/.test(ref)) {
          checked.push([rel, index + 1, ref, `${rel} pin`]);
        } else if (ALLOWED_NON_VERSION_REFS.has(ref)) {
          nonVersionRefs += 1;
        } else {
          errors.push(`${rel}:${index + 1}: unknown ref kind: ${ref}`);
        }
      }
    }
  }

  if (errors.length > 0) {
    for (const item of errors) console.error(item);
    return 2;
  }

  const mismatches = checked.filter((item) => item[2] !== version);
  if (mismatches.length > 0) {
    console.log(`expected = ${version} (VERSION)`);
    for (const [rel, lineno, value, label] of mismatches) {
      console.log(`${rel}:${lineno} = ${value} (${label})`);
    }
    return 1;
  }

  const pinCount = checked.length - 2;
  console.log(
    `version-sync-guard scanned ${scanFiles.length} files, checked ${pinCount} version pins (${checked.length} values including VERSION/CLI_VERSION), ${nonVersionRefs} non-version refs`,
  );
  console.log("version-sync-guard OK");
  return 0;
}

process.exitCode = main();
