import { describe, expect, test } from "bun:test";
import path from "node:path";

const port = process.env.GDA_LIVE_PORT ?? "";
const enabled = port !== "";
const root = path.resolve(import.meta.dir, "..", "..");
const cli = path.join(root, "cli", "bin", "gda.ts");

interface PtyResult {
  exit: number;
  output: string;
}

function runPty(args: string[], answer: string): Promise<PtyResult> {
  return new Promise((resolve, reject) => {
    let output = "";
    let answered = false;
    const decoder = new TextDecoder();
    const child = Bun.spawn([process.execPath, cli, ...args], {
      cwd: root,
      terminal: {
        cols: 120,
        rows: 40,
        data(terminal, chunk) {
          output += decoder.decode(chunk);
          if (!answered && output.includes("Accept? [y/N]")) {
            answered = true;
            terminal.write(`${answer}\r`);
          }
        },
      },
    });
    child.exited.then(
      (exit) => resolve({ exit, output }),
      (error: unknown) => reject(error),
    );
  });
}

async function callCli(args: string[]): Promise<number> {
  const child = Bun.spawn([process.execPath, cli, ...args], {
    cwd: root,
    stdout: "ignore",
    stderr: "ignore",
  });
  return await child.exited;
}

async function prepareScene(): Promise<void> {
  await callCli([
    "call",
    "create_editor_scene",
    "--args",
    JSON.stringify({ type: "Node2D", name: "TtyRoot", close_current: true }),
    "--port",
    port,
  ]);
}

describe.skipIf(!enabled)("live CLI MRTR over a pty", () => {
  test("accepting the prompt retries with inputResponses", async () => {
    await prepareScene();
    const created = await callCli([
      "call",
      "create_scene_node",
      "--args",
      JSON.stringify({ parent_path: ".", name: "TtySmoke", type: "Node2D" }),
      "--port",
      port,
    ]);
    expect(created).toBe(0);

    const result = await runPty(
      ["call", "delete_scene_node", "--direct", "--args", JSON.stringify({ path: "./TtySmoke" }), "--port", port],
      "y",
    );
    expect(result.exit).toBe(0);
    expect(result.output).toContain("Accept? [y/N]");
    expect(result.output).toContain('"result":"deleted"');
  });

  test("declining the prompt skips the call", async () => {
    const created = await callCli([
      "call",
      "create_scene_node",
      "--args",
      JSON.stringify({ parent_path: ".", name: "TtyDecline", type: "Node2D" }),
      "--port",
      port,
    ]);
    expect(created).toBe(0);

    const result = await runPty(
      ["call", "delete_scene_node", "--direct", "--args", JSON.stringify({ path: "./TtyDecline" }), "--port", port],
      "n",
    );
    expect(result.exit).toBe(1);
    expect(result.output).toContain("confirmation declined");
  });
});
