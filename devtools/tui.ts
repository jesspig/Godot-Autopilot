import fs from "node:fs";
import path from "node:path";
import {
  cancel,
  confirm as promptConfirm,
  intro,
  isCancel,
  log,
  multiselect,
  note,
  outro,
  path as promptPath,
  select,
  text as promptText,
} from "@clack/prompts";

import { runBuild } from "./commands/build";
import { runClean } from "./commands/clean";
import { runDeploy } from "./commands/deploy";
import { runPackage } from "./commands/package";
import { runTests } from "./commands/test";
import { DEPLOY_TARGETS, DEMO_NAMES, PLATFORM_LIBS, addonDir, hostPlatform } from "./lib/config";

const MENU_ACTIONS = ["构建", "部署", "打包", "清理", "跑测试", "查看部署状态", "退出"] as const;
type MenuAction = (typeof MENU_ACTIONS)[number];

class Cancelled extends Error {}

function unwrap<T>(value: T): Exclude<T, symbol> {
  if (isCancel(value)) {
    cancel("已取消");
    throw new Cancelled();
  }
  return value as Exclude<T, symbol>;
}

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
  note(renderStatusTable(), "插件部署状态");
}

async function choosePreset(message: string): Promise<"Debug" | "Release"> {
  return unwrap(
    await select<"Debug" | "Release">({
      message,
      options: [
        { value: "Debug", label: "Debug", hint: "开发迭代" },
        { value: "Release", label: "Release", hint: "发布产物" },
      ],
    }),
  );
}

async function actionBuild(): Promise<void> {
  const presetName = await choosePreset("构建类型");
  log.step(`构建 ${presetName}`);
  const code = await runBuild({ release: presetName === "Release", debug: presetName === "Debug" });
  if (code === 0) note(`构建完成（${presetName}）`, "构建");
  else note(`构建失败（退出码 ${code}）`, "构建");
}

async function actionDeploy(): Promise<void> {
  const presetName = await choosePreset("部署哪一版产物");
  const names = unwrap(
    await multiselect<string>({
      message: "部署目标（空格多选，回车确认）",
      options: DEMO_NAMES.map((name) => ({ value: name, label: name })),
      initialValues: [...DEMO_NAMES],
      required: false,
    }),
  );
  if (names.length === 0) {
    log.warn("未选择任何 demo，已取消");
    return;
  }
  if (
    presetName === "Release" &&
    !unwrap(
      await promptConfirm({
        message: "Release 部署前会清理各 demo 的 .godot 与 addons，继续？",
        initialValue: false,
      }),
    )
  ) {
    log.warn("已取消");
    return;
  }
  const code = await runDeploy({
    release: presetName === "Release",
    debug: presetName === "Debug",
    demos: names,
  });
  if (code === 0) {
    note(`部署完成（${presetName}，${names.length} 个 demo）`, "部署");
    printStatus();
  } else {
    note(`部署失败（退出码 ${code}）`, "部署");
  }
}

async function actionPackage(): Promise<void> {
  const useLibs = unwrap(
    await promptConfirm({
      message: "是否从三平台库目录收集后再打包？",
      initialValue: false,
    }),
  );
  let libsDir: string | null = null;
  if (useLibs) {
    const raw = unwrap(
      await promptPath({
        message: "三平台库目录",
        directory: true,
        initialValue: ".",
      }),
    ).trim();
    if (raw === "") {
      log.warn("未输入目录，已取消");
      return;
    }
    if (!fs.existsSync(raw) || !fs.statSync(raw).isDirectory()) {
      log.error(`目录不存在：${raw}`);
      return;
    }
    libsDir = raw;
  }
  const code = runPackage(libsDir);
  if (code === 0) note("打包完成", "打包");
  else note(`打包失败（退出码 ${code}）`, "打包");
}

async function actionClean(): Promise<void> {
  if (
    !unwrap(
      await promptConfirm({
        message: "清理全部 demo 的 .godot 与 addons？",
        initialValue: false,
      }),
    )
  ) {
    log.warn("已取消");
    return;
  }
  const code = runClean();
  if (code === 0) note("清理完成", "清理");
  else note(`清理失败（退出码 ${code}）`, "清理");
}

async function actionTest(): Promise<void> {
  const scope = unwrap(
    await select<"all" | "single">({
      message: "测试范围",
      options: [
        { value: "all", label: "全量默认门", hint: "单元 + 边界 + 集成 + 守卫 + CLI" },
        { value: "single", label: "单跑用例/守卫", hint: "按名称过滤" },
      ],
    }),
  );
  const preset = unwrap(
    await select<"debug" | "release">({
      message: "测试预设",
      options: [
        { value: "debug", label: "debug" },
        { value: "release", label: "release" },
      ],
    }),
  );
  let single: string | null = null;
  if (scope === "single") {
    const name = unwrap(
      await promptText({
        message: "用例名（如 migration_guard）",
        placeholder: "migration_guard",
      }),
    ).trim();
    if (name === "") {
      log.warn("用例名为空，已取消");
      return;
    }
    single = name;
  }
  const code = runTests({ preset, single });
  if (code === 0) note("测试通过", "测试");
  else note(`测试失败（退出码 ${code}）`, "测试");
}

async function runAction(action: MenuAction): Promise<void> {
  if (action === "构建") await actionBuild();
  else if (action === "部署") await actionDeploy();
  else if (action === "打包") await actionPackage();
  else if (action === "清理") await actionClean();
  else if (action === "跑测试") await actionTest();
  else printStatus();
}

export async function runTui(): Promise<number> {
  if (process.stdin.isTTY !== true || process.stdout.isTTY !== true) {
    console.error("tui 需要交互式终端，请在终端中运行；无终端环境请改用 bun run gda-dev-tools <子命令>。");
    return 2;
  }
  intro("Godot-Autopilot 构建控制台");
  for (;;) {
    let action: MenuAction;
    try {
      action = unwrap(
        await select<MenuAction>({
          message: "选择操作",
          options: MENU_ACTIONS.map((value) => ({ value, label: value })),
        }),
      );
    } catch (error) {
      if (error instanceof Cancelled) {
        outro("已退出");
        return 0;
      }
      throw error;
    }
    if (action === "退出") {
      outro("已退出");
      return 0;
    }
    try {
      await runAction(action);
    } catch (error) {
      if (error instanceof Cancelled) {
        log.warn("已取消，返回主菜单");
        continue;
      }
      throw error;
    }
  }
}
