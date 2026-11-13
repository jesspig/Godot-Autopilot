import fs from "node:fs";
import type { Dirent } from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const SELF_ROOT = path.resolve(fileURLToPath(new URL("../..", import.meta.url)));

const SKIP_DIRS = new Set([
  ".git",
  ".handoff",
  ".worktrees",
  ".venv",
  "build",
  "dist",
  "node_modules",
  "out",
  "report",
  "third_party",
]);

const TEXT_EXTENSIONS = new Set([
  ".bat",
  ".cjs",
  ".cmake",
  ".cmd",
  ".csv",
  ".gdextension",
  ".gitignore",
  ".in",
  ".js",
  ".json",
  ".md",
  ".mjs",
  ".ps1",
  ".sh",
  ".template",
  ".toml",
  ".ts",
  ".tsx",
  ".txt",
  ".xml",
  ".yaml",
  ".yml",
]);

const TEXT_BASENAMES = new Set(["CMakeLists.txt", ".gitignore", ".gitattributes", ".env.template"]);

const CONFIG_FILES = new Set(["pyproject.toml", "uv.lock", ".python-version"]);

type Rule = {
  name: string;
  pattern: RegExp;
  message: string;
};

const RULES: Rule[] = [
  { name: "uv-run", pattern: /(?<![\w-])uv\s+run(?![\w-])/g, message: "uv run is retired; use bun run gda-dev-tools" },
  {
    name: "python-main",
    pattern: /\bpython(?:3(?:\.\d+)?)?\s+(?:\.\\)?(?:\.\/)?main\.py\b/gi,
    message: "python main.py is retired; use bun run gda-dev-tools",
  },
  {
    name: "find-package-python3",
    pattern: /find_package\s*\(\s*Python3\b/g,
    message: "CMake must not look up Python; the dev chain is Bun only",
  },
  {
    name: "gda-python-executable",
    pattern: /\bGDA_PYTHON_EXECUTABLE\b/g,
    message: "GDA_PYTHON_EXECUTABLE is retired; CMake must not register Python",
  },
  {
    name: "setup-python",
    pattern: /actions\/setup-python|PYTHONUTF8/g,
    message: "CI must not install or configure Python",
  },
];

type WhitelistEntry = {
  file: string;
  rule: string;
  contains?: string;
  reason: string;
};

const WHITELIST: WhitelistEntry[] = [
  {
    file: "docs/wiki/changelog/",
    rule: "uv-run",
    reason: "历史变更日志按原貌记录迁移前命令",
  },
  {
    file: "docs/wiki/changelog/",
    rule: "python-main",
    reason: "历史变更日志按原貌记录迁移前命令",
  },
  {
    file: "docs/wiki/changelog/",
    rule: "gda-python-executable",
    reason: "历史变更日志按原貌记录迁移前实现",
  },
  {
    file: "docs/wiki/changelog/",
    rule: "find-package-python3",
    reason: "历史变更日志按原貌记录迁移前实现",
  },
  {
    file: "docs/wiki/changelog/",
    rule: "setup-python",
    reason: "历史变更日志按原貌记录迁移前 CI",
  },
  {
    file: "docs/wiki/log.md",
    rule: "uv-run",
    reason: "运维日志追加式保留历史命令",
  },
  {
    file: "docs/wiki/log.md",
    rule: "python-main",
    reason: "运维日志追加式保留历史命令",
  },
  {
    file: "docs/wiki/plans/",
    rule: "uv-run",
    reason: "设计文档保留立项时的历史命令",
  },
  {
    file: "docs/wiki/plans/",
    rule: "python-main",
    reason: "设计文档保留立项时的历史命令",
  },
  {
    file: "docs/wiki/modules/support.md",
    rule: "uv-run",
    reason: "模块沿革段记录迁移前的调用链，非当前入口",
  },
  {
    file: "docs/wiki/modules/support.md",
    rule: "find-package-python3",
    reason: "模块沿革段记录迁移前的解释器探测，非当前实现",
  },
  {
    file: "docs/wiki/build.md",
    rule: "uv-run",
    contains: "2026-10-10",
    reason: "构建页历史审计行记录迁移前命令，非当前入口",
  },
  {
    file: "docs/wiki/build.md",
    rule: "python-main",
    contains: "2026-10-10",
    reason: "构建页历史审计行记录迁移前命令，非当前入口",
  },
  {
    file: "docs/wiki/build.md",
    rule: "setup-python",
    contains: "2026-11-13",
    reason: "构建页审计头记录 T-26 已移除的 CI Python 步骤",
  },
  {
    file: "docs/wiki/build.md",
    rule: "setup-python",
    contains: "2026-08-29",
    reason: "构建页历史审计行记录迁移前 CI 编码修复",
  },
  {
    file: "docs/wiki/build.md",
    rule: "find-package-python3",
    contains: "审计日期",
    reason: "构建页审计头记录迁移前的解释器探测历史",
  },
  {
    file: "AGENTS.md",
    rule: "*",
    contains: "no_python_guard",
    reason: "入口文档说明守卫的检测项，非实际回退命令",
  },
  {
    file: "docs/wiki/tests.md",
    rule: "*",
    contains: "no_python_guard",
    reason: "测试页说明守卫的检测项，非实际回退命令",
  },
  {
    file: "tests/README.md",
    rule: "*",
    contains: "no_python_guard",
    reason: "测试说明说明守卫的检测项，非实际回退命令",
  },
  {
    file: "tests/guard/no_python_guard.ts",
    rule: "uv-run",
    reason: "守卫源码包含被检测的命令字面量与自测样本",
  },
  {
    file: "tests/guard/no_python_guard.ts",
    rule: "python-main",
    reason: "守卫源码包含被检测的命令字面量与自测样本",
  },
  {
    file: "tests/guard/no_python_guard.ts",
    rule: "find-package-python3",
    reason: "守卫源码包含被检测的 CMake 模式与自测样本",
  },
  {
    file: "tests/guard/no_python_guard.ts",
    rule: "gda-python-executable",
    reason: "守卫源码包含被检测的变量名与自测样本",
  },
  {
    file: "tests/guard/no_python_guard.ts",
    rule: "setup-python",
    reason: "守卫源码包含被检测的 CI 模式与自测样本",
  },
];

type Violation = {
  file: string;
  line: number;
  rule: string;
  message: string;
  text: string;
};

function toPosix(value: string): string {
  return value.split(path.sep).join("/");
}

function isTextFile(name: string): boolean {
  if (TEXT_BASENAMES.has(name)) return true;
  return TEXT_EXTENSIONS.has(path.extname(name).toLowerCase());
}

function whitelistReason(relative: string, rule: string, line: string): string | null {
  for (const entry of WHITELIST) {
    const matches = entry.file.endsWith("/") ? relative.startsWith(entry.file) : relative === entry.file;
    if (!matches || (entry.rule !== "*" && entry.rule !== rule)) continue;
    if (entry.contains !== undefined && !line.includes(entry.contains)) continue;
    return entry.reason;
  }
  return null;
}

function scanLine(relative: string, lineNumber: number, line: string): Violation[] {
  const found: Violation[] = [];
  for (const rule of RULES) {
    rule.pattern.lastIndex = 0;
    if (!rule.pattern.test(line)) continue;
    if (whitelistReason(relative, rule.name, line) !== null) continue;
    found.push({ file: relative, line: lineNumber, rule: rule.name, message: rule.message, text: line.trim() });
  }
  return found;
}

function scanText(relative: string, text: string): Violation[] {
  const found: Violation[] = [];
  const lines = text.split(/\r?\n/);
  for (let index = 0; index < lines.length; index += 1) {
    found.push(...scanLine(relative, index + 1, lines[index]));
  }
  return found;
}

function collectFiles(root: string): Array<{ absolute: string; relative: string }> {
  const files: Array<{ absolute: string; relative: string }> = [];
  const stack: string[] = [root];
  while (stack.length > 0) {
    const current = stack.pop() as string;
    let entries: Dirent[];
    try {
      entries = fs.readdirSync(current, { withFileTypes: true });
    } catch {
      continue;
    }
    for (const entry of entries) {
      const absolute = path.join(current, entry.name);
      const relative = toPosix(path.relative(root, absolute));
      if (entry.isDirectory()) {
        if (SKIP_DIRS.has(entry.name)) continue;
        stack.push(absolute);
      } else if (entry.isFile()) {
        files.push({ absolute, relative });
      }
    }
  }
  files.sort((left, right) => (left.relative < right.relative ? -1 : left.relative > right.relative ? 1 : 0));
  return files;
}

function scanRoot(root: string): { violations: Violation[]; errors: string[] } {
  const violations: Violation[] = [];
  const errors: string[] = [];
  for (const { absolute, relative } of collectFiles(root)) {
    const baseName = path.basename(absolute);
    if (baseName.endsWith(".py")) {
      violations.push({
        file: relative,
        line: 0,
        rule: "python-file",
        message: "Python sources are retired; the dev chain is Bun only",
        text: baseName,
      });
      continue;
    }
    if (CONFIG_FILES.has(baseName)) {
      violations.push({
        file: relative,
        line: 0,
        rule: "python-config",
        message: "Python tooling config is retired",
        text: baseName,
      });
      continue;
    }
    if (!isTextFile(baseName)) continue;
    let text: string;
    try {
      text = fs.readFileSync(absolute, "utf8");
    } catch (error) {
      errors.push(`${relative}: read error: ${error instanceof Error ? error.message : String(error)}`);
      continue;
    }
    violations.push(...scanText(relative, text));
  }
  return { violations, errors };
}

function report(root: string, violations: Violation[], errors: string[]): number {
  if (errors.length > 0) {
    for (const item of errors) console.error(item);
    return 2;
  }
  if (violations.length > 0) {
    console.log(`no-python-guard: ${violations.length} violation(s) under ${root}`);
    for (const item of violations) {
      const location = item.line === 0 ? item.file : `${item.file}:${item.line}`;
      console.log(`${location}: [${item.rule}] ${item.text}`);
    }
    return 1;
  }
  console.log("no-python-guard OK");
  return 0;
}

function writeSample(root: string, relative: string, content: string): void {
  const target = path.join(root, relative);
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.writeFileSync(target, content, "utf8");
}

function runSelfTests(): number {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "gda-no-python-"));
  try {
    writeSample(root, "main.py", "print('legacy')\n");
    writeSample(root, "pyproject.toml", "[project]\n");
    writeSample(root, "docs/guide.md", "run uv run main.py build\n");
    writeSample(root, "ci.yml", "run: python main.py test\n");
    writeSample(root, "CMakeLists.txt", "find_package(Python3 REQUIRED)\n");
    writeSample(root, "cmake/legacy.cmake", "set(GDA_PYTHON_EXECUTABLE python)\n");
    writeSample(root, "docs/wiki/changelog/2026-09-26-log.md", "uv run main.py build\n");
    writeSample(root, "src/clean.cpp", "int main() { return 0; }\n");
    const { violations, errors } = scanRoot(root);
    const rules = new Set(violations.map((item) => item.rule));
    const expected = [
      "python-file",
      "python-config",
      "uv-run",
      "python-main",
      "find-package-python3",
      "gda-python-executable",
    ];
    for (const rule of expected) {
      if (!rules.has(rule)) {
        console.error(`self-test FAIL: rule ${rule} was not reported`);
        return 1;
      }
    }
    if (violations.some((item) => item.file.startsWith("docs/wiki/changelog/"))) {
      console.error("self-test FAIL: whitelisted changelog sample was reported");
      return 1;
    }
    if (violations.some((item) => item.file === "src/clean.cpp")) {
      console.error("self-test FAIL: clean C++ sample was reported");
      return 1;
    }
    if (errors.length > 0) {
      console.error(`self-test FAIL: ${errors.join("; ")}`);
      return 1;
    }
    console.log("no-python-guard self-test OK");
    return 0;
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
}

function main(): number {
  const args = process.argv.slice(2);
  if (args.length === 1 && args[0] === "--self-test") return runSelfTests();
  let root = SELF_ROOT;
  if (args.length > 0) {
    if (args.length !== 2 || args[0] !== "--root") {
      console.error("usage: no_python_guard.ts [--root <dir>] [--self-test]");
      return 2;
    }
    root = path.resolve(args[1]);
  }
  if (!fs.existsSync(root) || !fs.statSync(root).isDirectory()) {
    console.error(`no-python-guard: root is not a directory: ${root}`);
    return 2;
  }
  const { violations, errors } = scanRoot(root);
  return report(root, violations, errors);
}

process.exitCode = main();
