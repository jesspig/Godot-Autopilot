import fs from "node:fs";
import path from "node:path";

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

export const DEMO_NAMES: readonly string[] = [
  "2d_dodge_the_creeps",
  "3d_squash_the_creeps",
  "gui_control_gallery",
  "viewport_gui_in_3d",
  "networking_multiplayer_pong",
  "pink_man_adventure",
];

export const DEMO_DIRS: readonly string[] = DEMO_NAMES.map((name) =>
  path.join(PROJECT_ROOT, "examples", name),
);

export const DEPLOY_TARGETS: readonly string[] = DEMO_DIRS;

export function addonDir(demoDir: string): string {
  return path.join(demoDir, "addons", "godot-autopilot");
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

export function resolveDemos(names: string[] | null): string[] {
  if (names === null) return [...DEMO_DIRS];
  const known = new Map(DEMO_NAMES.map((name, index) => [name, DEMO_DIRS[index]]));
  const unknown = names.filter((name) => !known.has(name));
  if (unknown.length > 0) {
    throw new Error(
      `未知的 demo：${unknown.join(", ")}（可选：${[...known.keys()].join(", ")}）`,
    );
  }
  return names.map((name) => known.get(name) as string);
}
