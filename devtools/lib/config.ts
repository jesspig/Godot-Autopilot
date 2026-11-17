import fs from "node:fs";
import path from "node:path";

import { discoverProjectsIn } from "./jam_projects";

export const PROJECT_ROOT = path.resolve(import.meta.dir, "..", "..");
export const BUILD_DIR = path.join(PROJECT_ROOT, "build");

const MULTI_CONFIG_PREFIXES = ["Visual Studio", "Xcode", "Ninja Multi-Config"];

export function resolvePreset(preset: string): string {
  return Bun.which("ninja") === null ? `${preset}-portable` : preset;
}

export function cachedGenerator(target: string): string | null {
  const cache = path.join(target, "CMakeCache.txt");
  if (!fs.existsSync(cache)) return null;
  const text = fs.readFileSync(cache, "utf8");
  for (const line of text.split(/\r?\n/)) {
    const colon = line.indexOf(":");
    if (colon < 0) continue;
    if (line.slice(0, colon).trim() !== "CMAKE_GENERATOR") continue;
    const equals = line.indexOf("=", colon);
    return equals < 0 ? "" : line.slice(equals + 1).trim();
  }
  return null;
}

export function artifactDir(preset: string): string {
  const base = path.join(BUILD_DIR, preset);
  const generator = cachedGenerator(base);
  const multiConfig =
    generator !== null && MULTI_CONFIG_PREFIXES.some((prefix) => generator.startsWith(prefix));
  if (!multiConfig) return base;
  return path.join(base, preset === "debug" ? "Debug" : "Release");
}

export const EXAMPLES_DIR = path.join(PROJECT_ROOT, "examples");

function isGodotProject(dir: string): boolean {
  return fs.existsSync(path.join(dir, "project.godot"));
}

export function discoverExampleProjects(): string[] {
  return discoverProjectsIn(EXAMPLES_DIR);
}

export function resolveProjects(paths: readonly string[]): string[] {
  const targets =
    paths.length > 0
      ? paths.map((input) => path.resolve(process.cwd(), input))
      : discoverExampleProjects();
  if (targets.length === 0) {
    throw new Error(
      "no Godot project found; pass --project <dir> (repeatable) or place projects under examples/",
    );
  }
  const invalid = targets.filter((dir) => !isGodotProject(dir));
  if (invalid.length > 0) {
    throw new Error(`not a Godot project (missing project.godot): ${invalid.join(", ")}`);
  }
  return [...new Set(targets)];
}

export function addonDir(projectDir: string): string {
  return path.join(projectDir, "addons", "godot-autopilot");
}

export const PLATFORM_LIBS: Readonly<Record<string, string>> = {
  windows: "godot-autopilot.dll",
  linux: "libgodot-autopilot.so",
  darwin: "libgodot-autopilot.dylib",
};

export const PLATFORM_PDB = "godot-autopilot.pdb";

export const BRIDGE_DIR = path.join(PROJECT_ROOT, "csharp_bridge");

export const BRIDGE_SOURCES: readonly string[] = [
  "BridgeCall.cs",
  "BridgeConvert.cs",
  "BridgeEvents.cs",
  "BridgeGetSet.cs",
  "GdaCSharpBridge.cs",
  "GdaCSharpRuntime.cs",
];

export const ADDON_VERSION = fs.readFileSync(path.join(PROJECT_ROOT, "VERSION"), "utf8").trim();

export function hostPlatform(): string {
  if (process.platform === "win32") return "windows";
  if (process.platform === "linux") return "linux";
  if (process.platform === "darwin") return "darwin";
  return process.platform;
}

export function hostPlatformLib(): string | null {
  return PLATFORM_LIBS[hostPlatform()] ?? null;
}

