import fs from "node:fs";

import { BRIDGE_SOURCES, PLATFORM_LIBS } from "../lib/config";
import { errorMessage } from "../lib/cmake";
import { GDEXTENSION_NAME, librariesEntries } from "../lib/gdextension";
import { readZip, validateZipLayout, type ZipArchiveEntry } from "../lib/zip";

const ADDON_SUBPATH = "addons/godot-autopilot";
const RES_PREFIX = "res://";

function collectProblems(entries: ZipArchiveEntry[]): string[] {
  const problems: string[] = [];
  const files = new Map<string, Buffer>();
  let addonDirPresent = false;
  for (const entry of entries) {
    if (entry.name === `${ADDON_SUBPATH}/` || entry.name.startsWith(`${ADDON_SUBPATH}/`)) {
      addonDirPresent = true;
    }
    if (!entry.name.endsWith("/")) files.set(entry.name, entry.data);
  }
  if (!addonDirPresent) return [`zip 内缺少插件目录 ${ADDON_SUBPATH}`];

  const addonFile = (name: string): string => `${ADDON_SUBPATH}/${name}`;

  for (const libName of Object.values(PLATFORM_LIBS)) {
    if (!files.has(addonFile(libName))) problems.push(`缺少平台库 ${libName}`);
  }

  for (const name of BRIDGE_SOURCES) {
    if (!files.has(addonFile(name))) problems.push(`缺少 C# 桥源码 ${name}`);
  }

  const gdextension = files.get(addonFile(GDEXTENSION_NAME));
  if (gdextension === undefined) {
    problems.push(`缺少描述文件 ${GDEXTENSION_NAME}`);
  } else {
    const referenced = new Set<string>();
    const missingTargets = new Set<string>();
    for (const resPath of librariesEntries(gdextension.toString("utf8"))) {
      referenced.add(resPath.split("/").pop() ?? "");
      if (!resPath.startsWith(RES_PREFIX)) {
        problems.push(`gdextension 条目不是 res:// 路径：${resPath}`);
        continue;
      }
      if (!files.has(resPath.slice(RES_PREFIX.length))) missingTargets.add(resPath);
    }
    for (const resPath of [...missingTargets].sort()) {
      problems.push(`gdextension 条目 ${resPath} 在包内不存在`);
    }
    for (const libName of Object.values(PLATFORM_LIBS)) {
      if (files.has(addonFile(libName)) && !referenced.has(libName)) {
        problems.push(`平台库 ${libName} 未被 gdextension 引用`);
      }
    }
  }

  for (const name of files.keys()) {
    if (name.endsWith(".pdb")) problems.push(`包内含调试符号 ${name}（不随包分发）`);
  }

  return problems;
}

export function runPackageCheck(zipPath: string): number {
  if (!fs.existsSync(zipPath) || !fs.statSync(zipPath).isFile()) {
    console.log(`[ERROR] zip not found: ${zipPath}`);
    return 1;
  }
  const layoutProblem = validateZipLayout(zipPath);
  if (layoutProblem !== null) {
    console.log(`[ERROR] invalid zip layout: ${layoutProblem}`);
    return 1;
  }
  let entries: ZipArchiveEntry[];
  try {
    entries = readZip(zipPath);
  } catch (error) {
    console.log(`[ERROR] cannot read zip ${zipPath}: ${errorMessage(error)}`);
    return 1;
  }
  const problems = collectProblems(entries);
  if (problems.length > 0) {
    for (const problem of problems) console.log(`[ERROR] ${problem}`);
    console.log(`[FAIL] ${problems.length} problem(s) in ${zipPath}`);
    return 1;
  }
  console.log(`package-integrity OK: ${zipPath}`);
  return 0;
}
