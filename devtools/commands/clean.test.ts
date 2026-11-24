import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { afterEach, describe, expect, spyOn, test } from "bun:test";

import { cleanTargets } from "./clean";
import { OWNED_FILES_MANIFEST, PLATFORM_LIBS, addonDir } from "../lib/config";
import { GDEXTENSION_NAME } from "../lib/gdextension";

const tempDirs: string[] = [];

function makeProject(): { project: string; addon: string } {
  const project = path.join(makeTempDir("gda-clean-project-"), "project");
  fs.mkdirSync(project, { recursive: true });
  fs.writeFileSync(path.join(project, "project.godot"), "[application]\n", "utf8");
  return { project, addon: addonDir(project) };
}

function makeTempDir(prefix: string): string {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), prefix));
  tempDirs.push(dir);
  return dir;
}

afterEach(() => {
  for (const dir of tempDirs.splice(0)) {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

function writeText(file: string, content: string): void {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, content, "utf8");
}

function writeManifest(addon: string, files: string[]): void {
  writeText(
    path.join(addon, OWNED_FILES_MANIFEST),
    JSON.stringify({ schema: 1, version: "0.0.0", files }),
  );
}

function captureLogs(): { logs: string[]; restore: () => void } {
  const logs: string[] = [];
  const spy = spyOn(console, "log").mockImplementation((...args: unknown[]) => {
    logs.push(args.map(String).join(" "));
  });
  return { logs, restore: () => spy.mockRestore() };
}

describe("cleanTargets ownership", () => {
  test("removes only manifest-owned files and preserves user files and directories", () => {
    const { project, addon } = makeProject();
    const library = PLATFORM_LIBS["linux"]!;
    writeText(path.join(addon, GDEXTENSION_NAME), "extension");
    writeText(path.join(addon, library), "library");
    writeText(path.join(addon, "BridgeCall.cs"), "bridge");
    writeText(path.join(addon, "User.cs"), "user-source");
    writeText(path.join(addon, "~locked.tmp"), "user-temp");
    writeText(path.join(addon, "user-data", "keep.txt"), "user-data");
    writeText(path.join(project, ".godot", "user-cache.txt"), "user-cache");
    writeManifest(addon, [GDEXTENSION_NAME, library, "BridgeCall.cs"]);

    const logs = captureLogs();
    try {
      const code = cleanTargets([project]);

      expect(code).toBe(0);
      expect(fs.existsSync(path.join(addon, GDEXTENSION_NAME))).toBe(false);
      expect(fs.existsSync(path.join(addon, library))).toBe(false);
      expect(fs.existsSync(path.join(addon, "BridgeCall.cs"))).toBe(false);
      expect(fs.existsSync(path.join(addon, OWNED_FILES_MANIFEST))).toBe(false);
      expect(fs.readFileSync(path.join(addon, "User.cs"), "utf8")).toBe("user-source");
      expect(fs.readFileSync(path.join(addon, "~locked.tmp"), "utf8")).toBe("user-temp");
      expect(fs.readFileSync(path.join(addon, "user-data", "keep.txt"), "utf8")).toBe("user-data");
      expect(fs.existsSync(path.join(project, ".godot"))).toBe(false);
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("User.cs"))).toBe(true);
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("~locked.tmp"))).toBe(true);
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("user-data"))).toBe(true);
      expect(logs.logs.some((line) => line.includes("[CLEAN] Removing") && line.includes(".godot"))).toBe(true);
    } finally {
      logs.restore();
    }
  });

  test("reports a locked Godot cache and still cleans owned addon files", () => {
    const { project, addon } = makeProject();
    const cache = path.join(project, ".godot");
    writeText(path.join(cache, "locked.txt"), "locked-cache");
    writeText(path.join(addon, GDEXTENSION_NAME), "extension");
    writeManifest(addon, [GDEXTENSION_NAME]);
    const remove = fs.rmSync;
    const removeSpy = spyOn(fs, "rmSync").mockImplementation((target, options) => {
      if (target === cache) {
        throw new Error("EBUSY: Godot cache is locked");
      }
      remove(target, options);
    });

    const logs = captureLogs();
    try {
      const code = cleanTargets([project]);

      expect(code).toBe(1);
      expect(fs.readFileSync(path.join(cache, "locked.txt"), "utf8")).toBe("locked-cache");
      expect(fs.existsSync(path.join(addon, GDEXTENSION_NAME))).toBe(false);
      expect(fs.existsSync(path.join(addon, OWNED_FILES_MANIFEST))).toBe(false);
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("EBUSY"))).toBe(true);
    } finally {
      logs.restore();
      removeSpy.mockRestore();
    }
  });

  test("a legacy addon without a manifest is preserved", () => {
    const { project, addon } = makeProject();
    writeText(path.join(addon, GDEXTENSION_NAME), "legacy-extension");
    writeText(path.join(addon, "User.cs"), "user-source");

    const logs = captureLogs();
    try {
      const code = cleanTargets([project]);

      expect(code).toBe(0);
      expect(fs.readFileSync(path.join(addon, GDEXTENSION_NAME), "utf8")).toBe("legacy-extension");
      expect(fs.readFileSync(path.join(addon, "User.cs"), "utf8")).toBe("user-source");
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("manifest"))).toBe(true);
    } finally {
      logs.restore();
    }
  });

  test("a locked owned file is reported and keeps the manifest for retry", () => {
    const { project, addon } = makeProject();
    const library = PLATFORM_LIBS["linux"]!;
    writeText(path.join(addon, library), "locked-library");
    writeText(path.join(addon, "User.cs"), "user-source");
    writeManifest(addon, [library]);
    const unlink = fs.unlinkSync;
    const unlinkSpy = spyOn(fs, "unlinkSync").mockImplementation((target) => {
      if (target === path.join(addon, library)) {
        throw new Error("EBUSY: library is locked");
      }
      unlink(target);
    });

    const logs = captureLogs();
    try {
      const code = cleanTargets([project]);

      expect(code).toBe(1);
      expect(fs.readFileSync(path.join(addon, library), "utf8")).toBe("locked-library");
      expect(fs.existsSync(path.join(addon, OWNED_FILES_MANIFEST))).toBe(true);
      expect(fs.readFileSync(path.join(addon, "User.cs"), "utf8")).toBe("user-source");
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("EBUSY"))).toBe(true);
    } finally {
      logs.restore();
      unlinkSpy.mockRestore();
    }
  });

  test("a manifest cannot follow a linked parent directory outside the addon", () => {
    const { project, addon } = makeProject();
    const outside = path.join(project, "outside-dir");
    writeText(path.join(outside, "secret.txt"), "outside-data");
    fs.mkdirSync(addon, { recursive: true });
    fs.symlinkSync(outside, path.join(addon, "linked"), process.platform === "win32" ? "junction" : "dir");
    writeManifest(addon, ["linked/secret.txt"]);

    const logs = captureLogs();
    try {
      const code = cleanTargets([project]);

      expect(code).toBe(1);
      expect(fs.readFileSync(path.join(outside, "secret.txt"), "utf8")).toBe("outside-data");
      expect(fs.existsSync(path.join(addon, OWNED_FILES_MANIFEST))).toBe(true);
      expect(logs.logs.some((line) => line.includes("unsafe parent path"))).toBe(true);
    } finally {
      logs.restore();
    }
  });

  test("an invalid manifest cannot delete files outside the addon", () => {
    const { project, addon } = makeProject();
    writeText(path.join(project, "outside.txt"), "outside-data");
    writeManifest(addon, ["../outside.txt", "../../escape.txt"]);

    const logs = captureLogs();
    try {
      const code = cleanTargets([project]);

      expect(code).toBe(1);
      expect(fs.readFileSync(path.join(project, "outside.txt"), "utf8")).toBe("outside-data");
      expect(fs.existsSync(path.join(addon, OWNED_FILES_MANIFEST))).toBe(true);
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("manifest"))).toBe(true);
    } finally {
      logs.restore();
    }
  });
});
