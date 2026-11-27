import { describe, expect, test } from "bun:test";

import { usageText } from "../../src/args";
import { COMMANDS } from "../../src/commands/index";

const CURRENT_COMMANDS = [
  "batch",
  "cache",
  "call",
  "categories",
  "describe",
  "docs",
  "install",
  "list",
  "logs",
  "monitor",
  "ping",
  "projects",
  "restart",
  "search",
  "skills",
  "start",
  "status",
  "stop",
  "tools",
  "trace",
  "update",
];

function documentedCommands(): string[] {
  const section = usageText().split("\nGlobal options:\n")[0] ?? "";
  const commands = new Set<string>();
  for (const match of section.matchAll(/^  ([a-z][a-z0-9-]*)(?:\s|$)/gm)) {
    if (match[1] !== "gda") commands.add(match[1]);
  }
  return [...commands].sort();
}

describe("CLI command manifest", () => {
  test("top-level registry matches the current surface snapshot", () => {
    expect(Object.keys(COMMANDS).sort()).toEqual(CURRENT_COMMANDS);
  });

  test("help text exposes the same top-level commands as the registry", () => {
    expect(documentedCommands()).toEqual(CURRENT_COMMANDS);
  });

  test("every manifest entry has a runnable handler", () => {
    expect(Object.values(COMMANDS).every((command) => typeof command.run === "function")).toBe(true);
  });
});
