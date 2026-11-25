import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { CliError, ExitCode } from "./errors";
import { isSafeArchiveEntry } from "./security-policy";

function archiveEntries(zipPath: string): string[] {
  const tar = Bun.spawnSync(["tar", "-tf", zipPath], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (tar.exitCode === 0) {
    return tar.stdout.toString().split(/\r?\n/).filter((line) => line.length > 0);
  }
  const unzip = Bun.spawnSync(["unzip", "-Z1", zipPath], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (unzip.exitCode === 0) {
    return unzip.stdout.toString().split(/\r?\n/).filter((line) => line.length > 0);
  }
  return [];
}

export function extractZip(zipPath: string, destDir: string): void {
  assertNoZipSlip(destDir);
  for (const entry of archiveEntries(zipPath)) {
    if (!isSafeArchiveEntry(entry)) {
      throw new CliError(ExitCode.Transport, `unsafe path in archive: ${entry}`);
    }
  }
  fs.mkdirSync(destDir, { recursive: true });
  const staging = fs.mkdtempSync(path.join(os.tmpdir(), "gda-extract-"));
  try {
    const tar = Bun.spawnSync(["tar", "-xf", zipPath, "-C", staging], {
      stdout: "pipe",
      stderr: "pipe",
    });
    if (tar.exitCode !== 0) {
      const unzip = Bun.spawnSync(["unzip", "-q", zipPath, "-d", staging], {
        stdout: "pipe",
        stderr: "pipe",
      });
      if (unzip.exitCode !== 0) {
        throw new CliError(
          ExitCode.Transport,
          `cannot extract ${zipPath}: ${tar.stderr.toString() || unzip.stderr.toString()}`,
          "Install tar or unzip, or pass an already extracted directory.",
        );
      }
    }
    assertNoZipSlip(staging);
    copyRegularTree(staging, destDir);
  } finally {
    fs.rmSync(staging, { recursive: true, force: true });
  }
}

function copyRegularTree(source: string, target: string): void {
  fs.mkdirSync(target, { recursive: true });
  for (const entry of fs.readdirSync(source, { withFileTypes: true })) {
    const from = path.join(source, entry.name);
    const to = path.join(target, entry.name);
    if (entry.isSymbolicLink()) continue;
    if (entry.isDirectory()) {
      copyRegularTree(from, to);
      continue;
    }
    if (entry.isFile()) {
      fs.mkdirSync(path.dirname(to), { recursive: true });
      const tmp = `${to}.${process.pid}.tmp`;
      fs.writeFileSync(tmp, fs.readFileSync(from));
      fs.renameSync(tmp, to);
    }
  }
}

function assertNoZipSlip(destDir: string): void {
  let root: string;
  try {
    root = fs.realpathSync(path.resolve(destDir));
  } catch {
    return;
  }
  const walk = (dir: string): void => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      const real = fs.realpathSync(full);
      const relative = path.relative(root, real);
      if (relative === ".." || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
        throw new CliError(ExitCode.Transport, `unsafe path in archive: ${entry.name}`);
      }
      if (entry.isDirectory() && !entry.isSymbolicLink()) walk(full);
    }
  };
  walk(root);
}

export function readTree(root: string): Record<string, string> {
  const files: Record<string, string> = {};
  const walk = (dir: string, prefix: string): void => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      const rel = prefix === "" ? entry.name : `${prefix}/${entry.name}`;
      if (entry.isDirectory()) walk(full, rel);
      else if (entry.isFile()) files[rel] = fs.readFileSync(full, "utf8");
    }
  };
  walk(root, "");
  return files;
}

export function writeTree(
  targetRoot: string,
  files: Record<string, string>,
  force: boolean,
): void {
  for (const [relative, content] of Object.entries(files)) {
    const normalized = path.posix.normalize(relative);
    if (normalized.startsWith("..") || path.posix.isAbsolute(normalized)) {
      throw new CliError(ExitCode.Transport, `unsafe path in archive: ${relative}`);
    }
    const target = path.join(targetRoot, ...normalized.split("/"));
    if (fs.existsSync(target) && !force) {
      const existing = fs.readFileSync(target, "utf8");
      if (existing !== content) {
        throw new CliError(
          ExitCode.Transport,
          `target already exists with different content: ${target}`,
          "Pass --force to overwrite.",
        );
      }
      continue;
    }
    fs.mkdirSync(path.dirname(target), { recursive: true });
    const tmp = `${target}.${process.pid}.tmp`;
    fs.writeFileSync(tmp, content, "utf8");
    fs.renameSync(tmp, target);
  }
}
