import { describe, expect, test } from "bun:test";

import {
  COMMAND_ALIASES,
  COMMAND_GROUPS,
  COMMAND_SPECS,
  buildAliasMap,
  buildManifest,
  completionCandidates,
  findSpec,
  isGroup,
  parseCommandArgs,
  resolveCommand,
} from "../src/command-spec";
import { CliError, ExitCode } from "../src/errors";

const EXPECTED_GROUPS = [
  "editor",
  "project",
  "tool",
  "workflow",
  "docs",
  "debug",
  "logs",
  "scene",
  "script",
  "resource",
  "game",
  "test",
  "job",
  "trace",
  "monitor",
  "setup",
  "skill",
  "plugin",
  "cache",
  "update",
  "doctor",
  "config",
  "shell",
  "completion",
];

function spec(path: string) {
  const found = findSpec(path);
  if (found === undefined) throw new Error(`missing spec: ${path}`);
  return found;
}

function usageError(path: string, argv: string[]): CliError {
  try {
    parseCommandArgs(spec(path), argv);
  } catch (error) {
    expect(error).toBeInstanceOf(CliError);
    return error as CliError;
  }
  throw new Error(`expected a usage error for ${path}`);
}

describe("命令树与 D-03", () => {
  test("一级领域恰好是 D-03 的 24 项", () => {
    expect(COMMAND_GROUPS.map((group) => group.name)).toEqual(EXPECTED_GROUPS);
    expect(buildManifest().group_count).toBe(24);
  });

  test("search/filter/categories 只能作为二级动作或 flags", () => {
    for (const name of ["search", "filter", "categories"]) {
      expect(EXPECTED_GROUPS).not.toContain(name);
      expect(isGroup(name)).toBe(false);
    }
    expect(spec("tool.search").subcommand).toBe("search");
    expect(spec("tool.categories").subcommand).toBe("categories");
  });

  test("docs/debug/logs 为一级领域", () => {
    expect(isGroup("docs")).toBe(true);
    expect(isGroup("debug")).toBe(true);
    expect(isGroup("logs")).toBe(true);
  });

  test("每个 spec 完整表达 group/subcommand/tools/action/positional/flags/output/risk/long_running/examples", () => {
    for (const entry of COMMAND_SPECS) {
      expect(entry.group).toBe(entry.path.split(".")[0]);
      expect(entry.subcommand).toBe(entry.path.split(".")[1]);
      expect(entry.name).toBe(entry.subcommand);
      expect(Array.isArray(entry.tools)).toBe(true);
      expect(entry.action.length).toBeGreaterThan(0);
      expect(Array.isArray(entry.positional)).toBe(true);
      expect(Array.isArray(entry.flags)).toBe(true);
      expect(entry.output.schema.length).toBeGreaterThan(0);
      expect(entry.output.envelope).toBe(true);
      expect(["none", "low", "high"]).toContain(entry.risk);
      expect(typeof entry.long_running).toBe("boolean");
      expect(entry.examples.length).toBeGreaterThan(0);
    }
  });

  test("manifest 单源输出名称、路径、参数、flags、输出 schema、风险与 invoke", () => {
    const manifest = buildManifest();
    expect(manifest.schema_version).toBe(1);
    expect(manifest.generated_by).toBe("cli/src/command-spec.ts");
    expect(manifest.command_count).toBe(COMMAND_SPECS.length);
    expect(manifest.commands.length).toBe(COMMAND_SPECS.length);
    for (const entry of manifest.commands) {
      expect(entry.name.length).toBeGreaterThan(0);
      expect(entry.path).toContain(".");
      expect(Array.isArray(entry.positional)).toBe(true);
      expect(Array.isArray(entry.flags)).toBe(true);
      expect(entry.output.schema.length).toBeGreaterThan(0);
      expect(["none", "low", "high"]).toContain(entry.risk);
      expect(entry.invoke).toBe(`gda ${entry.path.replace(".", " ")}`);
    }
    expect(manifest.aliases.length).toBe(COMMAND_ALIASES.size);
  });
});

describe("严格参数解析", () => {
  test("接受全局 flags 与命令位置参数", () => {
    const parsed = parseCommandArgs(spec("tool.search"), ["query", "--category", "editor", "--pretty"]);
    expect(parsed.positionals).toEqual(["query"]);
    expect(parsed.options["--category"]).toBe("editor");
    expect(parsed.options["--pretty"]).toBe(true);
  });

  test("命令专属 flag 不得越界到其他命令", () => {
    const error = usageError("tool.search", ["query", "--extends", "Node"]);
    expect(error.code).toBe(ExitCode.Usage);
    expect(error.message).toContain("unknown option --extends");
    expect(error.suggestion).toContain("gda tool.search --help");
  });

  test("MCP 专属 flag 不被 spec 接受", () => {
    const error = usageError("tool.call", ["ping", "--mcp-url", "http://localhost"]);
    expect(error.code).toBe(ExitCode.Usage);
    expect(error.message).toContain("unknown option --mcp-url");
  });

  test("互斥 flags 被拒绝", () => {
    const error = usageError("editor.stop", ["id", "--force-stop", "--wait", "5"]);
    expect(error.message).toContain("conflicts");
  });

  test("数字 flag 校验", () => {
    expect(parseCommandArgs(spec("logs.show"), ["--tail", "20"]).options["--tail"]).toBe(20);
    expect(usageError("trace.slow", ["--slow", "abc"]).message).toContain("requires a number");
  });

  test("必需位置参数缺失与多余位置参数均退出 3", () => {
    expect(usageError("tool.describe", []).code).toBe(ExitCode.Usage);
    expect(usageError("tool.describe", []).message).toContain("missing required argument <tool>");
    expect(usageError("tool.describe", ["a", "b"]).message).toContain("too many positional arguments");
  });

  test("可选位置参数可省略", () => {
    expect(parseCommandArgs(spec("monitor.report"), []).positionals).toEqual([]);
  });
});

describe("alias 与补全", () => {
  test("alias 解析到新子命令路径", () => {
    expect(resolveCommand(["projects"])!.path).toBe("project.list");
    expect(resolveCommand(["tasks", "wait"])!.path).toBe("job.wait");
    expect(resolveCommand(["editor", "restart"])!.path).toBe("editor.restart");
  });

  test("重复 alias 与不存在目标的 alias 被拒绝", () => {
    expect(() => buildAliasMap([["ping", "tool.ping"], ["ping", "tool.call"]])).toThrow(/duplicate command alias/);
    expect(() => buildAliasMap([["legacy", "tool.missing"]])).toThrow(/alias target is not a spec/);
  });

  test("补全候选来自同一 spec", () => {
    expect(completionCandidates([])).toContain("editor");
    expect(completionCandidates(["editor"])).toContain("status");
    expect(completionCandidates(["editor", "stop", "--f"])).toContain("--force-stop");
    expect(completionCandidates(["editor", "stop", "--f"])).not.toContain("--extends");
  });
});
