import { spawnSync } from "node:child_process";
import fs from "node:fs";
import net from "node:net";
import path from "node:path";

import type { GlobalOptions } from "../args";
import { callTool } from "../mcp/client";
import { CliError, ExitCode } from "../errors";
import { err } from "../output";
import { joinPath } from "../platform";
import {
  detectActiveInstance,
  describeInstance,
  multiInstanceAllowed,
  MULTI_INSTANCE_ENV,
  resolveProbeLevel,
  type ActiveInstance,
} from "../preflight";
import { isAlive, resolveGodotBin, spawnEditor, terminateProcess, terminationNote } from "../process";
import { loadRegistry, saveRegistry, stateDir, type InstanceEntry } from "../registry";
import { writeJson } from "./json-output";

const PORT_RANGE_START = 9530;
const PORT_RANGE_END = 9599;
const DEFAULT_WAIT_SEC = 60;
const IMPORT_TIMEOUT_MS = 120_000;
const PROBE_TIMEOUT_SEC = 5;
const PROBE_INTERVAL_MS = 500;
const LOG_TAIL_LINES = 5;
const LOG_TAIL_MAX_CHARS = 400;

export interface StartRunOptions {
  writeOutput?: boolean;
}

export async function run(_positional: string[], opts: GlobalOptions): Promise<number> {
  try {
    await start(opts);
    return ExitCode.Success;
  } catch (error) {
    if (error instanceof CliError) {
      err(error.message);
      if (error.suggestion !== undefined) {
        process.stderr.write(`[gda] suggestion: ${error.suggestion}\n`);
      }
      return error.code;
    }
    throw error;
  }
}

export async function start(opts: GlobalOptions, runOpts: StartRunOptions = {}): Promise<InstanceEntry> {
  const projectDir = path.resolve(opts.project ?? process.cwd());
  const explicitPort = opts.portText !== undefined ? parseExplicitPort(opts.portText) : null;

  const active = await detectActiveInstance(projectDir, resolveProbeLevel(opts.probe));
  if (active !== undefined) {
    assertStartAllowed(active, projectDir, explicitPort);
  }

  const waitSec = parseWaitSec(opts.waitText, DEFAULT_WAIT_SEC);
  const { bin } = resolveGodotBin({ explicit: opts.godot });
  await runImportStep(bin, projectDir, opts.noImport === true);

  const port = explicitPort ?? await pickFreePort();
  const headless = opts.headless === true;

  const logsDir = joinPath(stateDir(), "logs");
  fs.mkdirSync(logsDir, { recursive: true });
  const pendingLogPath = joinPath(logsDir, `.pending-${port}-${Date.now()}.log`);

  let pid: number;
  try {
    pid = (await spawnEditor({ projectDir, port, headless, godot: bin, logPath: pendingLogPath })).pid;
  } catch (error) {
    fs.rmSync(pendingLogPath, { force: true });
    throw error;
  }

  const id = `p${pid}`;
  if (waitSec > 0) {
    try {
      await waitForReady(port, pid, waitSec, pendingLogPath);
    } catch (error) {
      throw await rollbackFailedStart(error, pid, pendingLogPath);
    }
  }

  let finalLogPath = joinPath(logsDir, `${id}.log`);
  try {
    fs.renameSync(pendingLogPath, finalLogPath);
  } catch {
    finalLogPath = pendingLogPath;
  }

  const entry: InstanceEntry = {
    id,
    project: projectDir,
    port,
    pid,
    startedAt: new Date().toISOString(),
    headless,
    godot: bin,
    log: finalLogPath,
    projectPath: projectDir,
    instancePort: port,
    apiAvailable: false,
  };
  const data = loadRegistry();
  data.instances = data.instances.filter((e) => e.id !== id);
  data.instances.push(entry);
  saveRegistry(data);

  if (opts.play === true) {
    try {
      await callTool("play_editor_current_scene", {}, { port: entry.port, timeoutSec: PROBE_TIMEOUT_SEC });
    } catch (error) {
      err(`play request failed (non-fatal): ${(error as Error).message}`);
    }
  }

  if (runOpts.writeOutput !== false) writeJson(instanceView(entry), opts);
  return entry;
}

export function instanceView(e: InstanceEntry): Record<string, unknown> {
  return {
    id: e.id,
    project: e.project,
    port: e.port,
    pid: e.pid,
    headless: e.headless,
    startedAt: e.startedAt,
    live: isAlive(e.pid),
  };
}

const SECOND_EDITOR_RISK =
  "; a second editor on the same project may die without any message because Godot loads the plugin through a shared temporary copy";

