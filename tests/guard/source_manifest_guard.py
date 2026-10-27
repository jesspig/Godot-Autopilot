#!/usr/bin/env python3
"""L1 源清单守卫：断言插件源清单并集 == src/ 全部 .cpp，且 L1 清单引用的 src/ 源 ⊆ 插件并集。

清单解析规则（cmake/sources/*.cmake）：
  - 先剥注释：每行删除首个 # 起的尾部；
  - 提取全部 set( ... ) 与 add_executable( ... ) 的括号体（容忍跨行），按空白切 token；
  - `${CMAKE_SOURCE_DIR}/` 前缀替换为仓库根相对路径；含其它 `${...}` 变量引用的
    token 跳过（如 ${GDA_TOOLS_OPS_SOURCES}——两侧同源 glob，展开必然一致）。
插件并集 = gda_core/api_core/api_contract/api_storage/api_transport/api_cli/
  api_instance_guard 七份清单的 src/ 条目 ∪ src/tools/*_ops.cpp（非递归 glob，
  与 tests/CMakeLists.txt 的 file(GLOB GDA_TOOLS_OPS_SOURCES) 同模式）的实际文件；
  断言其 == src/ 下全部 .cpp（防漏登插件产物）。
L1 并集 = gda_unit_sources/api_unit_contract/api_unit_storage/api_unit_transport/
  api_unit_cli/api_unit_docs/api_unit_instance_guard + api_probe 八份清单的 src/ 条目
  （tests/ 下的用例与探针文件不属于插件目标，不参与断言）；
  断言其 ⊆ 插件并集（防 L1 引用未登记进插件产物的源）。
缺失逐条打印退出 1，清单文件读取失败退出 2，全部一致打印 OK 退出 0。只用标准库。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SOURCES_DIR = ROOT / "cmake" / "sources"

PLUGIN_MANIFESTS = (
    "gda_core.cmake",
    "api_core.cmake",
    "api_contract.cmake",
    "api_storage.cmake",
    "api_transport.cmake",
    "api_cli.cmake",
    "api_instance_guard.cmake",
)
L1_MANIFESTS = (
    "gda_unit_sources.cmake",
    "api_unit_contract.cmake",
    "api_unit_storage.cmake",
    "api_unit_transport.cmake",
    "api_unit_cli.cmake",
    "api_unit_docs.cmake",
    "api_unit_instance_guard.cmake",
    "api_probe.cmake",
)

BLOCK_PATTERN = re.compile(r"(?:set|add_executable)\s*\(([^)]*)\)", re.DOTALL)
VAR_REF_PATTERN = re.compile(r"\$\{([^}]+)\}")
COMMENT_PATTERN = re.compile(r"#[^\n]*")


def rel_of(path):
    return path.relative_to(ROOT).as_posix()


def parse_manifest_tokens(path):
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        print("source-manifest-guard: read error: " + rel_of(path) + ": " + str(exc), file=sys.stderr)
        sys.exit(2)
    tokens = []
    for block in BLOCK_PATTERN.findall(COMMENT_PATTERN.sub("", text)):
        for token in block.split():
            if token.startswith("${CMAKE_SOURCE_DIR}/"):
                token = token[len("${CMAKE_SOURCE_DIR}/"):]
            if VAR_REF_PATTERN.search(token) is not None:
                continue
            tokens.append(token.replace("\\", "/"))
    return tokens


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

    plugin_entries = set()
    for name in PLUGIN_MANIFESTS:
        for token in parse_manifest_tokens(SOURCES_DIR / name):
            if token.startswith("src/"):
                plugin_entries.add(token)
    for path in sorted((ROOT / "src" / "tools").glob("*_ops.cpp")):
        plugin_entries.add(rel_of(path))

    src_actual = {rel_of(p) for p in (ROOT / "src").rglob("*.cpp")}

    errors = []
    for item in sorted(src_actual - plugin_entries):
        errors.append("plugin manifests miss: " + item)
    for item in sorted(plugin_entries - src_actual):
        errors.append("plugin manifests list non-existent source: " + item)

    l1_src_entries = set()
    for name in L1_MANIFESTS:
        for token in parse_manifest_tokens(SOURCES_DIR / name):
            if token.startswith("src/"):
                l1_src_entries.add(token)
    for item in sorted(l1_src_entries - plugin_entries):
        errors.append("L1 manifests reference source not in plugin manifests: " + item)

    if errors:
        for item in errors:
            print(item, file=sys.stderr)
        return 1

    print(
        "source-manifest-guard plugin sources "
        + str(len(plugin_entries))
        + " == src/ *.cpp "
        + str(len(src_actual))
        + "; L1 manifests reference "
        + str(len(l1_src_entries))
        + " plugin sources, all registered"
    )
    print("source-manifest-guard OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
