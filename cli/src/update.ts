import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { extractZip, downloadToFile } from "./archive";
import { cacheRoot, selectLatest, verifyManifest, cacheAssetPath } from "./cache";
import { CliError, ExitCode } from "./errors";
import { isAlive, stopForce } from "./process";
import { CLI_VERSION } from "./version";

export interface UpdateItemResult {
  artifact: "skills" | "plugin" | "cli";
  from: string;
  to: string;
  source: "github" | "atomgit" | "cache" | "builtin" | "archive";
  ok: boolean;
  action: "updated" | "skipped" | "rolled_back" | "failed";
  error: string;
}

export interface UpdateResult {
  schema_version: 1;
  started_at: string;
  finished_at: string;
  targets: string[];
  results: UpdateItemResult[];
  rollback: { performed: boolean; reason: string };
  cli_restart: { exit_requested: boolean; exit_code: number };
}

export function updateResultPath(): string {
  return path.join(cacheRoot(), "update-result.json");
}

export function writeUpdateResult(result: UpdateResult): void {
  fs.mkdirSync(cacheRoot(), { recursive: true });
  const target = updateResultPath();
  const tmp = `${target}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, `${JSON.stringify(result, null, 2)}\n`, "utf8");
  fs.renameSync(tmp, target);
}

export function platformAsset(): { asset: string; platform: string; ext: string } {
  const platform = process.platform;
  const arch = process.arch;
  if (platform === "win32" && arch === "x64") {
    return { asset: `gda-${CLI_VERSION}-windows-x64.zip`, platform: "windows-x64", ext: "zip" };
  }
  if (platform === "linux" && arch === "x64") {
    return { asset: `gda-${CLI_VERSION}-linux-x64.tar.gz`, platform: "linux-x64", ext: "tar.gz" };
  }
  if (platform === "darwin" && arch === "arm64") {
    return { asset: `gda-${CLI_VERSION}-macos-arm64.tar.gz`, platform: "macos-arm64", ext: "tar.gz" };
  }
  if (platform === "darwin" && arch === "x64") {
    return { asset: `gda-${CLI_VERSION}-macos-x64.tar.gz`, platform: "macos-x64", ext: "tar.gz" };
  }
  throw new CliError(ExitCode.Transport, `unsupported update platform: ${platform}/${arch}`);
}

function parseFlag(argv: string[], name: string): string | undefined {
  const index = argv.indexOf(name);
  if (index === -1 || index + 1 >= argv.length) return undefined;
  return argv[index + 1];
}

function waitForParent(parentPid: number): void {
  if (!Number.isInteger(parentPid) || parentPid <= 0) return;
  const deadline = Date.now() + 60_000;
  while (isAlive(parentPid) && Date.now() < deadline) {
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 100);
  }
  if (isAlive(parentPid)) {
    stopForce(parentPid);
  }
}

function runVersion(binary: string): string {
  const result = Bun.spawnSync([binary, "--version"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (result.exitCode !== 0) {
    throw new CliError(ExitCode.Transport, `updated binary failed --version: ${result.stderr.toString()}`);
  }
  return result.stdout.toString().trim();
}

export function applyUpdate(argv: string[]): number {
  const source = parseFlag(argv, "--source");
  const target = parseFlag(argv, "--target");
  const parentPid = Number(parseFlag(argv, "--parent-pid") ?? "0");
  const expected = parseFlag(argv, "--expected-version") ?? "";
  const logPath = parseFlag(argv, "--log") ?? "";
  const skipVerify = argv.includes("--skip-verify");
  const startedAt = new Date().toISOString();
  const result: UpdateResult = {
    schema_version: 1,
    started_at: startedAt,
    finished_at: startedAt,
    targets: ["cli"],
    results: [
      {
        artifact: "cli",
        from: CLI_VERSION,
        to: expected,
        source: "cache",
        ok: false,
        action: "failed",
        error: "",
      },
    ],
    rollback: { performed: false, reason: "" },
    cli_restart: { exit_requested: false, exit_code: 0 },
  };
  if (source === undefined || target === undefined || expected === "") {
    result.results[0].error = "apply-update requires --source, --target, and --expected-version";
    writeUpdateResult(result);
    return 2;
  }
  waitForParent(parentPid);
  const dir = path.dirname(target);
  const temp = path.join(dir, `.gda-new-${process.pid}`);
  const old = `${target}.old-${Date.now()}`;
  try {
    fs.mkdirSync(dir, { recursive: true });
    fs.copyFileSync(source, temp);
    if (process.platform !== "win32") fs.chmodSync(temp, 0o755);
    if (fs.existsSync(target)) fs.renameSync(target, old);
    fs.renameSync(temp, target);
    if (!skipVerify) {
      const reported = runVersion(target);
      if (!reported.includes(expected)) {
        throw new CliError(ExitCode.Transport, `updated binary reports '${reported}', expected ${expected}`);
      }
    }
    if (fs.existsSync(old)) fs.rmSync(old, { force: true });
    result.results[0].ok = true;
    result.results[0].action = "updated";
    result.finished_at = new Date().toISOString();
    writeUpdateResult(result);
    if (logPath !== "") {
      fs.appendFileSync(logPath, `updated ${target} to ${expected}\n`, "utf8");
    }
    return 0;
  } catch (error) {
    if (fs.existsSync(old)) {
      fs.rmSync(target, { force: true });
      fs.renameSync(old, target);
      result.rollback = { performed: true, reason: String(error) };
    }
    result.results[0].error = String(error);
    result.results[0].action = "rolled_back";
    result.finished_at = new Date().toISOString();
    writeUpdateResult(result);
    return 1;
  }
}

function cacheCliBinary(): { binary: string; version: string; source: UpdateItemResult["source"] } | null {
  const manifest = selectLatest("cli");
  if (manifest === undefined || !verifyManifest(manifest)) return null;
  const temp = fs.mkdtempSync(path.join(cacheRoot(), "cli-update-"));
  const extracted = path.join(temp, "gda");
  try {
    if (manifest.asset.endsWith(".zip")) {
      const dir = path.join(temp, "unpacked");
      extractZip(cacheAssetPath(manifest), dir);
      const found = fs.readdirSync(dir, { recursive: true }).find((name) => String(name).endsWith("gda.exe") || String(name).endsWith("gda"));
      if (found === undefined) return null;
      fs.copyFileSync(path.join(dir, String(found)), extracted);
    } else {
      const result = Bun.spawnSync(["tar", "-xzf", cacheAssetPath(manifest), "-C", temp], { stdout: "pipe", stderr: "pipe" });
      if (result.exitCode !== 0) return null;
      if (!fs.existsSync(extracted)) return null;
    }
    if (process.platform !== "win32") fs.chmodSync(extracted, 0o755);
    return { binary: extracted, version: manifest.version, source: "cache" };
  } catch {
    return null;
  }
}

export async function updateCli(): Promise<number> {
  const executable = path.basename(process.execPath).toLowerCase();
  if (!executable.startsWith("gda")) {
    throw new CliError(
      ExitCode.Transport,
      "gda update requires the compiled gda binary; the current process is not a gda executable.",
    );
  }
  const cached = cacheCliBinary();
  if (cached === null) {
    throw new CliError(
      ExitCode.Transport,
      "no cached CLI binary is available for update.",
      "Run the installer/update check with network access first, or place the new binary in the CLI cache.",
    );
  }
  const child = Bun.spawn([
    cached.binary,
    "--apply-update",
    "--source",
    cached.binary,
    "--target",
    process.execPath,
    "--parent-pid",
    String(process.pid),
    "--expected-version",
    cached.version,
  ], {
    stdout: "inherit",
    stderr: "inherit",
  });
  child.unref();
  return ExitCode.Success;
}

export function updateCheck(): Record<string, unknown> {
  const cached = selectLatest("cli");
  return {
    schema_version: 1,
    cli_version: CLI_VERSION,
    platform: platformAsset().platform,
    cached_cli: cached?.version ?? null,
    cached_source: cached?.source ?? null,
    result_path: updateResultPath(),
  };
}
