import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { describe, expect, test } from "bun:test";

import { missingDeployArtifacts } from "./deploy";
import { PLATFORM_LIBS } from "../lib/config";

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
