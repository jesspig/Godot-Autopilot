import { describe, expect, test } from "bun:test";

import { usageText } from "../src/args";
import {
  buildManifest,
  COMMAND_ALIASES,
  COMMAND_SPECS,
  FLAG_MIGRATIONS,
  resolveCommand,
} from "../src/command-spec";

describe("Alias 与迁移映射", () => {
  test("所有旧 alias 仍指向已注册 spec", () => {
    for (const [from, to] of COMMAND_ALIASES) {
      const resolved = resolveCommand(from.split(" "));
      expect(resolved).toBeDefined();
      expect(resolved!.path).toBe(to);
    }
  });

  test("每个旧顶层命令保留 alias", () => {
    for (const name of [
      "ping", "status", "categories", "search", "describe", "call", "batch", "list",
      "start", "stop", "restart", "logs", "docs", "skills", "tools",
      "projects", "cache", "install", "update", "trace", "monitor",
    ]) {
      expect(COMMAND_ALIASES.has(name)).toBe(true);
    }
  });

  test("已退役 flag 迁移映射完整", () => {
    const entries = new Map(FLAG_MIGRATIONS.map((entry) => [entry.from, entry.to]));
    expect(entries.get("--direct")).toBe("gda tool call");
    expect(entries.get("--input")).toBe("--confirm");
    expect(entries.get("--async")).toBe("gda job");
    expect(entries.get("--json")).toBe("--format json");
    for (const removed of ["--force", "--all", "--file", "--probe"]) {
      expect(entries.get(removed)).not.toBe("");
    }
  });

  test("公开帮助与 manifest 迁移信息一致", () => {
    const help = usageText();
    for (const removed of ["--direct", "--input", "--async"]) {
      expect(help).not.toMatch(new RegExp(`(^|\\s)${removed}(?=\\s|$)`));
    }
    expect(help).toContain("--format <json>");
    const manifest = buildManifest();
    expect(manifest.schema_version).toBe(1);
    expect(manifest.command_count).toBe(COMMAND_SPECS.length);
    expect(manifest.flag_migrations.length).toBe(FLAG_MIGRATIONS.length);
    expect(manifest.exit_codes.map((entry) => entry.code)).toEqual([0, 1, 2, 3, 4, 5, 130]);
  });
});
