import path from "node:path";

import { describe, expect, test } from "bun:test";

import {
  canApplyUpdateTo,
  isPathInside,
  isSafeArchiveEntry,
  isSafeSkillName,
  isSafeTaskId,
  matchesProcessIdentity,
  normalizedPathKey,
} from "../src/security-policy";
import { safeTaskFileName } from "../src/tasks-index";

describe("archive entry validation", () => {
  test("accepts relative entries", () => {
    expect(isSafeArchiveEntry("inside/safe.txt")).toBe(true);
    expect(isSafeArchiveEntry("inside/")).toBe(true);
  });

  test("rejects traversal, absolute and drive paths", () => {
    expect(isSafeArchiveEntry("../escape")).toBe(false);
    expect(isSafeArchiveEntry("a/../../escape")).toBe(false);
    expect(isSafeArchiveEntry("/etc/passwd")).toBe(false);
    expect(isSafeArchiveEntry("C:/Windows/system32")).toBe(false);
    expect(isSafeArchiveEntry("..\\escape")).toBe(false);
    expect(isSafeArchiveEntry("a\0b")).toBe(false);
  });
});

describe("task and skill names", () => {
  test("task ids are whitelisted and hashed when unsafe", () => {
    expect(isSafeTaskId("task-1")).toBe(true);
    expect(isSafeTaskId("../escape")).toBe(false);
    expect(safeTaskFileName("task-1")).toBe("task-1.json");
    expect(safeTaskFileName("../escape")).toMatch(/^[0-9a-f]{32}\.json$/);
  });

  test("skill names reject dot segments", () => {
    expect(isSafeSkillName("gda-game-develop")).toBe(true);
    expect(isSafeSkillName(".")).toBe(false);
    expect(isSafeSkillName("..")).toBe(false);
    expect(isSafeSkillName("../outside")).toBe(false);
  });
});

describe("path containment", () => {
  test("recognises nested paths and rejects sibling prefixes", () => {
    const root = path.join(path.sep, "project", "skills");
    expect(isPathInside(root, path.join(root, "book", "SKILL.md"))).toBe(true);
    expect(isPathInside(root, root)).toBe(true);
    expect(isPathInside(root, path.join(root, "..", "skills-backup"))).toBe(false);
  });

  test("normalises slash style and trailing separators", () => {
    expect(normalizedPathKey("C:\\project\\skills\\")).toBe("C:/project/skills");
  });
});

describe("process identity", () => {
  test("rejects pid reuse by start time", () => {
    expect(matchesProcessIdentity(
      { pid: 42, started_at: 1000 },
      { pid: 42, started_at: 2000 },
    )).toBe(false);
    expect(matchesProcessIdentity(
      { pid: 42, started_at: 1000 },
      { pid: 42, started_at: 1000 },
    )).toBe(true);
    expect(matchesProcessIdentity({ pid: 42 }, { pid: 43 })).toBe(false);
  });
});

describe("update target restriction", () => {
  test("only the running executable is a valid target", () => {
    const current = path.join(path.sep, "app", "gda.exe");
    expect(canApplyUpdateTo(current, current)).toBe(true);
    expect(canApplyUpdateTo(current, undefined)).toBe(true);
    expect(canApplyUpdateTo(current, path.join(path.sep, "app", "other.exe"))).toBe(false);
  });
});
