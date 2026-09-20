#!/usr/bin/env python3
"""C++ 注释残留守卫：递归扫描 src/ 与 tests/ 下的 *.cpp/*.hpp。

按 C++ 词法剥离字符串字面量（含 R"TAG(...)TAG" raw string，TAG 可为空）与
字符字面量（含转义）后再判定；报告所有残留注释：整行/行内 //（// 后可选
空白接 namespace 的结尾标记除外）与 /* ... */（含单行）。
无违规打印 comment-guard OK 并退出 0，有违规退出 1，文件无法解码退出 2。
只用标准库。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCAN_DIRS = (ROOT / "src", ROOT / "tests")
RAW_START = re.compile(r"(?:u8|u|U|L)?R\"([^()\s\\]{0,16})\(")


def _snippet(lines, lineno):
    if 1 <= lineno <= len(lines):
        return lines[lineno - 1].rstrip("\r").strip()[:120]
    return ""


def _is_namespace_mark(body):
    s = body.strip()
    if not s.startswith("namespace"):
        return False
    rest = s[len("namespace"):]
    return rest == "" or rest[:1] in (" ", "\t")


def scan_text(text):
    lines = text.split("\n")
    out = []
    n = len(text)
    i = 0
    lineno = 1
    while i < n:
        c = text[i]
        if c == "\n":
            lineno += 1
            i += 1
            continue
        if c == "/":
            nxt = text[i + 1] if i + 1 < n else ""
            if nxt == "/":
                end = text.find("\n", i + 2)
                if end == -1:
                    end = n
                if not _is_namespace_mark(text[i + 2:end]):
                    out.append((lineno, _snippet(lines, lineno)))
                i = end
                continue
            if nxt == "*":
                out.append((lineno, _snippet(lines, lineno)))
                end = text.find("*/", i + 2)
                if end == -1:
                    break
                lineno += text.count("\n", i, end + 2)
                i = end + 2
                continue
        if c in "RULu":
            m = RAW_START.match(text, i)
            if m:
                closer = ")" + m.group(1) + '"'
                end = text.find(closer, m.end())
                if end == -1:
                    break
                lineno += text.count("\n", i, end + len(closer))
                i = end + len(closer)
                continue
        if c == '"':
            i += 1
            while i < n:
                ch = text[i]
                if ch == "\\":
                    i += 2
                    continue
                if ch == '"':
                    i += 1
                    break
                if ch == "\n":
                    lineno += 1
                    i += 1
                    break
                i += 1
            continue
        if c == "'":
            i += 1
            while i < n:
                ch = text[i]
                if ch == "\\":
                    i += 2
                    continue
                if ch == "'":
                    i += 1
                    break
                if ch == "\n":
                    lineno += 1
                    i += 1
                    break
                i += 1
            continue
        i += 1
    return out


def collect_files():
    found = set()
    for d in SCAN_DIRS:
        if not d.is_dir():
            continue
        for p in list(d.rglob("*.cpp")) + list(d.rglob("*.hpp")):
            found.add(p)
    return sorted(found, key=lambda p: p.relative_to(ROOT).as_posix())


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    try:
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    decode_errors = []
    violations = []
    for path in collect_files():
        rel = path.relative_to(ROOT).as_posix()
        try:
            data = path.read_bytes()
        except OSError as exc:
            decode_errors.append(rel + ": read error: " + str(exc))
            continue
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError as exc:
            decode_errors.append(rel + ": decode error: " + str(exc))
            continue
        for lineno, snippet in scan_text(text):
            violations.append(rel + ":" + str(lineno) + ": " + snippet)
    if decode_errors:
        for item in decode_errors:
            print(item, file=sys.stderr)
        return 2
    if violations:
        for item in violations:
            print(item)
        return 1
    print("comment-guard OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
