import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { BUILD_DIR, PROJECT_ROOT, cachedGenerator, resolvePreset } from "./config";

const DIAGNOSTIC_PROBE_BYTES = Buffer.from("gda-temp-probe\n", "utf8");
const CONFIGURE_TAIL_LINES = 40;

export function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function setEnv(env: Record<string, string>, name: string, value: string): void {
  for (const key of Object.keys(env)) {
    if (key !== name && key.toUpperCase() === name.toUpperCase()) delete env[key];
  }
  env[name] = value;
}

function redirectTempToBuild(env: Record<string, string>): void {
  const buildTmp = path.join(BUILD_DIR, "tmp");
  fs.mkdirSync(buildTmp, { recursive: true });
  for (const key of ["TMP", "TEMP", "TMPDIR"]) setEnv(env, key, buildTmp);
}

export function subprocessEnv(): Record<string, string> {
  const env: Record<string, string> = {};
  for (const [key, value] of Object.entries(process.env)) {
    if (value !== undefined) env[key] = value;
  }
  const pathKey = Object.keys(env).find((key) => key.toUpperCase() === "PATH") ?? "PATH";
  const ownDir = path.dirname(process.execPath);
  setEnv(env, pathKey, `${ownDir}${path.delimiter}${env[pathKey] ?? ""}`);
  if (process.platform === "win32") redirectTempToBuild(env);
  return env;
}

function openTeeLog(logPath: string | null): number | null {
  if (logPath === null) return null;
  try {
    fs.mkdirSync(path.dirname(logPath), { recursive: true });
    return fs.openSync(logPath, "w");
  } catch (error) {
    console.error(
      `[WARN] cannot open tee log ${logPath}: ${errorMessage(error)} (output stays on the console)`,
    );
    return null;
  }
}

function runInherited(cmd: string[], cwd: string): boolean {
  try {
    const result = Bun.spawnSync(cmd, {
      cwd,
      env: subprocessEnv(),
      stdin: "inherit",
      stdout: "inherit",
      stderr: "inherit",
    });
    return result.exitCode === 0;
  } catch (error) {
    console.error(`[ERROR] failed to start ${cmd[0]}: ${errorMessage(error)}`);
    return false;
  }
}

async function pump(stream: ReadableStream<Uint8Array>, logFd: number): Promise<void> {
  const reader = stream.getReader();
  for (;;) {
    const { done, value } = await reader.read();
    if (done) return;
    process.stdout.write(value);
    fs.writeSync(logFd, value);
  }
}

export async function run(
  cmd: string[],
  options: { cwd?: string; logPath?: string | null } = {},
): Promise<boolean> {
  process.stdout.write(`\n$ ${cmd.join(" ")}\n`);
  const logFd = openTeeLog(options.logPath ?? null);
  if (logFd === null) return runInherited(cmd, options.cwd ?? PROJECT_ROOT);
  fs.writeSync(logFd, `$ ${cmd.join(" ")}\n`);
  let processHandle: Bun.Subprocess<"inherit", "pipe", "pipe">;
  try {
    processHandle = Bun.spawn(cmd, {
      cwd: options.cwd ?? PROJECT_ROOT,
      env: subprocessEnv(),
      stdin: "inherit",
      stdout: "pipe",
      stderr: "pipe",
    });
  } catch (error) {
    const message = `[ERROR] failed to start ${cmd[0]}: ${errorMessage(error)}\n`;
    process.stderr.write(message);
    fs.writeSync(logFd, message);
    fs.writeSync(logFd, "[exit failed-to-start]\n");
    fs.closeSync(logFd);
    return false;
  }
  try {
    const stdout = processHandle.stdout;
    const stderr = processHandle.stderr;
    const pumps: Promise<void>[] = [];
    if (stdout !== undefined) pumps.push(pump(stdout, logFd));
    if (stderr !== undefined) pumps.push(pump(stderr, logFd));
    const code = await processHandle.exited;
    await Promise.all(pumps);
    fs.writeSync(logFd, `[exit ${code}]\n`);
    return code === 0;
  } finally {
    fs.closeSync(logFd);
  }
}

export function purge(target: string): void {
  console.log(`\n[AUTO-CLEAN] Removing ${target} (preserving _deps/)...`);
  for (const name of fs.readdirSync(target)) {
    if (name === "_deps") continue;
    try {
      fs.rmSync(path.join(target, name), { recursive: true, force: true });
    } catch {}
  }
}

