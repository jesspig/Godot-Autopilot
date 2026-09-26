"""dist 打包：三平台库收集与 zip 归档。"""

import shutil
import sys
from pathlib import Path

from .config import ADDON_VERSION, DEMO_DIRS, PLATFORM_LIBS, PROJECT_ROOT, _addon_dir
from .deploy import _generate_gdextension


def _collect_platform_libs(libs_dir: Path, addon_dir: Path) -> int:
    collected = 0
    for lib_name in PLATFORM_LIBS.values():
        matches = list(libs_dir.rglob(lib_name))
        if not matches:
            continue
        shutil.copy2(matches[0], addon_dir / lib_name)
        print(f"  {lib_name}  <- {matches[0].relative_to(libs_dir)}", flush=True)
        collected += 1
    return collected


def _package_addon(libs_dir: Path | None = None) -> None:
    # 各 demo 的部署内容完全相同，打包时以第一个 demo 的 addons 为准。
    addon_dir = _addon_dir(DEMO_DIRS[0])
    if libs_dir is not None:
        addon_dir.mkdir(parents=True, exist_ok=True)
        for f in addon_dir.glob("~*"):
            f.unlink(missing_ok=True)
        count = _collect_platform_libs(libs_dir, addon_dir)
        if count != len(PLATFORM_LIBS):
            print(
                f"[ERROR] Expected {len(PLATFORM_LIBS)} platform libraries under {libs_dir}, found {count}",
                flush=True,
            )
            sys.exit(1)
        _generate_gdextension(addon_dir)
    elif not addon_dir.exists():
        print("[ERROR] Addons not deployed; run a build first (e.g. uv run main.py build)", flush=True)
        sys.exit(1)
    dist_dir = PROJECT_ROOT / "dist"
    dist_dir.mkdir(exist_ok=True)
    zip_path = shutil.make_archive(
        str(dist_dir / f"godot-autopilot-{ADDON_VERSION}"),
        "zip",
        root_dir=DEMO_DIRS[0],
        base_dir="addons/godot-autopilot",
    )
    size_kb = Path(zip_path).stat().st_size / 1024
    print(f"[PACKAGE] {zip_path} ({size_kb:.0f} KB)", flush=True)
