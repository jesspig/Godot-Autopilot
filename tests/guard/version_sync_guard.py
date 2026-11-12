#!/usr/bin/env python3
"""版本号全量一致性守卫：断言 VERSION、README 的版本钉与 CLI_VERSION 同值。

版本钉形如 github:jesspig/Godot-Autopilot#<ref>，分两类：
  - 版本钉：ref 以数字开头（形如 0.2.7），必须与根 VERSION 同值；
  - 非版本引用：ref 不是版本号（当前仅 develop 分支与 <VERSION> 占位符），
    只登记、不参与版本比对，但形态未知时报错。
扫描范围：根 README*.md 的版本钉必须与 VERSION 同值；skills/ 与 skills-zh/
是交付给 MCP 客户端的技能书，禁止出现任何版本钉或 Git ref（版本由 CLI 与缓存
管理，技能书不钉版本）。全部一致打印 version-sync-guard OK 并退出 0，有不一致
逐条打印退出 1，文件读取失败 / README 缺钉 / 技能书出现版本钉 / 未知引用形态
退出 2。只用标准库。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
VERSION_FILE = ROOT / "VERSION"
CLI_VERSION_FILE = ROOT / "cli" / "src" / "version.ts"
SKILLS_DIRS = (ROOT / "skills", ROOT / "skills-zh")

REF_PATTERN = re.compile(r"github:jesspig/Godot-Autopilot#([^\s\"')]+)")
CLI_VERSION_PATTERN = re.compile(r'CLI_VERSION\s*=\s*"([^"]+)"')
VERSION_SHAPE = re.compile(r"^\d+\.\d+\.\d+$")
ALLOWED_NON_VERSION_REFS = frozenset({"develop", "<VERSION>"})

REQUIRED_PIN_FILES = (
    "README.md",
    "README.zh-CN.md",
    "README.zh-TW.md",
    "README.ja.md",
    "README.ko.md",
)


def rel_of(path):
    return path.relative_to(ROOT).as_posix()


def collect_scan_files():
    files = {}
    for path in sorted(ROOT.glob("README*.md")):
        files[rel_of(path)] = path
    for rel in REQUIRED_PIN_FILES:
        if rel not in files:
            files[rel] = ROOT / rel
    return files


def collect_skill_files():
    files = {}
    for skills_dir in SKILLS_DIRS:
        if not skills_dir.is_dir():
            continue
        for path in sorted(skills_dir.rglob("*")):
            if path.is_file():
                files[rel_of(path)] = path
    return files


def read_text(path, errors, required):
    try:
        data = path.read_bytes()
    except OSError as exc:
        errors.append(rel_of(path) + ": read error: " + str(exc))
        return None
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError as exc:
        if required:
            errors.append(rel_of(path) + ": decode error: " + str(exc))
        return None


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    try:
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    errors = []
    checked = []
    non_version_refs = 0
    version = ""
    version_text = read_text(VERSION_FILE, errors, True)
    if version_text is not None:
        version = version_text.strip()
        if version == "":
            errors.append("VERSION: no version value found")
        elif VERSION_SHAPE.match(version) is None:
            errors.append("VERSION: malformed version value: " + version)
        else:
            checked.append(("VERSION", 1, version, "VERSION"))
    cli_text = read_text(CLI_VERSION_FILE, errors, True)
    if cli_text is not None:
        match = CLI_VERSION_PATTERN.search(cli_text)
        if match is None:
            errors.append("cli/src/version.ts: no CLI_VERSION found")
        else:
            lineno = cli_text[: match.start()].count("\n") + 1
            checked.append((rel_of(CLI_VERSION_FILE), lineno, match.group(1), "CLI_VERSION"))
    scan_files = collect_scan_files()
    for rel, path in sorted(scan_files.items()):
        required = rel in REQUIRED_PIN_FILES
        text = read_text(path, errors, required)
        if text is None:
            continue
        version_pins = 0
        for lineno, line in enumerate(text.splitlines(), 1):
            for match in REF_PATTERN.finditer(line):
                ref = match.group(1)
                if ref[:1].isdigit():
                    version_pins += 1
                    checked.append((rel, lineno, ref, rel + " pin"))
                elif ref in ALLOWED_NON_VERSION_REFS:
                    non_version_refs += 1
                else:
                    errors.append(rel + ":" + str(lineno) + ": unknown ref kind: " + ref)
        if required and version_pins == 0:
            errors.append(rel + ": no version pin found")
    skill_files = collect_skill_files()
    for rel, path in sorted(skill_files.items()):
        text = read_text(path, errors, False)
        if text is None:
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            for match in REF_PATTERN.finditer(line):
                errors.append(rel + ":" + str(lineno) + ": skill books must not pin a Git ref: " + match.group(0))
    if errors:
        for item in errors:
            print(item, file=sys.stderr)
        return 2
    mismatches = [item for item in checked if item[2] != version]
    if mismatches:
        print("expected = " + version + " (VERSION)")
        for rel, lineno, value, label in mismatches:
            print(rel + ":" + str(lineno) + " = " + value + " (" + label + ")")
        return 1
    pin_count = len(checked) - 2
    print(
        "version-sync-guard scanned "
        + str(len(scan_files))
        + " README files and "
        + str(len(skill_files))
        + " skill files, checked "
        + str(pin_count)
        + " version pins ("
        + str(len(checked))
        + " values including VERSION/CLI_VERSION), "
        + str(non_version_refs)
        + " non-version refs"
    )
    print("version-sync-guard OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
