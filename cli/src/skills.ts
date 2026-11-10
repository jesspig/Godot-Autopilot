import fs from "node:fs";
import path from "node:path";

import { CliError, ExitCode } from "./errors";

export interface SkillSummary {
  name: string;
  path: string;
  files: number;
}

export interface SkillsRoot {
  path: string;
  source: "repo" | "project";
}

function countFiles(dir: string): number {
  let total = 0;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    if (entry.isDirectory()) {
      total += countFiles(path.join(dir, entry.name));
    } else if (entry.isFile()) {
      total += 1;
    }
  }
  return total;
}

export function resolveSkillsRoot(): SkillsRoot {
  const candidates: SkillsRoot[] = [
    { path: path.resolve(import.meta.dir, "..", "..", "skills"), source: "repo" },
    { path: path.join(process.cwd(), ".agents", "skills"), source: "project" },
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(candidate.path) && fs.statSync(candidate.path).isDirectory()) {
      return candidate;
    }
  }
  throw new CliError(
    ExitCode.Transport,
    "no skills directory found (the repository skills/ and this project's .agents/skills/ are both missing)",
  );
}

export function listSkills(root: SkillsRoot): SkillSummary[] {
  const books: SkillSummary[] = [];
  for (const entry of fs.readdirSync(root.path, { withFileTypes: true })) {
    if (!entry.isDirectory()) {
      continue;
    }
    const dir = path.join(root.path, entry.name);
    if (!fs.existsSync(path.join(dir, "SKILL.md"))) {
      continue;
    }
    books.push({ name: entry.name, path: dir, files: countFiles(dir) });
  }
  books.sort((a, b) => a.name.localeCompare(b.name));
  return books;
}

export function readSkillFile(root: SkillsRoot, name: string, relative?: string): string {
  if (!/^[A-Za-z0-9._-]+$/.test(name)) {
    throw new CliError(ExitCode.Usage, `invalid skill name: ${name}`);
  }
  const bookDir = path.join(root.path, name);
  if (!fs.existsSync(bookDir) || !fs.statSync(bookDir).isDirectory()) {
    throw new CliError(ExitCode.Usage, `unknown skill: ${name} (run 'gda skills list')`);
  }
  const target = relative === undefined ? "SKILL.md" : relative;
  const resolved = path.resolve(bookDir, target);
  const normalizedBook = path.resolve(bookDir) + path.sep;
  if (!resolved.startsWith(normalizedBook)) {
    throw new CliError(ExitCode.Usage, `--file must stay inside the skill directory: ${relative}`);
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    throw new CliError(ExitCode.Usage, `skill file not found: ${name}/${target}`);
  }
  return fs.readFileSync(resolved, "utf8");
}