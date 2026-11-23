import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { run as runDocs } from "../src/commands/docs";
import { writeBucket, type DocsMeta } from "../src/docs/store";
import { CliError, ExitCode } from "../src/errors";

const fixture = path.join(import.meta.dir, "fixtures", "docs", "extension_api.json");
const fixtureText = fs.readFileSync(fixture, "utf8");

let tmp: string;
let previousHome: string | undefined;

function captureOutput(): { stdout(): string; stderr(): string; restore(): void } {
  const origOut = process.stdout.write;
  const origErr = process.stderr.write;
  let outBuf = "";
  let errBuf = "";
  process.stdout.write = ((chunk: unknown) => {
    outBuf += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  process.stderr.write = ((chunk: unknown) => {
    errBuf += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  return {
    stdout: () => outBuf,
    stderr: () => errBuf,
    restore: () => {
      process.stdout.write = origOut;
      process.stderr.write = origErr;
    },
  };
}

async function runCaptured(positional: string[], opts: GlobalOptions) {
  const cap = captureOutput();
  try {
    const code = await runDocs(positional, opts);
    return { code, stdout: cap.stdout(), stderr: cap.stderr() };
  } finally {
    cap.restore();
  }
}

function meta(key: string): DocsMeta {
  return {
    schema: 1,
    key,
    engine: { major: 4, minor: 7, patch: 2, status: "stable", full_name: "Godot Engine v4.7.2.stable.official" },
    source_binary: "test",
    synced_at: new Date(0).toISOString(),
    bytes: 0,
    sha256: "",
  };
}

function expectUsage(action: () => Promise<unknown>): Promise<void> {
  return action().then(
    () => {
      throw new Error("expected a usage error");
    },
    (error: unknown) => {
      expect(error).toBeInstanceOf(CliError);
      expect((error as CliError).code).toBe(ExitCode.Usage);
    },
  );
}

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-docs-command-"));
  previousHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(() => {
  if (previousHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = previousHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

describe("docs command", () => {
  test("缺子命令与未知子命令报用法错误", async () => {
    await expectUsage(() => runDocs([], {}));
    await expectUsage(() => runDocs(["nope"], {}));
  });

  test("search 支持 --docs-file 与 limit，stdout 是纯 JSON", async () => {
    const result = await runCaptured(["search", "tween"], {
      docsFile: fixture,
      limitText: "2",
    });
    expect(result.code).toBe(ExitCode.Success);
    expect(result.stderr).toBe("");
    const payload = JSON.parse(result.stdout) as {
      cache: { status: string };
      results: Array<{ name: string }>;
      total: number;
    };
    expect(payload.cache.status).toBe("external");
    expect(payload.results.length).toBe(2);
    expect(payload.total).toBeGreaterThan(2);
  });

  test("search 支持 kind 与 class 过滤", async () => {
    const methods = await runCaptured(["search", "position"], {
      docsFile: fixture,
      kind: "method",
    });
    const methodPayload = JSON.parse(methods.stdout) as { results: Array<{ kind: string }> };
    expect(methodPayload.results.every((item) => item.kind === "method")).toBe(true);
    const scoped = await runCaptured(["search", "position"], {
      docsFile: fixture,
      classFilter: "Node2D",
    });
    const scopedPayload = JSON.parse(scoped.stdout) as { results: Array<{ className: string }> };
    expect(scopedPayload.results.every((item) => item.className === "Node2D")).toBe(true);
  });

  test("limit 上限 200 之外报用法错误", async () => {
    await expectUsage(() => runDocs(["search", "node"], { docsFile: fixture, limitText: "201" }));
  });

  test("class 默认给摘要，--section/--member 展开", async () => {
    const base = await runCaptured(["class", "Node2D"], { docsFile: fixture });
    const basePayload = JSON.parse(base.stdout) as {
      class: { counts: { methods: number }; sections?: unknown };
    };
    expect(basePayload.class.counts.methods).toBeGreaterThan(0);
    expect(basePayload.class.sections).toBeUndefined();
    const methods = await runCaptured(["class", "Node2D"], {
      docsFile: fixture,
      section: "methods",
    });
    const methodsPayload = JSON.parse(methods.stdout) as {
      class: { sections: { methods: unknown[] } };
    };
    expect(Array.isArray(methodsPayload.class.sections.methods)).toBe(true);
    const member = await runCaptured(["class", "Node2D"], {
      docsFile: fixture,
      member: "position",
    });
    const memberPayload = JSON.parse(member.stdout) as {
      class: { members: Array<{ name: string; kind: string }> };
    };
    expect(memberPayload.class.members.some((item) => item.name === "position" && item.kind === "property")).toBe(true);
  });

  test("member 命中方法，未命中返回 1", async () => {
    const hit = await runCaptured(["member", "Node2D", "look_at"], { docsFile: fixture });
    const hitPayload = JSON.parse(hit.stdout) as { matches: Array<{ kind: string; signature: string }>; total: number };
    expect(hit.code).toBe(ExitCode.Success);
    expect(hitPayload.total).toBe(1);
    expect(hitPayload.matches[0]?.kind).toBe("method");
    expect(hitPayload.matches[0]?.signature).toBe("look_at(point: Vector2)");
    const miss = await runCaptured(["member", "Node2D", "no_such_member"], { docsFile: fixture });
    const missPayload = JSON.parse(miss.stdout) as { total: number; suggestion?: string };
    expect(miss.code).toBe(ExitCode.ToolFailure);
    expect(missPayload.total).toBe(0);
    expect(missPayload.suggestion).toBeDefined();
  });

  test("status 只读并报告项目 pin", async () => {
    const project = path.join(tmp, "project");
    fs.mkdirSync(project, { recursive: true });
    fs.writeFileSync(
      path.join(project, "project.godot"),
      'config_version=5\n[application]\nconfig/features=PackedStringArray("4.7")\n',
      "utf8",
    );
    const result = await runCaptured(["status"], { project });
    const payload = JSON.parse(result.stdout) as {
      project: { pinned: string | null };
      cache: { exists: boolean };
      buckets: unknown[];
    };
    expect(result.code).toBe(ExitCode.Success);
    expect(payload.project.pinned).toBe("4.7");
    expect(payload.cache.exists).toBe(false);
    expect(payload.buckets).toEqual([]);
    expect(fs.existsSync(path.join(tmp, ".godot-autopilot", "docs"))).toBe(false);
  });

  test("clear 需要明确范围，--all 需要 --force", async () => {
    await expectUsage(() => runDocs(["clear"], {}));
    await expectUsage(() => runDocs(["clear"], { all: true }));
    await expectUsage(() => runDocs(["clear"], { engine: "bad" }));
    writeBucket("4.7", fixtureText, meta("4.7"));
    const cleared = await runCaptured(["clear"], { engine: "4.7" });
    const payload = JSON.parse(cleared.stdout) as { removed: string[]; remaining: string[] };
    expect(payload.removed).toEqual(["4.7"]);
    expect(payload.remaining).toEqual([]);
  });
});