function cacheEntry(cache: string, key: string): string | null {
  if (!fs.existsSync(cache)) return null;
  const text = fs.readFileSync(cache, "utf8");
  for (const line of text.split(/\r?\n/)) {
    const colon = line.indexOf(":");
    if (colon < 0) continue;
    if (line.slice(0, colon).trim() !== key) continue;
    const equals = line.indexOf("=", colon);
    const value = (equals < 0 ? "" : line.slice(equals + 1)).trim();
    return value === "" ? null : value;
  }
  return null;
}

function generatorChanged(target: string, wantNinja: boolean): boolean {
  const cached = cachedGenerator(target);
  return cached !== null && (cached === "Ninja") !== wantNinja;
}

function fileTail(file: string, count: number): string {
  let text: string;
  try {
    text = fs.readFileSync(file, "utf8");
  } catch {
    return "";
  }
  const rows = text.split(/\r?\n/);
  if (rows.length > 0 && rows[rows.length - 1] === "") rows.pop();
  return rows.slice(-count).join("\n");
}

function tempWriteProbe(): [boolean, string] {
  const probePath = path.join(
    os.tmpdir(),
    `gda-fs-probe-${process.pid}-${Date.now()}-${Math.random().toString(16).slice(2)}.tmp`,
  );
  try {
    fs.writeFileSync(probePath, DIAGNOSTIC_PROBE_BYTES);
  } catch (error) {
    return [false, `FAILED at create/write: ${errorMessage(error)}`];
  }
  try {
    fs.unlinkSync(probePath);
  } catch (error) {
    return [false, `FAILED at delete: ${errorMessage(error)} (${probePath})`];
  }
  return [true, `OK (${probePath})`];
}

function cmakeVersion(): string {
  try {
    const result = Bun.spawnSync(["cmake", "--version"], {
      env: subprocessEnv(),
      stdout: "pipe",
      stderr: "pipe",
    });
    const stdoutText = result.stdout.toString();
    const stderrText = result.stderr.toString();
    const text = (stdoutText.length > 0 ? stdoutText : stderrText).trim();
    const rows = text === "" ? [] : text.split(/\r?\n/);
    return rows.length > 0 ? rows[0] : `no output (exit ${result.exitCode})`;
  } catch (error) {
    return `unavailable (${errorMessage(error)})`;
  }
}

function rawTempWritable(): boolean {
  for (const key of ["TMP", "TEMP"]) {
    const value = process.env[key];
    if (value === undefined || value === "") continue;
    const probePath = path.join(value, `gda-temp-probe-${process.pid}.tmp`);
    try {
      fs.writeFileSync(probePath, DIAGNOSTIC_PROBE_BYTES);
      fs.unlinkSync(probePath);
      return true;
    } catch {}
  }
  return false;
}

function toolchainHints(
  preset: string,
  resolved: string,
  cache: string,
  tempOk: boolean,
): string[] {
  const hints: string[] = [];
  if (resolved === preset) {
    hints.push(
      `retry without ninja on PATH to select the portable preset, ` +
        `or run: cmake --preset ${preset}-portable`,
    );
  } else {
    hints.push(
      `the portable preset ${resolved} is already selected; ` +
        `if the cache is corrupt, remove build/${preset} and rerun`,
    );
  }
  const rcCompiler = cacheEntry(cache, "CMAKE_RC_COMPILER");
  if (rcCompiler !== null) {
    hints.push(
      `pass an explicit resource compiler: ` +
        `cmake --preset ${resolved} -DCMAKE_RC_COMPILER=<rc.exe or llvm-rc.exe> ` +
        `(cached: ${rcCompiler})`,
    );
  } else {
    hints.push(
      "install the Windows SDK resource compiler or pass " +
        "-DCMAKE_RC_COMPILER=<path to rc.exe / llvm-rc.exe>",
    );
  }
  if (!tempOk) {
    hints.push("point TMP and TEMP at a writable directory (the temp write probe failed) and rerun");
  }
  return hints;
}

function failureStamp(): string {
  const now = new Date();
  const pad = (value: number): string => String(value).padStart(2, "0");
  return (
    `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}-` +
    `${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`
  );
}

