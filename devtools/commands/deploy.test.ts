import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { afterEach, describe, expect, spyOn, test } from "bun:test";

import { deployArtifacts, missingDeployArtifacts } from "./deploy";
import {
  ADDON_VERSION,
  OWNED_FILES_MANIFEST,
  PLATFORM_LIBS,
  addonDir,
} from "../lib/config";
import { GDEXTENSION_NAME } from "../lib/gdextension";
import { readOwnedFilesManifest } from "../lib/owned-files";

const tempDirs: string[] = [];

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

function writeManifest(addon: string, files: string[], version = "0.0.0"): void {
  writeText(path.join(addon, OWNED_FILES_MANIFEST), JSON.stringify({ schema: 1, version, files }));
}

function createProject(): { project: string; addon: string } {
  const project = path.join(makeTempDir("gda-deploy-project-"), "project");
  writeText(path.join(project, "project.godot"), "[application]\n");
  return { project, addon: addonDir(project) };
}

function createArtifacts(): {
  binaryDir: string;
  bridgeDir: string;
  library: string;
  bridgeSource: string;
} {
  const root = makeTempDir("gda-deploy-artifacts-");
  const binaryDir = path.join(root, "bin");
  const bridgeDir = path.join(root, "bridge");
  const library = PLATFORM_LIBS["linux"]!;
  writeText(path.join(binaryDir, library), "new-library");
  writeText(path.join(bridgeDir, "BridgeCall.cs"), "new-bridge");
  return {
    binaryDir,
    bridgeDir,
    library,
    bridgeSource: path.join(bridgeDir, "BridgeCall.cs"),
  };
}

