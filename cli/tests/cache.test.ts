import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import {
  cacheAssetPath,
  cleanCache,
  listManifests,
  manifestPath,
  selectLatest,
  sha256File,
  verifyManifest,
  writeManifest,
  type CacheManifest,
} from "../src/cache";

let tmp: string;
let previousHome: string | undefined;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-cache-"));
  previousHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(() => {
  if (previousHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = previousHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

function manifest(overrides: Partial<CacheManifest> = {}): CacheManifest {
  return {
    artifact: "plugin",
    version: "0.3.2",
    source: "github",
    asset: "addons.zip",
    sha256: "a".repeat(64),
    size: 10,
    platform: "any",
    lang: "any",
    fetched_at: "2026-11-13T00:00:00Z",
    builtin: false,
    ...overrides,
  };
}

describe("cache manifests", () => {
  test("写入与读取 manifest", () => {
    const entry = manifest();
    writeManifest(entry);
    const listed = listManifests();
    expect(listed).toHaveLength(1);
    expect(listed[0]).toEqual(entry);
    expect(fs.existsSync(manifestPath(entry))).toBe(true);
  });

  test("插件按 semver 选最高非预发布", () => {
    writeManifest(manifest({ version: "0.3.1" }));
    writeManifest(manifest({ version: "0.3.10" }));
    writeManifest(manifest({ version: "0.4.0", builtin: true }));
    expect(selectLatest("plugin")?.version).toBe("0.3.10");
  });

  test("skills 按 fetched_at 选择且区分语言", () => {
    writeManifest(manifest({ artifact: "skills", version: "0.3.1", lang: "en", fetched_at: "2026-01-01T00:00:00Z" }));
    writeManifest(manifest({ artifact: "skills", version: "0.3.2", lang: "en", fetched_at: "2026-02-01T00:00:00Z" }));
    writeManifest(manifest({ artifact: "skills", version: "0.3.2", lang: "zh", fetched_at: "2026-03-01T00:00:00Z" }));
    expect(selectLatest("skills", "en")?.version).toBe("0.3.2");
    expect(selectLatest("skills", "zh")?.fetched_at).toBe("2026-03-01T00:00:00Z");
  });

  test("校验 SHA-256 与大小", () => {
    const entry = manifest();
    const assetPath = cacheAssetPath(entry);
    fs.mkdirSync(path.dirname(assetPath), { recursive: true });
    fs.writeFileSync(assetPath, "hello");
    const sized = { ...entry, size: 5, sha256: sha256File(assetPath) };
    expect(verifyManifest(sized)).toBe(true);
    expect(verifyManifest({ ...sized, sha256: "b".repeat(64) })).toBe(false);
  });

  test("清理保留最近 3 版并删除旧 manifest", () => {
    for (const version of ["0.3.1", "0.3.2", "0.3.3", "0.3.4"]) {
      writeManifest(manifest({ version }));
    }
    const result = cleanCache({ keepVersions: 3 });
    expect(result.removed.map((entry) => entry.version)).toEqual(["0.3.1"]);
    expect(listManifests().map((entry) => entry.version).sort()).toEqual(["0.3.2", "0.3.3", "0.3.4"]);
  });

  test("内置条目不参与清理", () => {
    writeManifest(manifest({ version: "0.3.1", builtin: true }));
    writeManifest(manifest({ version: "0.3.2" }));
    const result = cleanCache({ keepVersions: 1 });
    expect(result.removed).toHaveLength(0);
    expect(listManifests()).toHaveLength(2);
  });

  test("超过总大小上限时删除最旧条目", () => {
    writeManifest(manifest({ version: "0.3.1", size: 80, fetched_at: "2026-01-01T00:00:00Z" }));
    writeManifest(manifest({ version: "0.3.2", size: 80, fetched_at: "2026-02-01T00:00:00Z" }));
    const result = cleanCache({ keepVersions: 3, maxBytes: 100 });
    expect(result.removed).toHaveLength(1);
    expect(result.bytesAfter).toBeLessThanOrEqual(100);
  });
});
