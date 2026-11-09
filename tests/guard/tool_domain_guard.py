#!/usr/bin/env python3
"""工具域守卫：断言工具域规格文件与实现文件同名前缀对齐，且引用的 handler 均有定义。

判定规则：
  1. src/tools/<X>_tools.hpp 必须存在至少一个 src/tools/<X>_ops.cpp 或
     <X>_<子域>_ops.cpp（同一目录平铺，非递归）；
  2. <X>_tools.hpp 中出现的每个 <ns>::handle_xxx 必须在某个 src/tools/*.cpp 中
     以函数定义形式出现，防止声明存在而实现缺失的悬空引用。

元工具模块（autopilot_tools.hpp 不承载域工具）登记在 META_MODULES 中豁免规则 1。

无违规打印 tool-domain-guard OK 并退出 0，有违规逐条打印退出 1，
源目录读取失败退出 2。--self-test 以合成文本驱动断言函数。只用标准库。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TOOLS_DIR = ROOT / "src" / "tools"
META_MODULES = ("autopilot",)

DOMAIN_FILE = re.compile(r"^(.+)_tools\.hpp$")
CALLBACK_REF = re.compile(r"([a-z0-9_]+)::(handle_[a-z0-9_]+)")
CALLBACK_DEF = re.compile(
    r"(?m)^[A-Za-z_][A-Za-z0-9_:<> *&]*[ \t]+(handle_[a-z0-9_]+)[ \t]*\("
)


def ops_files_of(domain):
    exact = TOOLS_DIR / (domain + "_ops.cpp")
    pattern = re.compile("^" + re.escape(domain) + r"_[a-z0-9_]+\.cpp$")
    found = []
    if exact.is_file():
        found.append(exact)
    for path in sorted(TOOLS_DIR.glob("*.cpp")):
        if pattern.match(path.name) is not None and path not in found:
            found.append(path)
    return found


def callbacks_referenced(text):
    return {name for _, name in CALLBACK_REF.findall(text)}


def callbacks_defined(cpp_texts):
    defined = set()
    for text in cpp_texts:
        defined.update(CALLBACK_DEF.findall(text))
    return defined


def check(domain, tools_text, ops_present, defined):
    problems = []
    if not ops_present:
        problems.append(
            domain + "_tools.hpp: 缺少同前缀实现文件（应有 " + domain + "_ops.cpp 或 "
            + domain + "_*_ops.cpp）"
        )
    for name in sorted(callbacks_referenced(tools_text) - defined):
        problems.append(domain + "_tools.hpp: 引用的 " + name + " 在任何 src/tools/*.cpp 中都没有定义")
    return problems


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

    if "--self-test" in sys.argv:
        good = "render_ops::handle_ok"
        if check("render", good, True, {"handle_ok"}) != []:
            print("tool-domain-guard self-test FAIL: 正常样本被误报", file=sys.stderr)
            return 1
        if check("render", good, False, {"handle_ok"}) == []:
            print("tool-domain-guard self-test FAIL: 缺实现未被拦截", file=sys.stderr)
            return 1
        if check("render", "render_ops::handle_missing", True, set()) == []:
            print("tool-domain-guard self-test FAIL: 悬空引用未被拦截", file=sys.stderr)
            return 1
        print("tool-domain-guard self-test OK")
        return 0

    if not TOOLS_DIR.is_dir():
        print("tool-domain-guard: src/tools 不存在: " + str(TOOLS_DIR), file=sys.stderr)
        return 2

    cpp_texts = []
    try:
        for path in sorted(TOOLS_DIR.glob("*.cpp")):
            cpp_texts.append(path.read_text(encoding="utf-8", errors="replace"))
    except OSError as exc:
        print("tool-domain-guard: 读取实现文件失败: " + str(exc), file=sys.stderr)
        return 2

    defined = callbacks_defined(cpp_texts)
    problems = []
    domains = 0
    for path in sorted(TOOLS_DIR.glob("*_tools.hpp")):
        matched = DOMAIN_FILE.match(path.name)
        if matched is None:
            continue
        domain = matched.group(1)
        if domain in META_MODULES:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            print("tool-domain-guard: 读取 " + path.name + " 失败: " + str(exc), file=sys.stderr)
            return 2
        domains += 1
        problems.extend(check(domain, text, bool(ops_files_of(domain)), defined))

    if problems:
        for line in problems:
            print("tool-domain-guard: " + line, file=sys.stderr)
        return 1

    print("tool-domain-guard OK (domains: " + str(domains) + ")")
    return 0


if __name__ == "__main__":
    sys.exit(main())
