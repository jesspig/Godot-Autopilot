import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import { parseDump, type EngineVersion } from "../src/docs/api";
import {
  cacheStatus,
  docsFreshness,
  ensureDocs,
  exportDocs,
  loadExternalDocs,
  readGodotVersion,
  resolveEngine,
  type EngineDeps,
  type EngineResolution,
  type ExportRun,
} from "../src/docs/engine";
import { listBuckets, readBucket, writeBucket, type DocsMeta } from "../src/docs/store";
import { CliError, ExitCode } from "../src/errors";

const fixtureDir = path.join(import.meta.dir, "fixtures", "docs");
const fixture = path.join(fixtureDir, "extension_api.json");
const fixtureText = fs.readFileSync(fixture, "utf8");
const patchText = fs.readFileSync(path.join(fixtureDir, "extension_api-4.7.3.json"), "utf8");

let tmp: string;
let previousHome: string | undefined;

function version472(): EngineVersion {
  return { major: 4, minor: 7, patch: 2, status: "stable", fullName: "4.7.2.stable" };
}

function version473(): EngineVersion {
  return { major: 4, minor: 7, patch: 3, status: "stable", fullName: "4.7.3.stable" };
}

function version48(): EngineVersion {
  return { major: 4, minor: 8, patch: 0, status: "stable", fullName: "4.8.stable" };
}

function meta(key: string, version: EngineVersion): DocsMeta {
  return {
    schema: 1,
    key,
    engine: {
      major: version.major,
      minor: version.minor,
      patch: version.patch ?? 0,
      status: version.status ?? "stable",
      full_name: version.fullName ?? "",
    },
    source_binary: "test",
    synced_at: new Date(0).toISOString(),
    bytes: 0,
    sha256: "",
  };
}

function writeCache(text: string, version: EngineVersion): void {
  writeBucket("4.7", text, meta("4.7", version));
}

function fakeExport(text: string, counter?: { count: number }): (bin: string, cwd: string) => ExportRun {
  return (_bin, cwd) => {
    if (counter !== undefined) counter.count += 1;
    fs.writeFileSync(path.join(cwd, "extension_api.json"), text, "utf8");
    return { status: 0 };
  };
}

