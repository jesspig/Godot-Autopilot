import { afterAll, beforeAll, describe, expect, test } from "bun:test";
import { spawn, spawnSync } from "node:child_process";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { isAlive, spawnEditor, stopForce } from "../src/process";

const FAKE_GODOT_SOURCE = `
const port = Number(process.env.GODOT_AUTOPILOT_PORT ?? "0");
const net = require("node:net");
const s = net.createServer();
s.listen(port, "127.0.0.1", () => {
  console.log("READY");
});
s.on("connection", (c) => c.end("ok"));
`;

let tmp: string;
let fakeGodotExe: string | undefined;

beforeAll(async () => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-detach-"));
  const scriptPath = path.join(tmp, "fake_godot.js");
  fs.writeFileSync(scriptPath, FAKE_GODOT_SOURCE);
  const result = await Bun.build({
    entrypoints: [scriptPath],
    outdir: path.join(tmp, "bin"),
    target: "bun",
    compile: true,
  });
  if (!result.success) {
    throw new Error(`fake godot build failed: ${result.logs.join("\n")}`);
  }
  fakeGodotExe = result.outputs[0].path;
  if (process.platform !== "win32") {
    fs.chmodSync(fakeGodotExe, 0o755);
  }
});

afterAll(() => {
  if (process.platform === "win32") {
    spawnSync("powershell", [
      "-Command",
      "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'gda-detach' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }",
    ], { stdio: "ignore" });
  }
  fs.rmSync(tmp, { recursive: true, force: true });
});

function findFreePort(): Promise<number> {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      if (address === null || typeof address === "string") {
        server.close(() => reject(new Error("no address")));
        return;
      }
      server.close(() => resolve(address.port));
    });
  });
}

describe.serial("spawnEditor 脱父验证", () => {
  test("子进程退出后目标进程仍存活", async () => {
    const port = await findFreePort();
    const logPath = path.join(tmp, "editor.log");

    const childScript = `
      const { spawnEditor } = require(${JSON.stringify(path.resolve(import.meta.dir, "../src/process"))});
      const result = await spawnEditor({
        projectDir: ${JSON.stringify(tmp)},
        port: ${port},
        headless: true,
        godot: ${JSON.stringify(fakeGodotExe)},
        logPath: ${JSON.stringify(logPath)},
      });
      console.log("PID=" + result.pid);
      process.exit(0);
    `;

    const child = spawn(process.execPath, ["-e", childScript], {
      cwd: path.resolve(import.meta.dir, ".."),
      stdio: ["ignore", "pipe", "pipe"],
    });

    let stdout = "";
    child.stdout.on("data", (d: Buffer) => {
      stdout += d.toString();
    });

    const exitCode = await new Promise<number>((resolve) => {
      child.on("exit", (code) => resolve(code ?? 0));
    });
    expect(exitCode).toBe(0);

    const match = stdout.match(/PID=(\d+)/);
    expect(match).not.toBeNull();
    const pid = parseInt(match![1], 10);

    await Bun.sleep(500);
    expect(isAlive(pid)).toBe(true);

    stopForce(pid);
    const start = Date.now();
    while (Date.now() - start < 5000) {
      if (!isAlive(pid)) break;
      await Bun.sleep(100);
    }
    expect(isAlive(pid)).toBe(false);
  }, 30000);
});