function snapshotDirectory(dir: string): Record<string, string> {
  if (!fs.existsSync(dir)) return {};
  const snapshot: Record<string, string> = {};
  function visit(current: string, relative: string): void {
    for (const entry of fs.readdirSync(current, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
      const absolute = path.join(current, entry.name);
      const key = relative === "" ? entry.name : path.join(relative, entry.name);
      if (entry.isDirectory()) {
        snapshot[key] = "<directory>";
        visit(absolute, key);
      } else if (entry.isSymbolicLink()) {
        snapshot[key] = `<symlink:${fs.readlinkSync(absolute)}>`;
      } else {
        snapshot[key] = fs.readFileSync(absolute, "utf8");
      }
    }
  }
  visit(dir, "");
  return snapshot;
}

function captureLogs(): { logs: string[]; restore: () => void } {
  const logs: string[] = [];
  const spy = spyOn(console, "log").mockImplementation((...args: unknown[]) => {
    logs.push(args.map(String).join(" "));
  });
  return { logs, restore: () => spy.mockRestore() };
}

describe("missingDeployArtifacts", () => {
  test("empty directory reports the platform library and bridge sources missing", () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-deploy-preflight-"));
    try {
      const missing = missingDeployArtifacts(dir, "linux");
      expect(missing).toContain(PLATFORM_LIBS["linux"]!);
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  test("unsupported platform is reported without touching the filesystem", () => {
    const missing = missingDeployArtifacts("C:\\nonexistent", "plan9");
    expect(missing[0]).toContain("unsupported platform");
  });

  test("all present artifacts yields no missing entries", () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-deploy-preflight-"));
    try {
      fs.writeFileSync(path.join(dir, PLATFORM_LIBS["linux"]!), "");
      const bridgeDir = path.join(process.cwd(), "csharp_bridge");
      const missing = missingDeployArtifacts(dir, "linux");
      const missingBridge = missing.filter((name) => fs.existsSync(path.join(bridgeDir, name)) === false);
      expect(missingBridge).toEqual([]);
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe("deployArtifacts ownership", () => {
  test("preserves mixed user files and removes only stale manifest-owned files", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    writeText(path.join(addon, GDEXTENSION_NAME), "old-extension");
    writeText(path.join(addon, artifacts.library), "old-library");
    writeText(path.join(addon, "StaleBridge.cs"), "old-bridge");
    writeText(path.join(addon, "User.cs"), "user-source");
    writeText(path.join(addon, "~editor.tmp"), "user-temp");
    writeText(path.join(addon, "notes.txt"), "user-notes");
    writeText(path.join(addon, "user-data", "keep.txt"), "user-data");
    writeManifest(addon, [GDEXTENSION_NAME, artifacts.library, "StaleBridge.cs"]);

    const logs = captureLogs();
    try {
      const code = deployArtifacts("debug", [project], {
        binaryDir: artifacts.binaryDir,
        bridgeDir: artifacts.bridgeDir,
        platform: "linux",
        version: "9.9.9",
      });

      expect(code).toBe(0);
      expect(fs.readFileSync(path.join(addon, "User.cs"), "utf8")).toBe("user-source");
      expect(fs.readFileSync(path.join(addon, "~editor.tmp"), "utf8")).toBe("user-temp");
      expect(fs.readFileSync(path.join(addon, "notes.txt"), "utf8")).toBe("user-notes");
      expect(fs.readFileSync(path.join(addon, "user-data", "keep.txt"), "utf8")).toBe("user-data");
      expect(fs.existsSync(path.join(addon, "StaleBridge.cs"))).toBe(false);
      expect(fs.readFileSync(path.join(addon, artifacts.library), "utf8")).toBe("new-library");
      expect(fs.readFileSync(path.join(addon, "BridgeCall.cs"), "utf8")).toBe("new-bridge");
      expect(readOwnedFilesManifest(addon)).toEqual({
        schema: 1,
        version: "9.9.9",
        files: ["BridgeCall.cs", GDEXTENSION_NAME, artifacts.library].sort(),
      });
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("User.cs"))).toBe(true);
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("~editor.tmp"))).toBe(true);
    } finally {
      logs.restore();
    }
  });

  test("replaces a linked owned leaf in staging without writing through it", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    const outside = path.join(project, "outside-library");
    writeText(path.join(outside, "secret.txt"), "outside-data");
    fs.mkdirSync(addon, { recursive: true });
    fs.symlinkSync(
      outside,
      path.join(addon, artifacts.library),
      process.platform === "win32" ? "junction" : "dir",
    );
    const beforeOutside = snapshotDirectory(outside);

    const logs = captureLogs();
    try {
      const code = deployArtifacts("debug", [project], {
        binaryDir: artifacts.binaryDir,
        bridgeDir: artifacts.bridgeDir,
        platform: "linux",
      });

      expect(code).toBe(0);
      expect(snapshotDirectory(outside)).toEqual(beforeOutside);
      expect(fs.lstatSync(path.join(addon, artifacts.library)).isFile()).toBe(true);
      expect(fs.readFileSync(path.join(addon, artifacts.library), "utf8")).toBe("new-library");
    } finally {
      logs.restore();
    }
  });

  test("legacy directories without a manifest keep every unknown file", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    writeText(path.join(addon, "StaleBridge.cs"), "legacy-bridge");
    writeText(path.join(addon, "User.cs"), "user-source");
    writeText(path.join(addon, "~legacy.tmp"), "legacy-temp");

    const logs = captureLogs();
    try {
      const code = deployArtifacts("debug", [project], {
        binaryDir: artifacts.binaryDir,
        bridgeDir: artifacts.bridgeDir,
        platform: "linux",
        version: ADDON_VERSION,
      });

      expect(code).toBe(0);
      expect(fs.readFileSync(path.join(addon, "StaleBridge.cs"), "utf8")).toBe("legacy-bridge");
      expect(fs.readFileSync(path.join(addon, "User.cs"), "utf8")).toBe("user-source");
      expect(fs.readFileSync(path.join(addon, "~legacy.tmp"), "utf8")).toBe("legacy-temp");
      expect(readOwnedFilesManifest(addon)?.files).toContain("BridgeCall.cs");
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("StaleBridge.cs"))).toBe(true);
    } finally {
      logs.restore();
    }
  });

  test("an invalid legacy manifest never authorizes deletions", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    writeText(path.join(addon, "StaleBridge.cs"), "legacy-bridge");
    writeText(path.join(addon, "User.cs"), "user-source");
    writeText(path.join(project, "outside.txt"), "outside-data");
    writeText(
      path.join(addon, OWNED_FILES_MANIFEST),
      JSON.stringify({ schema: 1, version: "0.0.0", files: ["../outside.txt"] }),
    );

    const logs = captureLogs();
    try {
      const code = deployArtifacts("debug", [project], {
        binaryDir: artifacts.binaryDir,
        bridgeDir: artifacts.bridgeDir,
        platform: "linux",
      });

      expect(code).toBe(0);
      expect(fs.readFileSync(path.join(addon, "StaleBridge.cs"), "utf8")).toBe("legacy-bridge");
      expect(fs.readFileSync(path.join(addon, "User.cs"), "utf8")).toBe("user-source");
      expect(fs.readFileSync(path.join(project, "outside.txt"), "utf8")).toBe("outside-data");
      expect(readOwnedFilesManifest(addon)?.files).not.toContain("../outside.txt");
      expect(logs.logs.some((line) => line.includes("[WARN]") && line.includes("manifest"))).toBe(true);
    } finally {
      logs.restore();
    }
  });

  test("deploys every requested project", () => {
    const first = createProject();
    const second = createProject();
    const artifacts = createArtifacts();

    const code = deployArtifacts("debug", [first.project, second.project], {
      binaryDir: artifacts.binaryDir,
      bridgeDir: artifacts.bridgeDir,
      platform: "linux",
    });

    expect(code).toBe(0);
    expect(fs.readFileSync(path.join(first.addon, artifacts.library), "utf8")).toBe("new-library");
    expect(fs.readFileSync(path.join(second.addon, artifacts.library), "utf8")).toBe("new-library");
  });

  test("staged validation failure preserves the original and removes staging", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    writeText(path.join(addon, GDEXTENSION_NAME), "old-extension");
    writeText(path.join(addon, artifacts.library), "old-library");
    writeText(path.join(addon, "User.cs"), "user-source");
    const before = snapshotDirectory(addon);
    fs.unlinkSync(path.join(artifacts.binaryDir, artifacts.library));

    const code = deployArtifacts("debug", [project], {
      binaryDir: artifacts.binaryDir,
      bridgeDir: artifacts.bridgeDir,
      platform: "linux",
    });

    expect(code).toBe(1);
    expect(snapshotDirectory(addon)).toEqual(before);
    expect(fs.readdirSync(path.dirname(addon))).toEqual(["godot-autopilot"]);
  });

  test("copy failure leaves the original addon unchanged", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    writeText(path.join(addon, GDEXTENSION_NAME), "old-extension");
    writeText(path.join(addon, artifacts.library), "old-library");
    writeText(path.join(addon, "User.cs"), "user-source");
    writeManifest(addon, [GDEXTENSION_NAME, artifacts.library]);
    const before = snapshotDirectory(addon);

    const logs = captureLogs();
    try {
      const code = deployArtifacts("debug", [project], {
        binaryDir: artifacts.binaryDir,
        bridgeDir: artifacts.bridgeDir,
        platform: "linux",
        hooks: {
          copyFile(source, destination) {
            if (source === artifacts.bridgeSource) {
              throw new Error("simulated copy failure");
            }
            fs.cpSync(source, destination, { preserveTimestamps: true });
          },
        },
      });

      expect(code).toBe(1);
      expect(snapshotDirectory(addon)).toEqual(before);
      expect(fs.readdirSync(path.dirname(addon))).toEqual(["godot-autopilot"]);
      expect(logs.logs.some((line) => line.includes("[ERROR]") && line.includes("simulated copy failure"))).toBe(true);
    } finally {
      logs.restore();
    }
  });

  test("a locked addon directory aborts before replacement", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    writeText(path.join(addon, GDEXTENSION_NAME), "old-extension");
    writeText(path.join(addon, "User.cs"), "user-source");
    const before = snapshotDirectory(addon);

    const logs = captureLogs();
    try {
      const code = deployArtifacts("debug", [project], {
        binaryDir: artifacts.binaryDir,
        bridgeDir: artifacts.bridgeDir,
        platform: "linux",
        hooks: {
          rename(source, destination) {
            if (source === addon) {
              throw new Error("EBUSY: addon is locked");
            }
            fs.renameSync(source, destination);
          },
        },
      });

      expect(code).toBe(1);
      expect(snapshotDirectory(addon)).toEqual(before);
      expect(fs.readdirSync(path.dirname(addon))).toEqual(["godot-autopilot"]);
      expect(logs.logs.some((line) => line.includes("EBUSY"))).toBe(true);
    } finally {
      logs.restore();
    }
  });

  test("failure to install staging restores the original addon", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    writeText(path.join(addon, GDEXTENSION_NAME), "old-extension");
    writeText(path.join(addon, artifacts.library), "old-library");
    writeText(path.join(addon, "User.cs"), "user-source");
    writeManifest(addon, [GDEXTENSION_NAME, artifacts.library]);
    const before = snapshotDirectory(addon);
    let originalMoved = false;
    let installFailed = false;

    const logs = captureLogs();
    try {
      const code = deployArtifacts("debug", [project], {
        binaryDir: artifacts.binaryDir,
        bridgeDir: artifacts.bridgeDir,
        platform: "linux",
        hooks: {
          rename(source, destination) {
            if (source === addon) {
              originalMoved = true;
            } else if (originalMoved && destination === addon && !installFailed) {
              installFailed = true;
              throw new Error("EBUSY: staging install failed");
            }
            fs.renameSync(source, destination);
          },
        },
      });

      expect(code).toBe(1);
      expect(installFailed).toBe(true);
      expect(snapshotDirectory(addon)).toEqual(before);
      expect(fs.readdirSync(path.dirname(addon))).toEqual(["godot-autopilot"]);
    } finally {
      logs.restore();
    }
  });

  test("a locked unknown temp file fails safely without deleting user data", () => {
    const { project, addon } = createProject();
    const artifacts = createArtifacts();
    writeText(path.join(addon, GDEXTENSION_NAME), "old-extension");
    writeText(path.join(addon, "~locked.tmp"), "locked-user-temp");
    const before = snapshotDirectory(addon);

    const logs = captureLogs();
    try {
      const code = deployArtifacts("debug", [project], {
        binaryDir: artifacts.binaryDir,
        bridgeDir: artifacts.bridgeDir,
        platform: "linux",
        hooks: {
          copyDirectory(source, destination) {
            if (fs.existsSync(path.join(source, "~locked.tmp"))) {
              throw new Error("EBUSY: ~locked.tmp is locked");
            }
            fs.cpSync(source, destination, { recursive: true, preserveTimestamps: true, verbatimSymlinks: true });
          },
        },
      });

      expect(code).toBe(1);
      expect(snapshotDirectory(addon)).toEqual(before);
      expect(logs.logs.some((line) => line.includes("EBUSY") && line.includes("~locked.tmp"))).toBe(true);
    } finally {
      logs.restore();
    }
  });
});
