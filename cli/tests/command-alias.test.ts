import { describe, expect, test } from "bun:test";

import {
  COMMAND_ALIASES,
  COMMAND_SPECS,
  helpTextForTokens,
  resolveCommand,
} from "../src/command-spec";

const LEGACY_TOP_LEVEL = [
  "ping",
  "status",
  "categories",
  "search",
  "describe",
  "call",
  "batch",
  "list",
  "start",
  "stop",
  "restart",
  "logs",
  "docs",
  "skills",
  "tools",
  "resources",
  "prompts",
  "tasks",
  "projects",
  "cache",
  "install",
  "update",
  "trace",
  "monitor",
];

describe("旧顶层命令 alias", () => {
  test("每个旧顶层命令都有 alias 映射", () => {
    for (const name of LEGACY_TOP_LEVEL) {
      expect(COMMAND_ALIASES.has(name)).toBe(true);
    }
  });

  test("所有 alias 目标都解析到已注册 spec", () => {
    for (const [from, to] of COMMAND_ALIASES) {
      const resolved = resolveCommand(from.split(" "));
      expect(resolved).toBeDefined();
      expect(resolved!.path).toBe(to);
    }
  });

  test("二级 alias 优先于一级 alias", () => {
    expect(resolveCommand(["install", "plugin"])!.path).toBe("plugin.install");
    expect(resolveCommand(["install"])!.path).toBe("setup.install");
    expect(resolveCommand(["resources", "read"])!.path).toBe("resource.read");
    expect(resolveCommand(["resources"])!.path).toBe("resource.list");
  });

  test("alias 帮助指向对应新子命令", () => {
    const help = helpTextForTokens(["start"]);
    expect(help).toBeDefined();
    expect(help).toContain("Subcommand: start");
    expect(help).toContain("Invoke: gda editor start");
    const nested = helpTextForTokens(["tasks", "wait"]);
    expect(nested).toContain("Invoke: gda job wait");
  });

  test("alias 映射不为命令引入 MCP 专属 flags", () => {
    for (const entry of COMMAND_SPECS) {
      for (const flag of entry.flags) {
        expect(flag.name).not.toMatch(/mcp/i);
      }
    }
  });
});
