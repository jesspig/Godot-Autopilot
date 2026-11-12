import fs from "node:fs";
import type { Dirent } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const ROOT = path.resolve(fileURLToPath(new URL("../..", import.meta.url)));

export function toPosix(value: string): string {
  return value.split(path.sep).join("/");
}

export function comparePosix(a: string, b: string): number {
  const left = toPosix(a);
  const right = toPosix(b);
  if (left < right) return -1;
  if (left > right) return 1;
  return 0;
}

export function relativeWithin(from: string, to: string): string | null {
  const rel = path.relative(from, to);
  if (rel === "") return "";
  if (rel === ".." || rel.startsWith(`..${path.sep}`) || path.isAbsolute(rel)) return null;
  return rel;
}

export function relOf(target: string): string {
  return toPosix(relativeWithin(ROOT, target) ?? path.relative(ROOT, target));
}

export function isDirectory(target: string): boolean {
  try {
    return fs.statSync(target).isDirectory();
  } catch {
    return false;
  }
}

export function walkFiles(root: string): string[] {
  const found: string[] = [];
  const stack: string[] = [root];
  while (stack.length > 0) {
    const current = stack.pop() as string;
    let entries: Dirent[];
    try {
      entries = fs.readdirSync(current, { withFileTypes: true });
    } catch {
      continue;
    }
    for (const entry of entries) {
      const full = path.join(current, entry.name);
      if (entry.isDirectory()) stack.push(full);
      else if (entry.isFile()) found.push(full);
    }
  }
  found.sort(comparePosix);
  return found;
}

export function readBytes(target: string): Buffer {
  return fs.readFileSync(target);
}

export function decodeUtf8Strict(data: Uint8Array): string {
  return new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(data);
}

export function escapeRegExp(value: string): string {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
