import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { writeManifest, type CacheManifest } from "../src/cache";
import { run as runInstall } from "../src/commands/install";
import { run as runSkills } from "../src/commands/skills";
import { CliError } from "../src/errors";
import {
  builtinRetiredSkills,
  builtinSkillsDigest,
  builtinSkillsFiles,
  builtinSkillsVersion,
  readInstalledManifest,
  skillsContentDigest,
  SKILLS_MANIFEST_FILE,
  verifyInstalledSkills,
  verifySkillAssetManifest,
} from "../src/skills";
import { assertSkillsAssetIntegrity, skillsUpdateItem, skillsUpdateStatus } from "../src/update";

const root = path.resolve(import.meta.dir, "..", "..");
let tmp: string;
let previousHome: string | undefined;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-skills-sync-"));
  previousHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(() => {
  if (previousHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = previousHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

function capture(): { stdout(): string; restore(): void } {
  const original = process.stdout.write;
  let out = "";
  process.stdout.write = ((chunk: unknown) => {
    out += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return {
    stdout: () => out,
    restore: () => {
      process.stdout.write = original;
    },
  };
}

async function runSkillsCaptured(positional: string[], opts: GlobalOptions): Promise<{ code: number; data: Record<string, any> }> {
  const cap = capture();
  try {
    const code = await runSkills(positional, opts);
    return { code, data: (JSON.parse(cap.stdout()) as { data: Record<string, any> }).data };
  } finally {
    cap.restore();
  }
}

async function runInstallCaptured(positional: string[], opts: GlobalOptions): Promise<{ code: number; data: Record<string, any> }> {
  const cap = capture();
  try {
    const code = await runInstall(positional, opts);
    return { code, data: (JSON.parse(cap.stdout()) as { data: Record<string, any> }).data };
  } finally {
    cap.restore();
  }
}

function createLegacyInstall(target: string): string {
  const book = path.join(target, "godot-autopilot");
  fs.mkdirSync(path.join(book, "references"), { recursive: true });
  fs.writeFileSync(
    path.join(book, "SKILL.md"),
    '---\nname: godot-autopilot\ndescription: "old"\n---\n# Old\nUse the legacy transport and call_tool.\n',
    "utf8",
  );
  fs.writeFileSync(path.join(book, "references", "legacy.md"), "# Legacy helpers\ncall_tool\n", "utf8");
  fs.mkdirSync(path.join(target, "my-notes"), { recursive: true });
  fs.writeFileSync(path.join(target, "my-notes", "note.md"), "mine\n", "utf8");
  return path.join(book, "references", "legacy.md");
}

function snapshot(dir: string): Record<string, string> {
  const files: Record<string, string> = {};
  const walk = (current: string, prefix: string): void => {
    for (const entry of fs.readdirSync(current, { withFileTypes: true })) {
      const relative = prefix === "" ? entry.name : `${prefix}/${entry.name}`;
      const full = path.join(current, entry.name);
      if (entry.isDirectory()) walk(full, relative);
      else if (entry.isFile()) files[relative] = fs.readFileSync(full, "utf8");
    }
  };
  walk(dir, "");
  return files;
}

function findTempFiles(dir: string): string[] {
  const found: string[] = [];
  const walk = (current: string): void => {
    for (const entry of fs.readdirSync(current, { withFileTypes: true })) {
      const full = path.join(current, entry.name);
      if (entry.isDirectory()) walk(full);
      else if (entry.isFile() && entry.name.includes(".tmp")) found.push(full);
    }
  };
  walk(dir);
  return found;
}

describe("skill asset manifest", () => {
  test("schema、计数与 digest 由生成脚本写入且可复算", () => {
    const asset = JSON.parse(fs.readFileSync(path.join(root, "cli", "assets", "skills.json"), "utf8"));
    expect(verifySkillAssetManifest(asset)).toBe(true);
    expect(asset.version).toBe(builtinSkillsVersion());
    expect(asset.content_digest).toBe(builtinSkillsDigest());
    expect(asset.file_count).toBe(Object.keys(asset.files).length);
    expect(asset.file_count).toBe(Object.keys(asset.files_zh).length);
    expect(asset.content_digest).toBe(
      skillsContentDigest(asset.version, asset.files, asset.files_zh),
    );
    expect(verifySkillAssetManifest({ ...asset, content_digest: "0".repeat(64) })).toBe(false);
    expect(verifySkillAssetManifest({ ...asset, file_count: asset.file_count + 1 })).toBe(false);
    expect(verifySkillAssetManifest({ ...asset, schema_version: 0 })).toBe(false);
  });
});

describe("gda skills sync", () => {
  test("--check 预演只读：列出将删除/覆盖/保留项且不写盘", async () => {
    const target = path.join(tmp, "check-only");
    const stale = createLegacyInstall(target);
    const before = snapshot(target);
    const result = await runSkillsCaptured(["sync"], { pathText: target, check: true });
    expect(result.code).toBe(0);
    expect(result.data.dry_run).toBe(true);
    expect(result.data.would_delete).toContain(path.relative(target, stale).split(path.sep).join("/"));
    expect(result.data.would_overwrite).toContain("godot-autopilot/SKILL.md");
    expect(result.data.preserved).toContain("my-notes");
    expect(snapshot(target)).toEqual(before);
    expect(fs.existsSync(path.join(target, SKILLS_MANIFEST_FILE))).toBe(false);
  });

  test("--dry-run 子动作同样不写盘", async () => {
    const target = path.join(tmp, "dry-run");
    const stale = createLegacyInstall(target);
    const before = snapshot(target);
    const result = await runSkillsCaptured(["sync", "--dry-run"], { pathText: target });
    expect(result.code).toBe(0);
    expect(result.data.dry_run).toBe(true);
    expect(result.data.would_delete).toContain("godot-autopilot/references/legacy.md");
    expect(snapshot(target)).toEqual(before);
  });

  test("--force 删除旧 reference、覆盖受管文件并保留自建目录", async () => {
    const target = path.join(tmp, "forced");
    createLegacyInstall(target);
    const result = await runSkillsCaptured(["sync", "--force"], { pathText: target });
    expect(result.code).toBe(0);
    expect(result.data.written).toContain("godot-autopilot/references/recipes/skills-sync.md");
    expect(result.data.deleted).toContain("godot-autopilot/references/legacy.md");
    expect(fs.existsSync(path.join(target, "godot-autopilot", "references", "legacy.md"))).toBe(false);
    expect(fs.readFileSync(path.join(target, "godot-autopilot", "SKILL.md"), "utf8")).toBe(
      builtinSkillsFiles("en")["godot-autopilot/SKILL.md"],
    );
    expect(fs.existsSync(path.join(target, "my-notes", "note.md"))).toBe(true);
    const manifest = readInstalledManifest(target);
    expect(manifest?.version).toBe(builtinSkillsVersion());
    expect(manifest?.content_digest).toBe(builtinSkillsDigest());
    expect(verifyInstalledSkills(target, "en").ok).toBe(true);
  });

  test("EN/ZH 切换覆盖同一受管书", async () => {
    const target = path.join(tmp, "switch");
    await runSkills(["sync", "--force"], { pathText: target, zh: true });
    const skillFile = path.join(target, "godot-autopilot", "SKILL.md");
    expect(fs.readFileSync(skillFile, "utf8")).toBe(builtinSkillsFiles("zh")["godot-autopilot/SKILL.md"]);
    await runSkills(["sync", "--force"], { pathText: target });
    expect(fs.readFileSync(skillFile, "utf8")).toBe(builtinSkillsFiles("en")["godot-autopilot/SKILL.md"]);
    expect(verifyInstalledSkills(target, "en").ok).toBe(true);
  });

  test("内容 digest 损坏时拒绝并要求 --force 修复", async () => {
    const target = path.join(tmp, "damaged");
    await runSkills(["sync", "--force"], { pathText: target });
    const skillFile = path.join(target, "godot-autopilot", "SKILL.md");
    fs.writeFileSync(skillFile, "tampered\n", "utf8");
    expect(verifyInstalledSkills(target, "en").ok).toBe(false);
    let caught: unknown;
    try {
      await runSkills(["sync"], { pathText: target });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).suggestion).toContain("--force");
    expect(fs.readFileSync(skillFile, "utf8")).toBe("tampered\n");
    await runSkills(["sync", "--force"], { pathText: target });
    expect(verifyInstalledSkills(target, "en").ok).toBe(true);
  });

  test("旧 manifest 越界声明时拒绝且保留旧文件", async () => {
    const target = path.join(tmp, "escape");
    const stale = createLegacyInstall(target);
    fs.writeFileSync(
      path.join(target, SKILLS_MANIFEST_FILE),
      JSON.stringify({
        schema_version: 1,
        version: builtinSkillsVersion(),
        content_digest: builtinSkillsDigest(),
        tree_digest: "a".repeat(64),
        lang: "en",
        files: ["other-book/SKILL.md", "../outside.md"],
        installed_at: "2026-01-01T00:00:00Z",
      }),
      "utf8",
    );
    let caught: unknown;
    try {
      await runSkills(["sync"], { pathText: target });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).message).toContain("installed manifest declares paths outside a managed book");
    expect(fs.existsSync(stale)).toBe(true);
    expect(fs.existsSync(path.join(target, "my-notes", "note.md"))).toBe(true);
  });

  test("写盘中途失败回滚到旧安装并清理临时文件", async () => {
    const target = path.join(tmp, "rollback");
    createLegacyInstall(target);
    const skillFile = path.join(target, "godot-autopilot", "SKILL.md");
    const before = fs.readFileSync(skillFile, "utf8");
    const original = fs.renameSync;
    let calls = 0;
    (fs as unknown as { renameSync: typeof fs.renameSync }).renameSync = ((...args: Parameters<typeof fs.renameSync>) => {
      calls += 1;
      if (calls === 2) throw new Error("injected rename failure");
      return original(...args);
    }) as typeof fs.renameSync;
    let caught: unknown;
    try {
      await runSkills(["sync"], { pathText: target, force: true });
    } catch (error) {
      caught = error;
    } finally {
      fs.renameSync = original;
    }
    expect(calls).toBeGreaterThanOrEqual(2);
    expect(caught).toBeInstanceOf(CliError);
    expect(fs.readFileSync(skillFile, "utf8")).toBe(before);
    expect(fs.existsSync(path.join(target, "godot-autopilot", "references", "legacy.md"))).toBe(true);
    expect(findTempFiles(target)).toEqual([]);
  });
});

describe("install skills upgrade", () => {
  test("旧版升级清零过期 reference、写入 recipe 并保留自建目录", async () => {
    const target = path.join(tmp, "upgrade");
    const stale = createLegacyInstall(target);
    const retiredName = builtinRetiredSkills()[0] ?? "godot-autopilot-tools";
    fs.mkdirSync(path.join(target, retiredName), { recursive: true });
    const result = await runInstallCaptured(["skills"], {
      sourceText: "builtin",
      pathText: target,
      force: true,
      prune: true,
    });
    expect(result.code).toBe(0);
    expect(result.data.version).toBe(builtinSkillsVersion());
    expect(result.data.asset_version).toBe(builtinSkillsVersion());
    expect(result.data.content_digest).toBe(builtinSkillsDigest());
    expect(result.data.removed).toContain(retiredName);
    expect(fs.existsSync(stale)).toBe(false);
    expect(fs.existsSync(path.join(target, "godot-autopilot", "references", "recipes", "skills-sync.md"))).toBe(true);
    expect(fs.existsSync(path.join(target, "my-notes", "note.md"))).toBe(true);
    const contents = Object.values(snapshot(path.join(target, "godot-autopilot"))).join("\n");
    expect(contents).not.toContain("legacy transport");
    expect(contents).not.toContain("call_tool");
    expect(contents).not.toContain("legacy helpers");
  });

  test("offline 安装使用内置技能且报告真实 asset 版本", async () => {
    const target = path.join(tmp, "offline");
    const result = await runInstallCaptured(["skills"], {
      offline: true,
      pathText: target,
      force: true,
    });
    expect(result.code).toBe(0);
    expect(result.data.origin).toBe("builtin");
    expect(result.data.asset_version).toBe(builtinSkillsVersion());
    expect(result.data.content_digest).toBe(builtinSkillsDigest());
    expect(verifyInstalledSkills(target, "en").ok).toBe(true);
  });
});

describe("update skills status", () => {
  test("回报真实 asset version、digest 与已安装版本", async () => {
    const target = path.join(tmp, "status");
    await runSkills(["sync", "--force"], { pathText: target });
    const status = skillsUpdateStatus(target);
    expect(status.asset_ok).toBe(true);
    expect(status.asset_version).toBe(builtinSkillsVersion());
    expect(status.content_digest).toBe(builtinSkillsDigest());
    expect(status.file_count).toBe(Object.keys(builtinSkillsFiles("en")).length);
    expect(status.installed_version).toBe(builtinSkillsVersion());
    expect(status.installed_ok).toBe(true);
    const item = skillsUpdateItem(true, "", target);
    expect(item.artifact).toBe("skills");
    expect(item.from).toBe(builtinSkillsVersion());
    expect(item.to).toBe(builtinSkillsVersion());
    expect(() => assertSkillsAssetIntegrity()).not.toThrow();
  });

  test("缓存版本与 asset 版本匹配状态可核查", () => {
    const target = path.join(tmp, "cache-status");
    const base: CacheManifest = {
      artifact: "skills",
      version: builtinSkillsVersion(),
      source: "github",
      asset: "skills-en.zip",
      sha256: "a".repeat(64),
      size: 1,
      platform: "",
      lang: "en",
      fetched_at: "2026-01-01T00:00:00Z",
      builtin: false,
    };
    writeManifest(base);
    const matched = skillsUpdateStatus(target);
    expect(matched.cache_version).toBe(builtinSkillsVersion());
    expect(matched.cache_matches_asset).toBe(true);
    writeManifest({ ...base, version: "9.9.9", fetched_at: "2026-02-01T00:00:00Z" });
    const mismatched = skillsUpdateStatus(target);
    expect(mismatched.cache_version).toBe("9.9.9");
    expect(mismatched.cache_matches_asset).toBe(false);
    expect(mismatched.asset_version).toBe(builtinSkillsVersion());
  });
});