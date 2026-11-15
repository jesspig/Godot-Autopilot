import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import {
  bucketDir,
  bucketKey,
  clearBuckets,
  docsRoot,
  listBuckets,
  readBucket,
  writeBucket,
  type DocsMeta,
} from "../src/docs/store";
import { CliError, ExitCode } from "../src/errors";

const fixture = path.join(import.meta.dir, "fixtures", "docs", "extension_api.json");
const text = fs.readFileSync(fixture, "utf8");

let tmp: string;
let previousHome: string | undefined;

function meta(key: string, patch = 2, status = "stable"): DocsMeta {
  const minor = key.startsWith("4.8") ? 8 : 7;
  return {
    schema: 1,
    key,
    engine: {
      major: 4,
      minor,
      patch,
      status,
      full_name: `Godot Engine v4.${minor}.${patch}.${status}.official`,
    },
    source_binary: "test",
    synced_at: new Date(0).toISOString(),
    bytes: 0,
    sha256: "",
  };
}

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-docs-cache-"));
  previousHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(() => {
  if (previousHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = previousHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

describe("docs cache", () => {
  test("bucketKey 按 stable 与预发布分桶", () => {
    expect(bucketKey({ major: 4, minor: 7, patch: 2, status: "stable" })).toBe("4.7");
    expect(bucketKey({ major: 4, minor: 8, status: "dev" })).toBe("4.8-dev");
    expect(bucketKey({ major: 4, minor: 8, status: "rc1" })).toBe("4.8-rc1");
    expect(bucketKey({ major: 4, minor: 7 })).toBe("4.7");
  });

  test("写入后可读回文本、meta 与校验值", () => {
    const written = writeBucket("4.7", text, meta("4.7"));
    expect(written.bytes).toBe(Buffer.byteLength(text, "utf8"));
    expect(written.sha256.length).toBe(64);
    const bucket = readBucket("4.7");
    expect(bucket).toBeDefined();
    expect(bucket?.text).toBe(text);
    expect(bucket?.meta.engine.full_name).toContain("4.7.2");
    expect(fs.existsSync(path.join(docsRoot(), "4.7", "extension_api.json"))).toBe(true);
    expect(fs.existsSync(path.join(docsRoot(), "4.7", "meta.json"))).toBe(true);
  });

  test("覆盖写入保留可解析的完整文件且不残留临时文件", () => {
    writeBucket("4.7", text, meta("4.7"));
    const parsed = JSON.parse(text) as { header: { version_patch: number } };
    parsed.header.version_patch = 3;
    const next = `${JSON.stringify(parsed)}\n`;
    writeBucket("4.7", next, meta("4.7", 3));
    expect(readBucket("4.7")?.text).toBe(next);
    const leftovers = fs
      .readdirSync(path.join(docsRoot(), "4.7"))
      .filter((name) => name.endsWith(".tmp"));
    expect(leftovers).toEqual([]);
  });

  test("meta 缺失或损坏时视为无缓存", () => {
    const dir = bucketDir("4.7");
    fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(path.join(dir, "extension_api.json"), text, "utf8");
    expect(readBucket("4.7")).toBeUndefined();
    fs.writeFileSync(path.join(dir, "meta.json"), "{}", "utf8");
    expect(readBucket("4.7")).toBeUndefined();
  });

  test("listBuckets 按版本排序且只列出完整桶", () => {
    writeBucket("4.8-dev", text, meta("4.8-dev", 0, "dev"));
    writeBucket("4.7", text, meta("4.7"));
    expect(listBuckets().map((bucket) => bucket.key)).toEqual(["4.7", "4.8-dev"]);
    fs.rmSync(path.join(docsRoot(), "4.8-dev", "extension_api.json"));
    expect(listBuckets().map((bucket) => bucket.key)).toEqual(["4.7"]);
  });

  test("clearBuckets 只删除指定桶", () => {
    writeBucket("4.7", text, meta("4.7"));
    writeBucket("4.8-dev", text, meta("4.8-dev", 0, "dev"));
    expect(clearBuckets(["4.7"])).toEqual(["4.7"]);
    expect(listBuckets().map((bucket) => bucket.key)).toEqual(["4.8-dev"]);
    expect(clearBuckets(["4.7"])).toEqual([]);
    expect(clearBuckets(["4.8-dev"])).toEqual(["4.8-dev"]);
    expect(listBuckets()).toEqual([]);
  });

  test("非法桶键被拒绝", () => {
    let caught: unknown;
    try {
      bucketDir("../../etc");
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Usage);
  });
});