function preserveFailureScene(preset: string, target: string): string | null {
  const stamp = failureStamp();
  let scene = path.join(BUILD_DIR, `${preset}.failed-${stamp}`);
  let suffix = 1;
  while (fs.existsSync(scene)) {
    scene = path.join(BUILD_DIR, `${preset}.failed-${stamp}-${suffix}`);
    suffix += 1;
  }
  try {
    fs.mkdirSync(scene, { recursive: true });
  } catch (error) {
    console.error(`[WARN] cannot create the failure scene ${scene}: ${errorMessage(error)}`);
    return null;
  }
  const sources: Array<[string, string]> = [
    [path.join(target, "CMakeCache.txt"), "CMakeCache.txt"],
    [path.join(target, "CMakeFiles", "CMakeConfigureLog.yaml"), path.join("CMakeFiles", "CMakeConfigureLog.yaml")],
    [path.join(target, "configure.log"), "configure.log"],
  ];
  for (const [source, relative] of sources) {
    if (!fs.existsSync(source) || !fs.statSync(source).isFile()) continue;
    const destination = path.join(scene, relative);
    try {
      fs.mkdirSync(path.dirname(destination), { recursive: true });
      fs.cpSync(source, destination, { preserveTimestamps: true });
    } catch (error) {
      console.error(`[WARN] cannot preserve ${source}: ${errorMessage(error)}`);
    }
  }
  return scene;
}

function diagnoseConfigureFailure(preset: string): void {
  const target = path.join(BUILD_DIR, preset);
  const resolved = resolvePreset(preset);
  const cache = path.join(target, "CMakeCache.txt");
  const scene = preserveFailureScene(preset, target);
  const [tempOk, tempDetail] = tempWriteProbe();
  const lines = [
    "[DIAGNOSE] CMake configure failed",
    `  preset: ${preset} (resolved: ${resolved})`,
    `  cmake --version: ${cmakeVersion()}`,
    `  TMP=${process.env.TMP ?? "<unset>"}`,
    `  TEMP=${process.env.TEMP ?? "<unset>"}`,
    `  tempfile.gettempdir()=${os.tmpdir()}`,
    `  temp write probe: ${tempDetail}`,
    `  raw TMP/TEMP probe: ${rawTempWritable() ? "OK" : "FAILED (build/tmp fallback active)"}`,
    `  CMAKE_C_COMPILER=${cacheEntry(cache, "CMAKE_C_COMPILER") ?? "<unset>"}`,
    `  CMAKE_RC_COMPILER=${cacheEntry(cache, "CMAKE_RC_COMPILER") ?? "<unset>"}`,
    `  CMakeCache.txt=${fs.existsSync(cache) ? "present" : "missing"}`,
    `  CMakeConfigureLog.yaml=${fs.existsSync(path.join(target, "CMakeFiles", "CMakeConfigureLog.yaml")) ? "present" : "missing"}`,
    `  failure scene: ${scene ?? "<not created>"}`,
  ];
  for (const hint of toolchainHints(preset, resolved, cache, tempOk)) {
    lines.push(`  suggestion: ${hint}`);
  }
  let text = `${lines.join("\n")}\n`;
  process.stdout.write(`\n${text}`);
  if (scene === null) {
    console.error("[WARN] failure scene unavailable; diagnostics were printed to stdout only");
    return;
  }
  const configureTail = fileTail(path.join(target, "configure.log"), CONFIGURE_TAIL_LINES);
  if (configureTail !== "") text += `\n--- configure.log tail ---\n${configureTail}\n`;
  try {
    fs.writeFileSync(path.join(scene, "diagnostics.log"), text, "utf8");
  } catch (error) {
    console.error(`[WARN] diagnostics.log write failed: ${errorMessage(error)}`);
  }
}

export async function configure(preset: string): Promise<boolean> {
  const resolved = resolvePreset(preset);
  const target = path.join(BUILD_DIR, preset);
  const wantNinja = resolved === preset;
  const configureLog = path.join(target, "configure.log");
  const command = ["cmake", "--preset", resolved];

  if (generatorChanged(target, wantNinja)) {
    console.log(
      `\n[AUTO-CLEAN] Generator changed: ${cachedGenerator(target)} -> ` +
        `${wantNinja ? "Ninja" : "platform default"}`,
    );
    purge(target);
  }

  if (await run(command, { logPath: configureLog })) return true;

  diagnoseConfigureFailure(preset);

  if (generatorChanged(target, wantNinja)) {
    console.log("\n[AUTO-CLEAN] Generator still mismatched; purging and retrying configure once");
    purge(target);
    return run(command, { logPath: configureLog });
  }

  console.log("\n[AUTO-CLEAN] Skipped: configure failure reason unchanged (原因未变，跳过无意义重试).");
  return false;
}

export async function buildPreset(preset: string): Promise<boolean> {
  return run(["cmake", "--build", "--preset", resolvePreset(preset)], {
    logPath: path.join(BUILD_DIR, preset, "build.log"),
  });
}
