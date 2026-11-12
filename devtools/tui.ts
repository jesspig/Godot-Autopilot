import fs from "node:fs";
import path from "node:path";
import { createInterface } from "node:readline/promises";

import { runBuild } from "./commands/build";
import { runClean } from "./commands/clean";
import { runDeploy } from "./commands/deploy";
import { runPackage } from "./commands/package";
import { runTests } from "./commands/test";
import { DEPLOY_TARGETS, DEMO_NAMES, PLATFORM_LIBS, addonDir, hostPlatform } from "./lib/config";

const ANSI = {
  reset: "\u001b[0m",
  bold: "\u001b[1m",
  dim: "\u001b[2m",
  red: "\u001b[31m",
  green: "\u001b[32m",
  yellow: "\u001b[33m",
  cyan: "\u001b[36m",
};

const MENU_ACTIONS = ["构建", "部署", "打包", "清理", "跑测试", "查看部署状态", "退出"] as const;
type MenuAction = (typeof MENU_ACTIONS)[number];

type Prompt = ReturnType<typeof createInterface>;

class Cancelled extends Error {}

function wideCodePoint(code: number): boolean {
  return (
    (code >= 0x1100 && code <= 0x115f) ||
    (code >= 0x2e80 && code <= 0xa4cf) ||
    (code >= 0xac00 && code <= 0xd7a3) ||
    (code >= 0xf900 && code <= 0xfaff) ||
    (code >= 0xfe30 && code <= 0xfe4f) ||
    (code >= 0xff00 && code <= 0xff60) ||
    (code >= 0xffe0 && code <= 0xffe6) ||
    (code >= 0x1f300 && code <= 0x1f64f) ||
    (code >= 0x1f900 && code <= 0x1f9ff) ||
    (code >= 0x20000 && code <= 0x3fffd)
  );
}

function displayWidth(text: string): number {
  let width = 0;
  for (const character of text) width += wideCodePoint(character.codePointAt(0) as number) ? 2 : 1;
  return width;
}

function pad(text: string, width: number): string {
  return text + " ".repeat(Math.max(0, width - displayWidth(text)));
}

function banner(text: string, tone: "info" | "ok" | "warn" | "error" = "info"): void {
  const color =
    tone === "ok" ? ANSI.green : tone === "warn" ? ANSI.yellow : tone === "error" ? ANSI.red : ANSI.cyan;
  const width = displayWidth(text) + 2;
  console.log(`${color}┌${"─".repeat(width)}┐`);
  console.log(`│ ${text} │`);
  console.log(`└${"─".repeat(width)}┘${ANSI.reset}`);
}

async function ask(rl: Prompt, message: string): Promise<string> {
  try {
    return (await rl.question(message)).trim();
  } catch {
    throw new Cancelled();
  }
}

async function select(rl: Prompt, title: string, choices: readonly string[]): Promise<string> {
  console.log(`\n${ANSI.bold}${title}${ANSI.reset}`);
  choices.forEach((choice, index) => {
    console.log(`  ${ANSI.cyan}${index + 1}${ANSI.reset}) ${choice}`);
  });
  for (;;) {
    const raw = await ask(rl, `选择 [1-${choices.length}]（默认 1）: `);
    if (raw === "") return choices[0];
    const index = Number(raw);
    if (Number.isInteger(index) && index >= 1 && index <= choices.length) return choices[index - 1];
    console.log(`${ANSI.yellow}无效输入：${raw}${ANSI.reset}`);
  }
}

