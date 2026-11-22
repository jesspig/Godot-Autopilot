import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { withRegistryLock, type InstanceEntry } from "../src/registry";

let tmp: string;
let prevHome: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-registry-atomic-"));
  prevHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
});

afterEach(() => {
  if (prevHome === undefined) delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  else process.env.GODOT_AUTOPILOT_CLI_HOME = prevHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

function entry(id: string): InstanceEntry {
  return {
    id,
    pid: process.pid,
    port: 1,
    project: "p",
    projectPath: "p",
    instancePort: 1,
    apiAvailable: false,
    startedAt: new Date().toISOString(),
    headless: false,
    godot: "godot",
    log: "log",
  };
}

describe("withRegistryLock 原子读改写", () => {
  test("连续的读改写在单锁内完成且不丢记录", () => {
    withRegistryLock((data) => { data.instances.push(entry("id-a")); });
    withRegistryLock((data) => { data.instances.push(entry("id-b")); });
    const raw = fs.readFileSync(path.join(tmp, "instances.json"), "utf8");
    const doc = JSON.parse(raw) as { instances: { id: string }[] };
    expect(doc.instances.map((e) => e.id).sort()).toEqual(["id-a", "id-b"]);
  });

  test("mutate 的返回值被透传", () => {
    const n = withRegistryLock((data) => { data.instances.push(entry("id-c")); return data.instances.length; });
    expect(n).toBe(1);
  });
});
