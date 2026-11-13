import fs from "node:fs";
import os from "node:os";
import path from "node:path";

export const GDEXTENSION_NAME = "godot-autopilot.gdextension";

export const GDEXTENSION_CONTENT = [
  "[configuration]",
  'entry_symbol = "GDExtensionEntryPoint"',
  'compatibility_minimum = "4.7"',
  "",
  "[libraries]",
  'windows.debug.x86_64 = "res://addons/godot-autopilot/godot-autopilot.dll"',
  'windows.release.x86_64 = "res://addons/godot-autopilot/godot-autopilot.dll"',
  'linux.debug.x86_64 = "res://addons/godot-autopilot/libgodot-autopilot.so"',
  'linux.release.x86_64 = "res://addons/godot-autopilot/libgodot-autopilot.so"',
  'macos.debug.universal = "res://addons/godot-autopilot/libgodot-autopilot.dylib"',
  'macos.release.universal = "res://addons/godot-autopilot/libgodot-autopilot.dylib"',
  "",
].join("\n");

export function generateGdextension(targetAddonDir: string): void {
  fs.mkdirSync(targetAddonDir, { recursive: true });
  const content = GDEXTENSION_CONTENT.replace(/\n/g, os.EOL);
  fs.writeFileSync(path.join(targetAddonDir, GDEXTENSION_NAME), content, "utf8");
  console.log(`  ${GDEXTENSION_NAME}  (generated)`);
}

function stripQuotes(value: string): string {
  return value.replace(/^"+/, "").replace(/"+$/, "");
}

export type GdextensionLibrary = {
  key: string;
  path: string;
};

export function libraries(text: string): GdextensionLibrary[] {
  const entries: GdextensionLibrary[] = [];
  let inLibraries = false;
  for (const line of text.split(/\r?\n/)) {
    const stripped = line.trim();
    if (stripped.startsWith("[")) {
      inLibraries = stripped === "[libraries]";
      continue;
    }
    if (!inLibraries || !stripped.includes("=")) continue;
    const equals = stripped.indexOf("=");
    entries.push({
      key: stripped.slice(0, equals).trim(),
      path: stripQuotes(stripped.slice(equals + 1).trim()),
    });
  }
  return entries;
}

export function librariesEntries(text: string): string[] {
  return libraries(text).map((entry) => entry.path);
}
