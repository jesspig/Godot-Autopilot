import { describe, expect, test } from "bun:test";

import { restartPortNote } from "../src/commands/restart";

describe("restart 端口语义", () => {
  test("端口变化时回包说明新端口由插件决定", () => {
    const note = restartPortNote(9527, 9531);
    expect(note).toContain("9531");
    expect(note).toContain("plugin");
  });

  test("端口相同时无需说明", () => {
    expect(restartPortNote(9527, 9527)).toBeNull();
  });
});
