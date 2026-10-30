#!/usr/bin/env python3
"""版本号全量一致性守卫：断言 VERSION、技能书与 README 的版本钉、CLI_VERSION 同值。

版本钉形如 github:jesspig/Godot-Autopilot#<ref>，分两类：
  - 版本钉：ref 以数字开头（形如 0.2.7），必须与根 VERSION 同值；
  - 非版本引用：ref 不是版本号（当前仅 develop 分支与 <VERSION> 占位符），
    只登记、不参与版本比对，但形态未知时报错。
扫描范围：skills/ 下全部文本文件 + 根 README*.md。扫描到的每一处版本钉
都必须与 VERSION 同值；同时「应含版本钉」的文件（REQUIRED_PIN_FILES）
必须至少命中一处版本钉，否则视为覆盖缺口。
全部一致打印 version-sync-guard OK 并退出 0，有不一致逐条打印退出 1，
文件读取失败 / 关键文件缺钉 / 未知引用形态退出 2。只用标准库。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
VERSION_FILE = ROOT / "VERSION"
CLI_VERSION_FILE = ROOT / "cli" / "src" / "version.ts"
SKILLS_DIR = ROOT / "skills"

REF_PATTERN = re.compile(r"github:jesspig/Godot-Autopilot#([^\s\"')]+)")
CLI_VERSION_PATTERN = re.compile(r'CLI_VERSION\s*=\s*"([^"]+)"')
VERSION_SHAPE = re.compile(r"^\d+\.\d+\.\d+$")
ALLOWED_NON_VERSION_REFS = frozenset({"develop", "<VERSION>"})

REQUIRED_PIN_FILES = (
    "skills/godot-autopilot/SKILL.md",
    "skills/godot-autopilot-tools/SKILL.md",
    "skills/godot-autopilot-tools/references/cli-access.md",
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
    if SKILLS_DIR.is_dir():
        for path in sorted(SKILLS_DIR.rglob("*")):
            if path.is_file():
                files[rel_of(path)] = path
    for path in sorted(ROOT.glob("README*.md")):
        files[rel_of(path)] = path
    for rel in REQUIRED_PIN_FILES:
        if rel not in files:
            files[rel] = ROOT / rel
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
        + " files, checked "
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
