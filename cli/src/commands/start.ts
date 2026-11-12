import { spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

import type { GlobalOptions } from "../args";
import { callTool } from "../mcp/client";
import { CliError, ExitCode } from "../errors";
import { err } from "../output";
import { joinPath, projectPathKey } from "../platform";
import { readPluginInstanceIndex, type PluginInstanceRecord } from "../instances";
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

const DEFAULT_WAIT_SEC = 60;
const IMPORT_TIMEOUT_MS = 120_000;
const READY_POLL_INTERVAL_MS = 500;
const PLAY_TIMEOUT_SEC = 5;
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
  if (opts.portText !== undefined) {
    throw new CliError(
      ExitCode.Usage,
      "gda start no longer selects or injects a port; the plugin owns the MCP port.",
      "Remove --port. Use --port only on connection commands such as gda call/status.",
    );
  }

  const active = await detectActiveInstance(projectDir, resolveProbeLevel(opts.probe));
  if (active !== undefined) {
    assertStartAllowed(active, projectDir);
  }

  const waitSec = parseWaitSec(opts.waitText, DEFAULT_WAIT_SEC);
  const { bin } = resolveGodotBin({ explicit: opts.godot });
  await runImportStep(bin, projectDir, opts.noImport === true);

  const headless = opts.headless === true;
  const logsDir = joinPath(stateDir(), "logs");
  fs.mkdirSync(logsDir, { recursive: true });
  const pendingLogPath = joinPath(logsDir, `.pending-${Date.now()}.log`);

  let pid: number;
  try {
    pid = (await spawnEditor({ projectDir, headless, godot: bin, logPath: pendingLogPath })).pid;
  } catch (error) {
    fs.rmSync(pendingLogPath, { force: true });
    throw error;
  }

  const id = `p${pid}`;
  let record: PluginInstanceRecord | undefined;
  if (waitSec > 0) {
    try {
      record = await waitForPluginRecord(projectDir, pid, waitSec, pendingLogPath);
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

  const port = record?.port ?? 0;
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
      await callTool("play_editor_current_scene", {}, { port: entry.port, timeoutSec: PLAY_TIMEOUT_SEC });
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

function assertStartAllowed(active: ActiveInstance, projectDir: string): void {
  const where = describeInstance(active);
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
    `stop it first (gda stop --project ${projectDir}), or set ${MULTI_INSTANCE_ENV}=1 to allow a second instance`,
  );
}

async function waitForPluginRecord(
  projectDir: string,
  pid: number,
  waitSec: number,
  logPath: string,
): Promise<PluginInstanceRecord> {
  const deadline = Date.now() + waitSec * 1000;
  const key = projectPathKey(projectDir);
  for (;;) {
    if (!isAlive(pid)) {
      throw new CliError(
        ExitCode.Transport,
        `editor process exited before the plugin registered an instance${logTailSuffix(logPath)}`,
        "check the editor log and the Godot binary",
      );
    }
    const record = readPluginInstanceIndex().find(
      (entry) => entry.pid === pid && projectPathKey(entry.projectPath) === key,
    );
    if (record !== undefined) {
      return record;
    }
    if (Date.now() >= deadline) {
      throw new CliError(
        ExitCode.Transport,
        `the plugin did not register an instance within ${waitSec}s${logTailSuffix(logPath)}`,
        "raise --wait, or check that the Godot Autopilot plugin is enabled and its MCP server started",
      );
    }
    await new Promise((resolve) => setTimeout(resolve, READY_POLL_INTERVAL_MS));
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
  // 首行通常是异常本体（EACCES/ENOENT 的完整消息），栈帧在其后；首行必须完整保留，
  // 否则定位时只剩栈而丢掉失败原因。其余行沿用既有条数与字符上限。
  const head = lines[0];
  const rest = lines.slice(1);
  const tail = (rest.length <= LOG_TAIL_LINES - 1 ? rest : rest.slice(-(LOG_TAIL_LINES - 1))).join(" | ");
  if (tail === "") return head;
  const shortened = tail.length > LOG_TAIL_MAX_CHARS ? `${tail.slice(0, LOG_TAIL_MAX_CHARS)}...` : tail;
  return `${head} | ${shortened}`;
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

function parseWaitSec(text: string | undefined, fallback: number): number {
  if (text === undefined) return fallback;
  const n = Number(text);
  if (!Number.isFinite(n) || n < 0) {
    throw new CliError(ExitCode.Usage, `--wait must be a non-negative number of seconds, got: ${text}`);
  }
  return n;
}
