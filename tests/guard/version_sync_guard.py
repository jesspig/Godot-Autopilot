#!/usr/bin/env python3
"""版本号三处一致性守卫：断言 VERSION、技能书版本钉、CLI_VERSION 相同。

技能书里的 github:jesspig/Godot-Autopilot#<tag> 若与插件 VERSION 不一致，
用户 bunx github:...#tag 会解析失败。三处一致打印 version-sync-guard OK
并退出 0，任一不一致或提取失败退出 1，文件读取失败退出 2。
只用标准库。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
VERSION_FILE = ROOT / "VERSION"
SKILL_FILE = ROOT / "skills" / "godot-autopilot-tools" / "SKILL.md"
CLI_VERSION_FILE = ROOT / "cli" / "src" / "version.ts"
SKILL_PIN = re.compile(r"github:jesspig/Godot-Autopilot#([^\s\"')]+)")
CLI_VERSION = re.compile(r'CLI_VERSION\s*=\s*"([^"]+)"')


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    try:
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    read_errors = []
    values = []
    try:
        values.append(("VERSION", VERSION_FILE, VERSION_FILE.read_text(encoding="utf-8").strip()))
    except OSError as exc:
        read_errors.append("VERSION: " + str(VERSION_FILE) + ": read error: " + str(exc))
    try:
        skill_text = SKILL_FILE.read_text(encoding="utf-8")
        m = SKILL_PIN.search(skill_text)
        values.append(("SKILL.md pin", SKILL_FILE, m.group(1) if m else ""))
        if not m:
            read_errors.append("SKILL.md pin: " + str(SKILL_FILE) + ": no version pin found")
    except OSError as exc:
        read_errors.append("SKILL.md pin: " + str(SKILL_FILE) + ": read error: " + str(exc))
    try:
        cli_text = CLI_VERSION_FILE.read_text(encoding="utf-8")
        m = CLI_VERSION.search(cli_text)
        values.append(("CLI_VERSION", CLI_VERSION_FILE, m.group(1) if m else ""))
        if not m:
            read_errors.append("CLI_VERSION: " + str(CLI_VERSION_FILE) + ": no CLI_VERSION found")
    except OSError as exc:
        read_errors.append("CLI_VERSION: " + str(CLI_VERSION_FILE) + ": read error: " + str(exc))
    if read_errors:
        for item in read_errors:
            print(item, file=sys.stderr)
        return 2
    distinct = {v for _, _, v in values}
    if len(distinct) == 1:
        print("version-sync-guard OK")
        return 0
    for name, path, value in values:
        print(name + " = " + value + " (" + str(path) + ")")
    return 1


if __name__ == "__main__":
    sys.exit(main())
