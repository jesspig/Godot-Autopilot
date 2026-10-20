#!/usr/bin/env python3
"""打包产物完整性自检：校验 addons.zip 的平台库、C# 桥源码与 gdextension 一致性。

用法：
    python scripts/test_package_integrity.py <zip 路径>

校验项：
    1. 三平台库（dll/so/dylib）齐全；
    2. C# 桥源码（BRIDGE_SOURCES 全部）齐全；
    3. gdextension 的 [libraries] 条目与包内实际文件一一对应；
    4. 包内不含 .pdb（调试符号不随包分发）。

全部通过打印 package-integrity OK 并退出 0；存在缺失项逐条打印并退出 1；
参数错误退出 2。只用标准库。
"""

import argparse
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.config import BRIDGE_SOURCES, PLATFORM_LIBS

ADDON_SUBPATH = Path("addons") / "godot-autopilot"
GDEXTENSION_NAME = "godot-autopilot.gdextension"
RES_PREFIX = "res://"


def _libraries_entries(gdext_path: Path) -> list[str]:
    """读 gdextension 的 [libraries] 段，返回其中的 res:// 路径。"""
    entries: list[str] = []
    in_libraries = False
    for line in gdext_path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped.startswith("["):
            in_libraries = stripped == "[libraries]"
            continue
        if not in_libraries or "=" not in stripped:
            continue
        _, _, value = stripped.partition("=")
        entries.append(value.strip().strip('"'))
    return entries


def _collect_problems(extract_root: Path) -> list[str]:
    """在解压目录里逐项核对，返回问题列表，空列表表示全部通过。"""
    problems: list[str] = []
    addon_dir = extract_root / ADDON_SUBPATH
    if not addon_dir.is_dir():
        return [f"zip 内缺少插件目录 {ADDON_SUBPATH.as_posix()}"]

    for lib_name in PLATFORM_LIBS.values():
        if not (addon_dir / lib_name).is_file():
            problems.append(f"缺少平台库 {lib_name}")

    for name in BRIDGE_SOURCES:
        if not (addon_dir / name).is_file():
            problems.append(f"缺少 C# 桥源码 {name}")

    gdext_path = addon_dir / GDEXTENSION_NAME
    if not gdext_path.is_file():
        problems.append(f"缺少描述文件 {GDEXTENSION_NAME}")
    else:
        referenced: set[str] = set()
        missing_targets: set[str] = set()
        for res_path in _libraries_entries(gdext_path):
            referenced.add(Path(res_path).name)
            if not res_path.startswith(RES_PREFIX):
                problems.append(f"gdextension 条目不是 res:// 路径：{res_path}")
                continue
            if not (extract_root / res_path[len(RES_PREFIX) :]).is_file():
                missing_targets.add(res_path)
        for res_path in sorted(missing_targets):
            problems.append(f"gdextension 条目 {res_path} 在包内不存在")
        for lib_name in PLATFORM_LIBS.values():
            if (addon_dir / lib_name).is_file() and lib_name not in referenced:
                problems.append(f"平台库 {lib_name} 未被 gdextension 引用")

    for pdb_path in extract_root.rglob("*.pdb"):
        rel = pdb_path.relative_to(extract_root).as_posix()
        problems.append(f"包内含调试符号 {rel}（不随包分发）")

    return problems


def main(argv: list[str] | None = None) -> int:
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    try:
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    parser = argparse.ArgumentParser(description="校验打包产物完整性")
    parser.add_argument("zip_path", type=Path, help="待校验的 zip 路径")
    args = parser.parse_args(argv)

    zip_path: Path = args.zip_path
    if not zip_path.is_file():
        print(f"[ERROR] zip not found: {zip_path}", flush=True)
        return 1

    with tempfile.TemporaryDirectory(prefix="gda-package-check-") as tmp:
        extract_root = Path(tmp)
        try:
            with zipfile.ZipFile(zip_path) as archive:
                archive.extractall(extract_root)
        except (zipfile.BadZipFile, OSError) as exc:
            print(f"[ERROR] cannot read zip {zip_path}: {exc}", flush=True)
            return 1
        problems = _collect_problems(extract_root)

    if problems:
        for item in problems:
            print(f"[ERROR] {item}", flush=True)
        print(f"[FAIL] {len(problems)} problem(s) in {zip_path}", flush=True)
        return 1

    print(f"package-integrity OK: {zip_path}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
