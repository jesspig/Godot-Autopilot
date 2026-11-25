import { describe, expect, test } from "bun:test";

import {
  COMMAND_GROUPS,
  COMMAND_SPECS,
  commandsHelpText,
  findSpec,
  groupHelpText,
  leafHelpText,
} from "../src/command-spec";

function spec(path: string) {
  const found = findSpec(path);
  if (found === undefined) throw new Error(`missing spec: ${path}`);
  return found;
}

describe("帮助输出快照", () => {
  test("editor.stop 子命令帮助由 spec 字段稳定生成", () => {
    const help = leafHelpText(spec("editor.stop"));
    const lines = help.split("\n");
    expect(lines[0]).toBe("gda editor.stop — Stop a Godot editor");
    expect(help).toContain("Usage:\n  gda editor stop [id] [flags]");
    expect(help).toContain("Subcommand: stop");
    expect(help).toContain("Group: editor");
    expect(help).toContain("Invoke: gda editor stop");
    expect(help).toContain("Tools: editor_stop");
    expect(help).toContain("Action: gcp.editor.stop");
    expect(help).toContain("Flags for editor.stop:");
    expect(help).toContain("--force");
    expect(help).toContain("Output: gda.result.v1 (envelope)");
    expect(help).toContain("Risk: high");
    expect(help).toContain("Long running: no");
  });

  test("每个 spec 有独立子命令帮助且首行与路径一致", () => {
    for (const entry of COMMAND_SPECS) {
      const help = leafHelpText(entry);
      expect(help.split("\n")[0]).toBe(`gda ${entry.path} — ${entry.description}`);
      expect(help).toContain(`Subcommand: ${entry.subcommand}`);
      expect(help).toContain(`Group: ${entry.group}`);
      expect(help).toContain(`Output: ${entry.output.schema}`);
      expect(help).toContain(`Risk: ${entry.risk}`);
    }
  });

  test("领域帮助列出全部二级动作，不把 search/filter/categories 当一级", () => {
    for (const group of COMMAND_GROUPS) {
      const help = groupHelpText(group.name);
      expect(help).toBeDefined();
      for (const entry of COMMAND_SPECS.filter((item) => item.group === group.name)) {
        expect(help).toContain(`  ${entry.subcommand}`);
      }
    }
    expect(groupHelpText("search")).toBeUndefined();
    expect(groupHelpText("filter")).toBeUndefined();
  });

  test("gda commands --help 列出所有一级领域", () => {
    const help = commandsHelpText();
    expect(help).toContain("Usage:\n  gda commands --json");
    for (const group of COMMAND_GROUPS) {
      expect(help).toContain(group.name);
    }
  });
});
