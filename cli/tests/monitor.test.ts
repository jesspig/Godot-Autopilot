import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { expect, test } from "bun:test";

import { analyzeMonitorFile, parseMonitorText } from "../src/monitor";

const SAMPLE = [
  JSON.stringify({
    type: "session",
    schema: 1,
    id: "s-1",
    builtin_monitors: [
      { i: 0, n: "time/fps", t: "time" },
      { i: 1, n: "object/count", t: "quantity" },
    ],
    custom_monitors: [{ i: 0, n: "Game/Enemies", t: "quantity", cat: "Game" }],
  }),
  JSON.stringify({ type: "frame", n: 1, d: 10, t: 0, v: [50, 100], c: [1] }),
  JSON.stringify({ type: "custom_change", n: 2, custom_monitors: [{ i: 0, n: "Game/DPS", t: "time", cat: "Game" }] }),
  JSON.stringify({ type: "frame", n: 2, d: 20, t: 10, v: [60, 110], c: [5] }),
  JSON.stringify({ type: "frame", n: 3, d: 30, t: 30, v: [70, 120], c: [9] }),
  "",
].join("\n");

test("parseMonitorText reads session, frames and custom changes", () => {
  const session = parseMonitorText(SAMPLE);
  expect(session.sessionId).toBe("s-1");
  expect(session.builtin.length).toBe(2);
  expect(session.custom[0].n).toBe("Game/DPS");
  expect(session.frames.length).toBe(3);
});

test("analyzeMonitorFile computes frame and series statistics", () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-monitor-test-"));
  const file = path.join(dir, "session.jsonl");
  fs.writeFileSync(file, SAMPLE, "utf8");
  const report = analyzeMonitorFile(file, 25);
  expect(report.frames).toBe(3);
  expect(report.duration_ms).toBe(30);
  expect(report.frame.avg_ms).toBe(20);
  expect(report.frame.max_ms).toBe(30);
  expect(report.frame.slow_frames).toBe(1);
  expect(report.series[0].name).toBe("time/fps");
  expect(report.series[0].avg).toBe(60);
  expect(report.series[2].name).toBe("Game/DPS");
  fs.rmSync(dir, { recursive: true, force: true });
});
