import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const GDA_HOME_DIR_NAME = ".godot-autopilot";

export function sep(): string {
  return process.platform === "win32" ? "\\" : "/";
}

export function joinPath(...parts: string[]): string {
  return parts.filter((p) => p !== "").join(sep());
}

export function isAbsolutePath(p: string): boolean {
  if (process.platform === "win32") return /^[A-Za-z]:[\\/]/.test(p) || p.startsWith("\\\\");
  return p.startsWith("/");
}

export function resolvePath(p: string): string {
  if (isAbsolutePath(p)) return p;
  return joinPath(process.cwd(), p);
}

export function readTextFile(path: string): string {
  return fs.readFileSync(path, "utf8");
}

export function gdaHome(): string {
  const override = process.env.GODOT_AUTOPILOT_HOME;
  if (override !== undefined && override.trim() !== "") return resolvePath(override.trim());
  return resolvePath(os.homedir());
}

export function gdaRoot(): string {
  return joinPath(gdaHome(), GDA_HOME_DIR_NAME);
}

export function projectPathKey(p: string): string {
  const resolved = path.resolve(p);
  return process.platform === "win32" ? resolved.toLowerCase() : resolved;
}

export function readProjectName(startDir: string): string | null {
  let dir = resolvePath(startDir);
  for (;;) {
    const candidate = joinPath(dir, "project.godot");
    try {
      if (fs.statSync(candidate).isFile()) {
        const text = fs.readFileSync(candidate, "utf8");
        const m = text.match(/^\s*config\/name\s*=\s*"([^"\r\n]*)"/m);
        if (m) return m[1];
        return null;
      }
    } catch {
    }
    const parent = dir.substring(0, Math.max(dir.lastIndexOf(sep()), dir.endsWith(":") ? dir.length : 0));
    if (!parent || parent === dir) return null;
    if (process.platform !== "win32" && parent === "") return null;
    dir = parent;
  }
}

export function userDataBase(): string | null {
  try {
    // 显式覆写：测试与非常规安装路径下绕过平台推断；未设或为空白时走下方原生分支
    const override = process.env.GODOT_AUTOPILOT_USERDATA;
    if (override !== undefined && override.trim() !== "") return override;
    if (process.platform === "win32") {
      const appData = process.env.APPDATA;
      if (!appData) return null;
      return joinPath(appData, "Godot", "app_userdata");
    }
    const home = process.env.HOME || process.env.USERPROFILE;
    if (!home) return null;
    if (process.platform === "darwin") {
      return joinPath(home, "Library", "Application Support", "Godot", "app_userdata");
    }
    return joinPath(home, ".local", "share", "godot", "app_userdata");
  } catch {
    return null;
  }
}
