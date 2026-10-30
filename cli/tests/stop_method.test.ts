import { describe, expect, test } from "bun:test";

import { stopMethod } from "../src/commands/stop";

describe("停止方法选择", () => {
  test("显式强制时强制", () => {
    expect(stopMethod(true, false)).toBe("force");
    expect(stopMethod(true, true)).toBe("force");
  });

  test("无窗口实例强制，因为没有窗口可响应优雅关闭请求", () => {
    expect(stopMethod(undefined, true)).toBe("force");
    expect(stopMethod(false, true)).toBe("force");
  });

  test("有窗口且未强制时优雅关闭", () => {
    expect(stopMethod(undefined, false)).toBe("graceful");
    expect(stopMethod(false, false)).toBe("graceful");
  });
});