async function multiSelect(
  rl: Prompt,
  title: string,
  choices: readonly string[],
  defaults: readonly boolean[],
): Promise<string[]> {
  console.log(`\n${ANSI.bold}${title}${ANSI.reset}`);
  choices.forEach((choice, index) => {
    console.log(`  ${ANSI.cyan}${index + 1}${ANSI.reset}) [${defaults[index] ? "x" : " "}] ${choice}`);
  });
  console.log(`${ANSI.dim}输入序号（空格分隔，回车采用默认；输入 none 表示全不选）${ANSI.reset}`);
  for (;;) {
    const raw = await ask(rl, "选择: ");
    if (raw === "") return choices.filter((_, index) => defaults[index]);
    if (raw.toLowerCase() === "none") return [];
    const picked = new Set<number>();
    let valid = true;
    for (const token of raw.split(/[\s,]+/).filter((item) => item !== "")) {
      const index = Number(token);
      if (!Number.isInteger(index) || index < 1 || index > choices.length) {
        console.log(`${ANSI.yellow}无效序号：${token}${ANSI.reset}`);
        valid = false;
        break;
      }
      picked.add(index - 1);
    }
    if (!valid) continue;
    if (picked.size === 0) return [];
    return choices.filter((_, index) => picked.has(index));
  }
}

async function confirm(rl: Prompt, title: string, defaultValue: boolean): Promise<boolean> {
  const hint = defaultValue ? "Y/n" : "y/N";
  const raw = (await ask(rl, `${title} [${hint}]: `)).toLowerCase();
  if (raw === "") return defaultValue;
  if (raw === "y" || raw === "yes") return true;
  if (raw === "n" || raw === "no") return false;
  console.log(`${ANSI.yellow}无效输入：${raw}${ANSI.reset}`);
  return confirm(rl, title, defaultValue);
}

async function text(rl: Prompt, title: string): Promise<string> {
  return ask(rl, `${title}: `);
}

function formatMtime(target: string): string {
  if (!fs.existsSync(target)) return "✗";
  const value = new Date(fs.statSync(target).mtime);
  const two = (input: number): string => String(input).padStart(2, "0");
  return `✓ ${two(value.getMonth() + 1)}-${two(value.getDate())} ${two(value.getHours())}:${two(value.getMinutes())}`;
}

function statusRows(): Array<[string, string, string]> {
  const libraryName = PLATFORM_LIBS[hostPlatform()] ?? null;
  return DEPLOY_TARGETS.map((projectDir): [string, string, string] => {
    const targetAddonDir = addonDir(projectDir);
    const gdextension = path.join(targetAddonDir, "godot-autopilot.gdextension");
    const library = libraryName === null ? null : path.join(targetAddonDir, libraryName);
    return [
      path.basename(projectDir),
      formatMtime(gdextension),
      library === null ? "—" : formatMtime(library),
    ];
  });
}

function renderStatusTable(): string {
  const rows = statusRows();
  const headers = ["工程", "gdextension", "平台库"] as const;
  const projectWidth = Math.max(displayWidth(headers[0]), ...rows.map((row) => displayWidth(row[0])));
  const gdextensionWidth = Math.max(displayWidth(headers[1]), ...rows.map((row) => displayWidth(row[1])));
  const libraryWidth = Math.max(displayWidth(headers[2]), ...rows.map((row) => displayWidth(row[2])));
  const border = (left: string, middle: string, right: string): string =>
    `${left}${"─".repeat(projectWidth + 2)}${middle}${"─".repeat(gdextensionWidth + 2)}${middle}${"─".repeat(libraryWidth + 2)}${right}`;
  const renderRow = (cells: readonly [string, string, string]): string =>
    `│ ${pad(cells[0], projectWidth)} │ ${pad(cells[1], gdextensionWidth)} │ ${pad(cells[2], libraryWidth)} │`;
  const lines = [
    border("┌", "┬", "┐"),
    renderRow([headers[0], headers[1], headers[2]]),
    border("├", "┼", "┤"),
  ];
  for (const row of rows) lines.push(renderRow(row));
  lines.push(border("└", "┴", "┘"));
  return lines.join("\n");
}

function printStatus(): void {
  console.log(`\n${ANSI.bold}插件部署状态${ANSI.reset}`);
  console.log(renderStatusTable());
}

async function actionBuild(rl: Prompt): Promise<void> {
  const presetName = await select(rl, "构建类型", ["Debug", "Release"]);
  banner(`构建 ${presetName}`);
  const code = await runBuild({ release: presetName === "Release", debug: presetName === "Debug" });
  if (code === 0) banner(`构建完成（${presetName}）`, "ok");
  else banner(`构建失败（退出码 ${code}）`, "error");
}

