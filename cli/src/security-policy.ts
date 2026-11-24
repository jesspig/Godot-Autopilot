import path from "node:path";

export function isSafeArchiveEntry(entry: string): boolean {
  if (entry.length === 0 || entry.includes("\0")) return false;
  const normalized = entry.replace(/\\/g, "/");
  if (normalized.startsWith("/") || /^[A-Za-z]:/.test(normalized)) return false;
  const segments = normalized.split("/").filter((segment) => segment.length > 0);
  return segments.length > 0 && !segments.some((segment) => segment === "..");
}

export function isSafeTaskId(taskId: string): boolean {
  return /^[A-Za-z0-9_-]{1,128}$/.test(taskId);
}

export function isSafeSkillName(name: string): boolean {
  return name !== "." && name !== ".." && /^[A-Za-z0-9_-]{1,128}$/.test(name);
}

export function normalizedPathKey(value: string): string {
  const slashed = value.replace(/\\/g, "/");
  return slashed.endsWith("/") && slashed.length > 1 ? slashed.slice(0, -1) : slashed;
}

export function isPathInside(root: string, candidate: string): boolean {
  const rootKey = normalizedPathKey(path.resolve(root));
  const candidateKey = normalizedPathKey(path.resolve(candidate));
  if (rootKey === candidateKey) return true;
  return candidateKey.startsWith(`${rootKey}/`);
}

export interface ProcessIdentityRecord {
  pid: number;
  started_at?: string | number;
  executable?: string;
}

export interface ProcessIdentityObservation {
  pid: number;
  started_at?: string | number;
  executable?: string;
}

export function matchesProcessIdentity(
  record: ProcessIdentityRecord,
  observed: ProcessIdentityObservation,
): boolean {
  if (record.pid <= 0 || observed.pid !== record.pid) return false;
  if (record.started_at !== undefined && observed.started_at !== undefined) {
    if (String(record.started_at) !== String(observed.started_at)) return false;
  }
  if (record.executable !== undefined && observed.executable !== undefined) {
    if (normalizedPathKey(record.executable) !== normalizedPathKey(observed.executable)) return false;
  }
  return true;
}

export function canApplyUpdateTo(
  currentExecutable: string,
  requestedTarget: string | undefined,
): boolean {
  if (requestedTarget === undefined) return true;
  return normalizedPathKey(path.resolve(requestedTarget)) ===
         normalizedPathKey(path.resolve(currentExecutable));
}
