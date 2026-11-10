import fs from "node:fs";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { run as runProjects } from "../src/commands/projects";
import { ExitCode } from "../src/errors";

const DEAD_PID = 2147483647;

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
    const code = await runProjects(positional, opts);
    return { code, stdout: cap.stdout(), stderr: cap.stderr() };
  } finally {
    cap.restore();
  }
}

let tmpHome: string;
let prevHome: string | undefined;
let gdaRoot: string;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmpHome = fs.mkdtempSync(path.join(base, "gda-projects-test-"));
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmpHome;
  gdaRoot = path.join(tmpHome, ".godot-autopilot");
});

afterEach(() => {
  if (prevHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_HOME = prevHome;
  }
  fs.rmSync(tmpHome, { recursive: true, force: true });
});

function seedProject(dirName: string, record?: Record<string, unknown>): void {
  const projectDir = path.join(gdaRoot, "projects", dirName);
  fs.mkdirSync(path.join(projectDir, "instances"), { recursive: true });
  if (record !== undefined) {
    fs.writeFileSync(path.join(projectDir, "instances", "rec.json"), JSON.stringify(record), "utf8");
  }
}

describe("projects", () => {
  test("聚合工程名、hash、实例数与存活状态", async () => {
    const started = Date.UTC(2026, 10, 10, 6, 0, 0);
    seedProject("alpha-1234abcd", {
      pid: DEAD_PID,
      project_path: path.join(tmpHome, "alpha"),
      started_at: started,
    });
    fs.mkdirSync(path.join(gdaRoot, "projects", "beta-5678ef90"), { recursive: true });

    const result = await runCaptured([], {});
    expect(result.code).toBe(ExitCode.Success);
    const projects = JSON.parse(result.stdout) as Array<{
      name: string;
      path: string;
      hash: string;
      instances: number;
      lastActive: string;
      live: boolean;
    }>;
    expect(projects.length).toBe(2);
    const alpha = projects.find((p) => p.name === "alpha");
    expect(alpha).toBeDefined();
    expect(alpha?.hash).toBe("1234abcd");
    expect(alpha?.instances).toBe(1);
    expect(alpha?.path.endsWith("alpha")).toBe(true);
    expect(alpha?.live).toBe(false);
    expect(alpha?.lastActive).not.toBe("");

    const beta = projects.find((p) => p.name === "beta");
    expect(beta).toBeDefined();
    expect(beta?.hash).toBe("5678ef90");
    expect(beta?.instances).toBe(0);
    expect(beta?.path).toBe("");
    expect(beta?.live).toBe(false);
  });

  test("无工程目录时输出空数组并给出提示", async () => {
    const result = await runCaptured([], {});
    expect(result.code).toBe(ExitCode.Success);
    expect(JSON.parse(result.stdout)).toEqual([]);
    expect(result.stderr).toContain("no projects found");
  });
});