function assertStartAllowed(
  active: ActiveInstance,
  projectDir: string,
  explicitPort: number | null,
): void {
  const where = describeInstance(active);
  if (explicitPort !== null) {
    const samePort = active.port !== null && active.port === explicitPort;
    if (samePort) {
      throw new CliError(
        ExitCode.Transport,
        `refusing to start: this project already has a live Godot Autopilot instance on ${where}; the requested port ${explicitPort} is the port that instance is already using`,
        `stop it first (gda stop --project ${projectDir}), or pass --port <another port>, or set ${MULTI_INSTANCE_ENV}=1 to allow a second instance`,
      );
    }
    err(
      `warning: this project already has a live Godot Autopilot instance on ${where}` +
        `; starting on port ${explicitPort} anyway as requested` +
        SECOND_EDITOR_RISK,
    );
    return;
  }
  if (multiInstanceAllowed()) {
    err(
      `warning: this project already has a live Godot Autopilot instance on ${where}` +
        `; starting anyway because ${MULTI_INSTANCE_ENV} is set` +
        SECOND_EDITOR_RISK,
    );
    return;
  }
  throw new CliError(
    ExitCode.Transport,
    `refusing to start: this project already has a live Godot Autopilot instance on ${where}`,
    `stop it first (gda stop --project ${projectDir}), or pass --port <port> to start on another port, or set ${MULTI_INSTANCE_ENV}=1 to allow a second instance`,
  );
}

async function waitForReady(port: number, pid: number, waitSec: number, logPath: string): Promise<void> {
  const deadline = Date.now() + waitSec * 1000;
  for (;;) {
    if (!isAlive(pid)) {
      throw new CliError(
        ExitCode.Transport,
        `editor process exited before becoming ready${logTailSuffix(logPath)}`,
        "check the editor log and the Godot binary",
      );
    }
    try {
      await callTool("ping", {}, { port, timeoutSec: PROBE_TIMEOUT_SEC, direct: true });
      return;
    } catch {
      if (Date.now() >= deadline) {
        throw new CliError(
          ExitCode.Transport,
          `editor not ready within ${waitSec}s on port ${port}${logTailSuffix(logPath)}`,
          "raise --wait, or check that the Godot Autopilot plugin is enabled",
        );
      }
      await new Promise((resolve) => setTimeout(resolve, PROBE_INTERVAL_MS));
    }
  }
}

async function rollbackFailedStart(error: unknown, pid: number, logPath: string): Promise<CliError> {
  const outcome = await terminateProcess(pid);
  const base = error instanceof CliError ? error : new CliError(ExitCode.Transport, String(error));
  const processNote = terminationNote(outcome, pid);
  return new CliError(base.code, `${base.message}; ${processNote}; ${removePendingLog(logPath)}`, base.suggestion);
}

function removePendingLog(logPath: string): string {
  try {
    fs.rmSync(logPath, { force: true });
    return "its pending log was removed";
  } catch (error) {
    return `its pending log could not be removed (${(error as Error).message})`;
  }
}

function logTailSuffix(logPath: string): string {
  const tail = readLogTail(logPath);
  return tail === "" ? "" : ` (log tail: ${tail})`;
}

function readLogTail(logPath: string): string {
  let text: string;
  try {
    text = fs.readFileSync(logPath, "utf8");
  } catch {
    return "";
  }
  const lines = text.split(/\r?\n/).filter((line) => line.trim() !== "");
  if (lines.length === 0) return "";
  const tail = lines.slice(-LOG_TAIL_LINES).join(" | ");
  return tail.length > LOG_TAIL_MAX_CHARS ? `${tail.slice(0, LOG_TAIL_MAX_CHARS)}...` : tail;
}

async function runImportStep(bin: string, projectDir: string, skip: boolean): Promise<void> {
  if (skip) return;
  try {
    const result = spawnSync(bin, ["--headless", "--import", "--path", projectDir], {
      timeout: IMPORT_TIMEOUT_MS,
      stdio: ["ignore", "pipe", "pipe"],
    });
    if (result.error !== undefined || result.status !== 0) {
      err(`editor import step failed (non-fatal): ${result.error?.message ?? `exit ${result.status}`}`);
    }
  } catch (error) {
    err(`editor import step error (non-fatal): ${(error as Error).message}`);
  }
}

async function pickFreePort(): Promise<number> {
  for (let candidate = PORT_RANGE_START; candidate <= PORT_RANGE_END; candidate += 1) {
    if (await isPortFree(candidate)) return candidate;
  }
  throw new CliError(
    ExitCode.Transport,
    `no free port in ${PORT_RANGE_START}-${PORT_RANGE_END}`,
    "pass --port to choose a specific port",
  );
}

function isPortFree(port: number): Promise<boolean> {
  return new Promise((resolve) => {
    const server = net.createServer();
    server.once("error", () => resolve(false));
    server.once("listening", () => {
      server.close(() => resolve(true));
    });
    server.listen(port, "127.0.0.1");
  });
}

function parseExplicitPort(text: string): number {
  const trimmed = text.trim();
  if (!/^\d+$/.test(trimmed)) {
    throw new CliError(ExitCode.Usage, `--port must be an integer in 1-65535, got: ${text}`);
  }
  const n = Number(trimmed);
  if (n < 1 || n > 65535) {
    throw new CliError(ExitCode.Usage, `--port must be an integer in 1-65535, got: ${text}`);
  }
  return n;
}

function parseWaitSec(text: string | undefined, fallback: number): number {
  if (text === undefined) return fallback;
  const n = Number(text);
  if (!Number.isFinite(n) || n < 0) {
    throw new CliError(ExitCode.Usage, `--wait must be a non-negative number of seconds, got: ${text}`);
  }
  return n;
}