function expectTransport(action: () => unknown, contains: string): void {
  let caught: unknown;
  try {
    action();
  } catch (error) {
    caught = error;
  }
  expect(caught).toBeInstanceOf(CliError);
  expect((caught as CliError).code).toBe(ExitCode.Transport);
  const detail = `${(caught as CliError).message} ${(caught as CliError).suggestion ?? ""}`;
  expect(detail).toContain(contains);
}

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-docs-engine-"));
  previousHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(() => {
  if (previousHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = previousHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

describe("docs engine resolution", () => {
  test("--godot 优先于 live 与项目 pin", () => {
    const deps: EngineDeps = {
      readVersion: () => version472(),
      findLive: () => ({ godot: "C:/live/godot.exe" }),
      readProjectPin: () => ({ major: 4, minor: 7 }),
    };
    const resolved = resolveEngine({ godot: "C:/fake/godot.exe" }, deps);
    expect(resolved.source).toBe("explicit");
    expect(resolved.key).toBe("4.7");
    expect(resolved.version.patch).toBe(2);
  });

  test("live 实例优先于项目 pin", () => {
    const deps: EngineDeps = {
      readVersion: () => version472(),
      findLive: () => ({ godot: "C:/live/godot.exe" }),
      readProjectPin: () => ({ major: 4, minor: 8 }),
    };
    const resolved = resolveEngine({ project: tmp }, deps);
    expect(resolved.source).toBe("live");
    expect(resolved.key).toBe("4.7");
  });

  test("无 live 时回退到项目 pin", () => {
    const resolved = resolveEngine(
      { project: tmp },
      { findLive: () => undefined, readProjectPin: () => ({ major: 4, minor: 7 }) },
    );
    expect(resolved.source).toBe("project");
    expect(resolved.key).toBe("4.7");
    expect(resolved.binary).toBe("");
  });

  test("无 pin 时回退到最新缓存", () => {
    writeCache(fixtureText, version472());
    const resolved = resolveEngine(
      { project: tmp },
      { findLive: () => undefined, readProjectPin: () => undefined },
    );
    expect(resolved.source).toBe("newest-cache");
    expect(resolved.key).toBe("4.7");
    expect(resolved.version.patch).toBe(2);
  });

  test("无缓存时回退到 PATH", () => {
    const deps: EngineDeps = {
      findLive: () => undefined,
      readProjectPin: () => undefined,
      listBuckets: () => [],
      resolveBinary: () => ({ bin: "C:/path/godot.exe", source: "path" }),
      readVersion: () => version472(),
    };
    const resolved = resolveEngine({ project: tmp }, deps);
    expect(resolved.source).toBe("path");
    expect(resolved.key).toBe("4.7");
  });

  test("readGodotVersion 解析三段版本", () => {
    const parsed = readGodotVersion(process.execPath);
    expect(parsed.major).toBeGreaterThan(0);
    expect(parsed.minor).toBeGreaterThanOrEqual(0);
  });
});

describe("docs freshness", () => {
  test("完整版本按 x.y.z + status 比对", () => {
    const header = parseDump(fixtureText).header;
    expect(docsFreshness(header, version472()).fresh).toBe(true);
    expect(docsFreshness(header, version472()).verified).toBe(true);
    expect(docsFreshness(header, version473()).fresh).toBe(false);
    expect(docsFreshness(header, { ...version472(), status: "dev" }).fresh).toBe(false);
  });

  test("只有 x.y 时同 minor 复用且未验证", () => {
    const header = parseDump(fixtureText).header;
    expect(docsFreshness(header, { major: 4, minor: 7 })).toEqual({ fresh: true, verified: false });
    expect(docsFreshness(header, { major: 4, minor: 8 })).toEqual({ fresh: false, verified: false });
  });
});

describe("docs ensure", () => {
  test("无缓存时自动导出一次，第二次命中缓存", () => {
    const counter = { count: 0 };
    const engine: EngineResolution = {
      key: "4.7",
      source: "explicit",
      binary: "C:/fake/godot.exe",
      version: version472(),
    };
    const first = ensureDocs(engine, { runExport: fakeExport(fixtureText, counter) });
    expect(first.cache.status).toBe("auto-exported");
    expect(counter.count).toBe(1);
    expect(first.index.classes.size).toBe(5);
    const second = ensureDocs(engine, { runExport: fakeExport(fixtureText, counter) });
    expect(second.cache.status).toBe("fresh");
    expect(counter.count).toBe(1);
  });

  test("同 minor 换 patch 会重导且仍只有一个桶", () => {
    writeCache(fixtureText, version472());
    const counter = { count: 0 };
    const engine: EngineResolution = {
      key: "4.7",
      source: "explicit",
      binary: "C:/fake/godot.exe",
      version: version473(),
    };
    const loaded = ensureDocs(engine, { runExport: fakeExport(patchText, counter) });
    expect(loaded.cache.status).toBe("auto-exported");
    expect(counter.count).toBe(1);
    expect(listBuckets().map((bucket) => bucket.key)).toEqual(["4.7"]);
    expect(readBucket("4.7")?.meta.engine.patch).toBe(3);
  });

  test("项目 pin 只有 x.y 时复用同 minor 缓存且 verified=false", () => {
    writeCache(fixtureText, version472());
    const counter = { count: 0 };
    const engine: EngineResolution = {
      key: "4.7",
      source: "project",
      binary: "",
      version: { major: 4, minor: 7 },
    };
    const loaded = ensureDocs(engine, { runExport: fakeExport(fixtureText, counter) });
    expect(loaded.cache.status).toBe("fresh");
    expect(loaded.cache.verified).toBe(false);
    expect(counter.count).toBe(0);
  });

  test("项目 pin 与可用二进制跨 minor 时报错并建议 --godot", () => {
    const counter = { count: 0 };
    const engine: EngineResolution = {
      key: "4.7",
      source: "project",
      binary: "",
      version: { major: 4, minor: 7 },
    };
    expectTransport(
      () =>
        ensureDocs(engine, {
          resolveBinary: () => ({ bin: "C:/Godot48/godot.exe", source: "path" }),
          readVersion: () => version48(),
          runExport: fakeExport(fixtureText, counter),
        }),
      "--godot",
    );
    expect(counter.count).toBe(0);
  });

  test("导出失败不写缓存且不回退", () => {
    const engine: EngineResolution = {
      key: "4.7",
      source: "explicit",
      binary: "C:/fake/godot.exe",
      version: version472(),
    };
    expectTransport(
      () => ensureDocs(engine, { runExport: () => ({ status: 1, stderr: "boom" }) }),
      "boom",
    );
    expect(readBucket("4.7")).toBeUndefined();
  });

  test("缓存损坏按缺失处理并重导", () => {
    writeCache("{", version472());
    const counter = { count: 0 };
    const engine: EngineResolution = {
      key: "4.7",
      source: "explicit",
      binary: "C:/fake/godot.exe",
      version: version472(),
    };
    const loaded = ensureDocs(engine, { runExport: fakeExport(fixtureText, counter) });
    expect(loaded.cache.status).toBe("auto-exported");
    expect(counter.count).toBe(1);
    expect(() => parseDump(readBucket("4.7")?.text ?? "")).not.toThrow();
  });

  test("exportDocs 校验目标版本后再写缓存", () => {
    const counter = { count: 0 };
    expectTransport(
      () =>
        exportDocs({
          bin: "C:/fake/godot.exe",
          expected: version473(),
          deps: { runExport: fakeExport(fixtureText, counter) },
        }),
      "expected 4.7.3",
    );
    expect(counter.count).toBe(1);
    expect(readBucket("4.7")).toBeUndefined();
  });

  test("--docs-file 绕过缓存并标记 external", () => {
    const loaded = loadExternalDocs(fixture);
    expect(loaded.cache.status).toBe("external");
    expect(loaded.engine.key).toBe("4.7");
    expect(loaded.index.classes.size).toBe(5);
    expect(listBuckets()).toEqual([]);
  });

  test("cacheStatus 报告缺失、新鲜与漂移", () => {
    const engine: EngineResolution = {
      key: "4.7",
      source: "explicit",
      binary: "C:/fake/godot.exe",
      version: version472(),
    };
    expect(cacheStatus(engine).exists).toBe(false);
    expect(cacheStatus(engine).next_query_will_export).toBe(true);
    writeCache(fixtureText, version472());
    expect(cacheStatus(engine).stale).toBe(false);
    expect(cacheStatus(engine).next_query_will_export).toBe(false);
    expect(cacheStatus({ ...engine, version: version473() }).stale).toBe(true);
  });

  test("并发写入同一桶仍得到可解析的完整文件", async () => {
    const worker = path.join(fixtureDir, "write-worker.ts");
    const spawnOne = (): ReturnType<typeof Bun.spawn> =>
      Bun.spawn([process.execPath, worker, tmp], {
        env: { ...process.env, GODOT_AUTOPILOT_HOME: tmp },
        stdout: "ignore",
        stderr: "pipe",
      });
    const left = spawnOne();
    const right = spawnOne();
    const [leftCode, rightCode] = await Promise.all([left.exited, right.exited]);
    expect(leftCode).toBe(0);
    expect(rightCode).toBe(0);
    const bucket = readBucket("4.7");
    expect(bucket).toBeDefined();
    expect(() => parseDump(bucket?.text ?? "")).not.toThrow();
    const leftovers = fs
      .readdirSync(path.join(tmp, ".godot-autopilot", "docs", "4.7"))
      .filter((name) => name.endsWith(".tmp"));
    expect(leftovers).toEqual([]);
  });
});
