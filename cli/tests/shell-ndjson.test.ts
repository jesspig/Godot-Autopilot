import { describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { createShellSession, run as runShell, type ShellFrame } from "../src/commands/shell";
import { CliError } from "../src/errors";

function collect(): { frames: ShellFrame[]; emit(frame: ShellFrame): void } {
  const frames: ShellFrame[] = [];
  return { frames, emit: (frame) => frames.push(frame) };
}

describe("gda shell --protocol ndjson", () => {
  test("argv 请求回显 id 与 argv，子进程退出产生 exitCode", async () => {
    const sink = collect();
    const session = createShellSession(sink.emit);
    await session.handle(JSON.stringify({ type: "argv", id: "run-1", argv: [process.execPath, "-e", "process.stdout.write('{\"ok\":true}\\n')"] }));
    await new Promise<void>((resolve) => setTimeout(resolve, 400));
    const argvFrame = sink.frames.find((frame) => frame.type === "argv");
    expect(argvFrame?.id).toBe("run-1");
    expect(Array.isArray(argvFrame?.argv)).toBe(true);
    const envelope = sink.frames.find((frame) => frame.type === "envelope");
    expect(envelope?.id).toBe("run-1");
    expect((envelope?.envelope as { ok?: boolean }).ok).toBe(true);
    const exit = sink.frames.find((frame) => frame.type === "exitCode");
    expect(exit?.id).toBe("run-1");
    expect(exit?.exitCode).toBe(0);
    await session.shutdown();
  }, 10_000);

  test("envelope 请求按 id 转发", async () => {
    const sink = collect();
    const session = createShellSession(sink.emit);
    await session.handle(JSON.stringify({ type: "envelope", id: "env-1", envelope: { schema_version: 1, ok: true } }));
    expect(sink.frames).toEqual([{ type: "envelope", id: "env-1", envelope: { schema_version: 1, ok: true } }]);
  });

  test("shutdown 请求终止子进程并确认 id", async () => {
    const sink = collect();
    const session = createShellSession(sink.emit);
    await session.handle(JSON.stringify({ type: "argv", id: "long-1", argv: [process.execPath, "-e", "setTimeout(()=>{},30000)"] }));
    const done = await session.handle(JSON.stringify({ type: "shutdown", id: "shutdown-1" }));
    expect(done).toBe(true);
    const shutdown = sink.frames.find((frame) => frame.type === "shutdown");
    expect(shutdown?.id).toBe("shutdown-1");
    expect(shutdown?.ok).toBe(true);
    await session.shutdown();
  }, 10_000);

  test("未知请求类型产生 error frame", async () => {
    const sink = collect();
    const session = createShellSession(sink.emit);
    await session.handle(JSON.stringify({ type: "unknown", id: "bad-1" }));
    expect(sink.frames[0]).toEqual({ type: "error", id: "bad-1", error: "unknown shell request type: unknown" });
  });

  test("非 ndjson protocol 报用法错误", async () => {
    let caught: unknown;
    try {
      await runShell([], {} as GlobalOptions);
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).message).toContain("--protocol ndjson");
  });
});