async function actionDeploy(rl: Prompt): Promise<void> {
  const presetName = await select(rl, "部署哪一版产物", ["Debug", "Release"]);
  const names = await multiSelect(
    rl,
    "部署目标（空格多选，回车部署全部）",
    DEMO_NAMES,
    DEMO_NAMES.map(() => true),
  );
  if (names.length === 0) {
    banner("未选择任何 demo，已取消", "warn");
    return;
  }
  if (presetName === "Release" && !(await confirm(rl, "Release 部署前会清理各 demo 的 .godot 与 addons，继续？", false))) {
    banner("已取消", "warn");
    return;
  }
  const code = await runDeploy({
    release: presetName === "Release",
    debug: presetName === "Debug",
    demos: names,
  });
  if (code === 0) {
    banner(`部署完成（${presetName}，${names.length} 个 demo）`, "ok");
    printStatus();
  } else {
    banner(`部署失败（退出码 ${code}）`, "error");
  }
}

async function actionPackage(rl: Prompt): Promise<void> {
  const useLibs = await confirm(rl, "是否从三平台库目录收集后再打包？", false);
  let libsDir: string | null = null;
  if (useLibs) {
    const raw = await text(rl, "三平台库目录");
    if (raw === "") {
      banner("未输入目录，已取消", "warn");
      return;
    }
    if (!fs.existsSync(raw) || !fs.statSync(raw).isDirectory()) {
      banner(`目录不存在：${raw}`, "error");
      return;
    }
    libsDir = raw;
  }
  const code = runPackage(libsDir);
  if (code === 0) banner("打包完成", "ok");
  else banner(`打包失败（退出码 ${code}）`, "error");
}

async function actionClean(rl: Prompt): Promise<void> {
  if (!(await confirm(rl, "清理全部 demo 的 .godot 与 addons？", false))) {
    banner("已取消", "warn");
    return;
  }
  const code = runClean();
  if (code === 0) banner("清理完成", "ok");
  else banner(`清理失败（退出码 ${code}）`, "error");
}

async function actionTest(rl: Prompt): Promise<void> {
  const scope = await select(rl, "测试范围", ["全量默认门", "单跑用例/守卫"]);
  const preset = await select(rl, "测试预设", ["debug", "release"]);
  let single: string | null = null;
  if (scope === "单跑用例/守卫") {
    const name = await text(rl, "用例名（如 migration_guard）");
    if (name === "") {
      banner("用例名为空，已取消", "warn");
      return;
    }
    single = name;
  }
  const code = runTests({ preset, single });
  if (code === 0) banner("测试通过", "ok");
  else banner(`测试失败（退出码 ${code}）`, "error");
}

export async function runTui(): Promise<number> {
  if (process.stdin.isTTY !== true) {
    console.error(
      `${ANSI.red}tui 需要交互式终端，请在终端中运行；无终端环境请改用 ` +
        `bun run gda-dev-tools build / package / clean 等命令。${ANSI.reset}`,
    );
    return 2;
  }
  const rl = createInterface({ input: process.stdin, output: process.stdout });
  rl.on("SIGINT", () => {
    rl.close();
  });
  try {
    banner("Godot-Autopilot 构建控制台");
    for (;;) {
      const action = (await select(rl, "选择操作", MENU_ACTIONS)) as MenuAction;
      if (action === "退出") return 0;
      if (action === "构建") await actionBuild(rl);
      else if (action === "部署") await actionDeploy(rl);
      else if (action === "打包") await actionPackage(rl);
      else if (action === "清理") await actionClean(rl);
      else if (action === "跑测试") await actionTest(rl);
      else printStatus();
    }
  } catch (error) {
    if (error instanceof Cancelled) {
      console.log(`\n${ANSI.yellow}已取消。${ANSI.reset}`);
      return 0;
    }
    throw error;
  } finally {
    rl.close();
  }
}
