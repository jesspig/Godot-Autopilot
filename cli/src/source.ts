import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import { CliError, ExitCode } from "./errors";

export type AssetSourceName = "github";

export interface AssetSource {
  name: AssetSourceName;
  base: string;
}

export interface DownloadedAsset {
  path: string;
  source: AssetSourceName;
}

const DEFAULT_GITHUB_BASE = "https://github.com/jesspig/Godot-Autopilot/releases";

export function assetSources(): AssetSource[] {
  return [{ name: "github", base: process.env.GDA_GITHUB_BASE ?? DEFAULT_GITHUB_BASE }];
}

export function assetUrl(base: string, version: string, asset: string): string {
  return `${base.replace(/\/+$/, "")}/download/v${version}/${asset}`;
}

function sha256(filePath: string): string {
  return createHash("sha256").update(fs.readFileSync(filePath)).digest("hex");
}

function expectedDigest(sums: string, asset: string): string | null {
  for (const line of sums.split(/\r?\n/)) {
    const parts = line.trim().split(/\s+/);
    if (parts.length >= 2 && parts[0] === asset) return parts[1].toLowerCase();
    if (parts.length >= 2 && parts[1] === asset) return parts[0].toLowerCase();
  }
  return null;
}

async function fetchText(url: string, timeoutMs: number): Promise<string> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, { signal: controller.signal });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.text();
  } finally {
    clearTimeout(timer);
  }
}

async function downloadFile(url: string, target: string, timeoutMs: number): Promise<void> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, { signal: controller.signal });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    await Bun.write(target, await response.arrayBuffer());
  } finally {
    clearTimeout(timer);
  }
}

export async function downloadAsset(
  asset: string,
  version: string,
  destDir: string,
  timeoutMs = 60_000,
): Promise<DownloadedAsset> {
  const failures: string[] = [];
  for (const source of assetSources()) {
    const target = path.join(destDir, asset);
    try {
      await downloadFile(assetUrl(source.base, version, asset), target, timeoutMs);
      const sumsText = await fetchText(assetUrl(source.base, version, "SHA256SUMS"), timeoutMs);
      const expected = expectedDigest(sumsText, asset);
      if (expected === null) {
        throw new Error(`${asset} is not listed in SHA256SUMS`);
      }
      if (sha256(target) !== expected) {
        throw new Error(`checksum mismatch for ${asset}`);
      }
      return { path: target, source: source.name };
    } catch (error) {
      fs.rmSync(target, { force: true });
      failures.push(`${source.name}: ${error instanceof Error ? error.message : String(error)}`);
    }
  }
  throw new CliError(
    ExitCode.Transport,
    `no source could provide ${asset} v${version}: ${failures.join("; ")}`,
    "Pass --archive with a local release asset, or check the network and release tag.",
  );
}
