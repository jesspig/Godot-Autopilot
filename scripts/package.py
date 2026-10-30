"""dist 打包：三平台库与 C# 桥收集、zip 归档。"""

import platform
import shutil
import sys
import tempfile
from pathlib import Path

from .config import (
    ADDON_VERSION,
    BRIDGE_DIR,
    BRIDGE_SOURCES,
    DEMO_DIRS,
    PLATFORM_LIBS,
    PROJECT_ROOT,
    _addon_dir,
)
from .deploy import _generate_gdextension

# zip 内的插件目录前缀，下游按此路径安装，不可变更
ADDON_SUBPATH = Path("addons") / "godot-autopilot"


def _collect_platform_libs(libs_dir: Path, addon_dir: Path) -> set[str]:
    """从 libs_dir 递归收集平台库到 addon_dir，返回收集到的库名集合。

    缺失的库不在此处报错，由调用方按必需清单判定。
    """
    collected: set[str] = set()
    for lib_name in PLATFORM_LIBS.values():
        matches = list(libs_dir.rglob(lib_name))
        if not matches:
            continue
        shutil.copy2(matches[0], addon_dir / lib_name)
        print(f"  {lib_name}  <- {matches[0].relative_to(libs_dir)}", flush=True)
        collected.add(lib_name)
    return collected


def _collect_csharp_bridge(addon_dir: Path) -> None:
    """把 csharp_bridge/ 下的 C# 桥源码复制到 addon_dir，必需文件缺失即报错退出。"""
    missing = [name for name in BRIDGE_SOURCES if not (BRIDGE_DIR / name).is_file()]
    if missing:
        print(
            f"[ERROR] Missing C# bridge sources in {BRIDGE_DIR}: {', '.join(missing)}",
            flush=True,
        )
        sys.exit(1)
    for src in sorted(BRIDGE_DIR.glob("*.cs")):
        shutil.copy2(src, addon_dir / src.name)
        size_kb = src.stat().st_size / 1024
        print(f"  {src.name}  ({size_kb:.0f} KB)", flush=True)


def _collect_package_files(lib_dir: Path, addon_dir: Path, required_libs: list[str]) -> None:
    """收集打包内容到 addon_dir：平台库 + C# 桥源码 + gdextension 描述文件。

    lib_dir 是平台库来源目录（递归查找取首个匹配）；required_libs 是必须齐备的
    平台库名，缺失任一即报错退出。.pdb 不在收集范围，不随包分发。
    """
    found = _collect_platform_libs(lib_dir, addon_dir)
    missing = [name for name in required_libs if name not in found]
    if missing:
        print(
            f"[ERROR] Missing platform libraries under {lib_dir}: {', '.join(missing)}",
            flush=True,
        )
        sys.exit(1)
    _collect_csharp_bridge(addon_dir)
    _generate_gdextension(addon_dir)


def _package_addon(libs_dir: Path | None = None) -> None:
    """把插件内容打包为 dist/godot-autopilot-<version>.zip。

    libs_dir 非空时从该目录收集平台库（三平台必须齐备）；否则从已部署的 demo
    目录收集（本平台库必须就位）。两条路径产出同构内容：平台库 + C# 桥源码 +
    gdextension 描述文件，均不含 .pdb。
    """
    if libs_dir is not None:
        lib_dir = libs_dir
        required_libs = list(PLATFORM_LIBS.values())
    else:
        deployed_dir = _addon_dir(DEMO_DIRS[0])
        if not deployed_dir.exists():
            print("[ERROR] Addons not deployed; run a build first (e.g. uv run main.py build)", flush=True)
            sys.exit(1)
        lib_dir = deployed_dir
        system = platform.system().lower()
        lib_name = PLATFORM_LIBS.get(system)
        if not lib_name:
            print(f"[ERROR] Unsupported platform: {system}", flush=True)
            sys.exit(1)
        required_libs = [lib_name]

    dist_dir = PROJECT_ROOT / "dist"
    dist_dir.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gda-package-") as tmp:
        stage_root = Path(tmp)
        stage_addon = stage_root / ADDON_SUBPATH
        stage_addon.mkdir(parents=True)
        _collect_package_files(lib_dir, stage_addon, required_libs)
        zip_path = shutil.make_archive(
            str(dist_dir / f"godot-autopilot-{ADDON_VERSION}"),
            "zip",
            root_dir=stage_root,
            base_dir=ADDON_SUBPATH.as_posix(),
        )
    size_kb = Path(zip_path).stat().st_size / 1024
    print(f"[PACKAGE] {zip_path} ({size_kb:.0f} KB)", flush=True)
