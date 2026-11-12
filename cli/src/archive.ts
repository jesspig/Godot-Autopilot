import fs from "node:fs";
import path from "node:path";

import { CliError, ExitCode } from "./errors";

export function extractZip(zipPath: string, destDir: string): void {
  fs.mkdirSync(destDir, { recursive: true });
  const tar = Bun.spawnSync(["tar", "-xf", zipPath, "-C", destDir], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (tar.exitCode === 0) return;
  const unzip = Bun.spawnSync(["unzip", "-q", zipPath, "-d", destDir], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (unzip.exitCode === 0) return;
  throw new CliError(
    ExitCode.Transport,
    `cannot extract ${zipPath}: ${tar.stderr.toString() || unzip.stderr.toString()}`,
    "Install tar or unzip, or pass an already extracted directory.",
  );
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

export async function downloadToFile(url: string, target: string, timeoutMs = 60_000): Promise<void> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, { signal: controller.signal });
    if (!response.ok) {
      throw new CliError(ExitCode.Transport, `download failed with HTTP ${response.status}: ${url}`);
    }
    fs.mkdirSync(path.dirname(target), { recursive: true });
    await Bun.write(target, await response.arrayBuffer());
  } finally {
    clearTimeout(timer);
  }
}
