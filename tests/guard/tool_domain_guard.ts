import fs from "node:fs";
import path from "node:path";

import { ROOT, escapeRegExp, isDirectory, walkFiles } from "./guard_support";

const TOOLS_DIR = path.join(ROOT, "src", "tools");
const META_MODULES = new Set(["autopilot"]);
const DOMAIN_FILE = /^(.+)_tools\.hpp$/;
const CALLBACK_REF = /([a-z0-9_]+)::(handle_[a-z0-9_]+)/g;
const CALLBACK_DEF = /^[A-Za-z_][A-Za-z0-9_:<> *&]*[ \t]+(handle_[a-z0-9_]+)[ \t]*\(/gm;

function opsFilesOf(domain: string, cppFiles: string[]): string[] {
  const exactName = `${domain}_ops.cpp`;
  const pattern = new RegExp(`^${escapeRegExp(domain)}_[a-z0-9_]+\\.cpp$`);
  const found: string[] = [];
  for (const file of cppFiles) {
    const name = path.basename(file);
    if (name === exactName || pattern.test(name)) found.push(file);
  }
  return found;
}

function callbacksReferenced(text: string): Set<string> {
  const names = new Set<string>();
  for (const match of text.matchAll(CALLBACK_REF)) names.add(match[2]);
  return names;
}

function callbacksDefined(cppTexts: string[]): Set<string> {
  const defined = new Set<string>();
  for (const text of cppTexts) {
    for (const match of text.matchAll(CALLBACK_DEF)) defined.add(match[1]);
  }
  return defined;
}

function check(
  domain: string,
  toolsText: string,
  opsPresent: boolean,
  defined: Set<string>,
): string[] {
  const problems: string[] = [];
  if (!opsPresent) {
    problems.push(
      `${domain}_tools.hpp: 缺少同前缀实现文件（应有 ${domain}_ops.cpp 或 ${domain}_*_ops.cpp）`,
    );
  }
  for (const name of [...callbacksReferenced(toolsText)].filter((name) => !defined.has(name)).sort()) {
    problems.push(`${domain}_tools.hpp: 引用的 ${name} 在任何 src/tools/**/*.cpp 中都没有定义`);
  }
  return problems;
}

function runSelfTests(): number {
  const good = "render_ops::handle_ok";
  if (check("render", good, true, new Set(["handle_ok"])).length !== 0) {
    console.error("tool-domain-guard self-test FAIL: 正常样本被误报");
    return 1;
  }
  if (check("render", good, false, new Set(["handle_ok"])).length === 0) {
    console.error("tool-domain-guard self-test FAIL: 缺实现未被拦截");
    return 1;
  }
  if (check("render", "render_ops::handle_missing", true, new Set<string>()).length === 0) {
    console.error("tool-domain-guard self-test FAIL: 悬空引用未被拦截");
    return 1;
  }
  console.log("tool-domain-guard self-test OK");
  return 0;
}

function main(): number {
  if (process.argv.slice(2).includes("--self-test")) return runSelfTests();

  if (!isDirectory(TOOLS_DIR)) {
    console.error(`tool-domain-guard: src/tools 不存在: ${TOOLS_DIR}`);
    return 2;
  }

  const cppFiles = walkFiles(TOOLS_DIR).filter((file) => file.endsWith(".cpp"));
  const cppTexts: string[] = [];
  try {
    for (const file of cppFiles) cppTexts.push(fs.readFileSync(file, "utf8"));
  } catch (error) {
    console.error(`tool-domain-guard: 读取实现文件失败: ${(error as Error).message}`);
    return 2;
  }

  const defined = callbacksDefined(cppTexts);
  const problems: string[] = [];
  let domains = 0;
  for (const file of walkFiles(TOOLS_DIR).filter((file) => path.basename(file).endsWith("_tools.hpp"))) {
    const match = DOMAIN_FILE.exec(path.basename(file));
    if (match === null) continue;
    const domain = match[1];
    if (META_MODULES.has(domain)) continue;
    let text: string;
    try {
      text = fs.readFileSync(file, "utf8");
    } catch (error) {
      console.error(`tool-domain-guard: 读取 ${path.basename(file)} 失败: ${(error as Error).message}`);
      return 2;
    }
    domains += 1;
    problems.push(...check(domain, text, opsFilesOf(domain, cppFiles).length > 0, defined));
  }

  if (problems.length > 0) {
    for (const line of problems) console.error(`tool-domain-guard: ${line}`);
    return 1;
  }

  console.log(`tool-domain-guard OK (domains: ${domains})`);
  return 0;
}

process.exitCode = main();
