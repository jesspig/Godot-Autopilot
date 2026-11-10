import fs from "node:fs";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { catalogPath, readCatalog, writeCatalog } from "../src/catalog";
import { run as runTools } from "../src/commands/tools";
import { CliError, ExitCode } from "../src/errors";

function captureOutput(): { stdout(): string; restore(): void } {
  const origOut = process.stdout.write;
  let outBuf = "";
  process.stdout.write = ((chunk: unknown) => {
    outBuf += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return {
    stdout: () => outBuf,
    restore: () => {
      process.stdout.write = origOut;
    },
  };
}

async function runCaptured(positional: string[], opts: GlobalOptions) {
  const cap = captureOutput();
  try {
    const code = await runTools(positional, opts);
    return { code, stdout: cap.stdout() };
  } finally {
    cap.restore();
  }
}

const SAMPLE_TOOLS = [
  {
    name: "create_scene_node",
    category: "scene",
    tags: ["node", "create"],
    side_effect: "write",
    description: "Create a node in the edited scene",
    input_schema: { type: "object" },
    api_token: "should-not-be-cached",
  },
  {
    name: "capture_editor_viewport",
    category: "capture",
    tags: ["image"],
    side_effect: "none",
    description: "Capture the editor viewport",
    input_schema: { type: "object" },
  },
];

let tmpHome: string;
let prevHome: string | undefined;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmpHome = fs.mkdtempSync(path.join(base, "gda-tools-test-"));
  prevHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmpHome;
});

afterEach(() => {
  if (prevHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_CLI_HOME = prevHome;
  }
  fs.rmSync(tmpHome, { recursive: true, force: true });
});

describe("tools (offline catalog)", () => {
  test("list 读取缓存并输出工具", async () => {
    writeCatalog(SAMPLE_TOOLS);
    const result = await runCaptured(["list"], {});
    expect(result.code).toBe(ExitCode.Success);
    const catalog = JSON.parse(result.stdout) as { tools: Array<{ name: string }> };
    expect(catalog.tools.map((tool) => tool.name)).toEqual([
      "create_scene_node",
      "capture_editor_viewport",
    ]);
    expect(typeof (catalog as { generated_at?: unknown }).generated_at).toBe("string");
    expect(typeof (catalog as { version?: unknown }).version).toBe("string");
  });

  test("缓存不含 api_token 等敏感字段", () => {
    writeCatalog(SAMPLE_TOOLS);
    const catalog = readCatalog();
    expect(catalog).not.toBeNull();
    expect(JSON.stringify(catalog)).not.toContain("api_token");
    expect(JSON.stringify(catalog)).not.toContain("should-not-be-cached");
    expect(fs.existsSync(catalogPath())).toBe(true);
  });

  test("search 按名称描述类别标签过滤", async () => {
    writeCatalog(SAMPLE_TOOLS);
    const result = await runCaptured(["search", "capture"], {});
    expect(result.code).toBe(ExitCode.Success);
    const catalog = JSON.parse(result.stdout) as { tools: Array<{ name: string }> };
    expect(catalog.tools.map((tool) => tool.name)).toEqual(["capture_editor_viewport"]);
  });

  test("describe 输出单个工具 schema", async () => {
    writeCatalog(SAMPLE_TOOLS);
    const result = await runCaptured(["describe", "create_scene_node"], {});
    expect(result.code).toBe(ExitCode.Success);
    const tool = JSON.parse(result.stdout) as { name: string; input_schema?: unknown };
    expect(tool.name).toBe("create_scene_node");
    expect(tool.input_schema).toEqual({ type: "object" });
  });

  test("缓存缺失报传输错误并提示刷新", async () => {
    let caught: unknown;
    try {
      await runCaptured(["list"], {});
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect((caught as CliError).message).toContain("--refresh");
  });

  test("describe 未知工具报用法错误", async () => {
    writeCatalog(SAMPLE_TOOLS);
    let caught: unknown;
    try {
      await runCaptured(["describe", "no_such_tool"], {});
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Usage);
  });
});