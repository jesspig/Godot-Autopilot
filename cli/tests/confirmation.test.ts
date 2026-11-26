import { describe, expect, test } from "bun:test";

import { ExitCode } from "../src/errors";
import {
  confirmationText,
  confirmOrDecline,
  isConfirmed,
  promptConfirmation,
  requireConfirmation,
  type ConfirmationSummary,
} from "../src/output";

function summary(): ConfirmationSummary {
  return {
    command: "scene.node.add",
    tool: "create_scene_node",
    target: "instance:gda-1",
    risk: "high",
    args: { type: "Node2D", name: "Player" },
  };
}

function captureStderr(fn: () => void): string {
  const original = process.stderr.write;
  let text = "";
  process.stderr.write = ((chunk: string | Uint8Array) => {
    text += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  try {
    fn();
  } finally {
    process.stderr.write = original;
  }
  return text;
}

describe("业务确认语义", () => {
  test("确认摘要包含工具、目标、风险与参数", () => {
    const text = confirmationText(summary());
    expect(text).toContain("scene.node.add");
    expect(text).toContain("create_scene_node");
    expect(text).toContain("instance:gda-1");
    expect(text).toContain("risk=high");
    expect(text).toContain("Node2D");
  });

  test("--confirm 只走业务确认并写入 stderr 摘要", () => {
    const opts = { confirm: true };
    expect(isConfirmed(opts)).toBe(true);
    const text = captureStderr(() => requireConfirmation(summary(), opts));
    expect(text).toContain("confirm scene.node.add");
  });

  test("非 TTY 缺确认停止且不执行工具", async () => {
    const original = process.stdin.isTTY;
    Object.defineProperty(process.stdin, "isTTY", { value: false, configurable: true });
    try {
      let threw = false;
      try {
        await promptConfirmation(summary(), {});
      } catch (error) {
        threw = true;
        const record = error as { code?: number; details?: { confirmation?: ConfirmationSummary } };
        expect(record.code).toBe(ExitCode.ConfirmationRequired);
        expect(record.details?.confirmation?.tool).toBe("create_scene_node");
      }
      expect(threw).toBe(true);
    } finally {
      Object.defineProperty(process.stdin, "isTTY", { value: original, configurable: true });
    }
  });

  test("交互式拒绝返回 5，接受才继续", () => {
    expect(() => confirmOrDecline("n", summary())).toThrow();
    try {
      confirmOrDecline("no", summary());
    } catch (error) {
      expect((error as { code: number }).code).toBe(ExitCode.UserDeclined);
    }
    const accepted = captureStderr(() => confirmOrDecline("yes", summary()));
    expect(accepted).toContain("confirm scene.node.add");
  });

  test("缺确认与拒绝退出码分别为 4 与 5，不同于用法错误 3", () => {
    expect(ExitCode.ConfirmationRequired).toBe(4);
    expect(ExitCode.UserDeclined).toBe(5);
    expect(ExitCode.Usage).toBe(3);
  });
});
