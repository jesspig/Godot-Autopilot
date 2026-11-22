import { describe, expect, test } from "bun:test";

import { shouldStopByPid } from "../src/process";

describe("实例身份二次确认", () => {
  test("pid 不存活时不发信号", () => {
    expect(shouldStopByPid(false, 9527, true)).toBe(false);
  });

  test("pid 存活且端点存活时发信号", () => {
    expect(shouldStopByPid(true, 9527, true)).toBe(true);
  });

  test("pid 存活但端点明确不存活时不发信号（防 PID 复用误杀）", () => {
    expect(shouldStopByPid(true, 9527, false)).toBe(false);
  });

  test("无端点信息时按 pid 判定，不误拒", () => {
    expect(shouldStopByPid(true, null, null)).toBe(true);
    expect(shouldStopByPid(true, 0, null)).toBe(true);
  });
});
