#!/usr/bin/env python3
"""Godot-Autopilot 构建脚本。

构建 GDExtension 并部署到 example/addons/。

Usage:
    uv run build.py                    # Debug 构建并部署
    uv run build.py --release          # 清理 + Release 构建并部署
    uv run build.py --release --package  # Release 构建并打包 dist/
    uv run build.py --package          # 打包已部署的 addons
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent
BUILD_DIR = PROJECT_ROOT / "build"
EXAMPLE_DIR = PROJECT_ROOT / "example"
EXAMPLE_ADDON_DIR = EXAMPLE_DIR / "addons" / "godot-autopilot"

PLATFORM_LIBS: dict[str, str] = {
    "windows": "godot-autopilot.dll",
    "linux": "libgodot-autopilot.so",
    "darwin": "libgodot-autopilot.dylib",
}

PLATFORM_PDB = "godot-autopilot.pdb"

ADDON_VERSION = (PROJECT_ROOT / "VERSION").read_text(encoding="utf-8").strip()


def _run(cmd: list[str], cwd: Path | None = None) -> bool:
    print(f"\n$ {' '.join(cmd)}", flush=True)
    return subprocess.run(cmd, cwd=cwd or PROJECT_ROOT).returncode == 0


def _configure(preset: str) -> bool:
    if _run(["cmake", "--preset", preset]):
        return True
    target = BUILD_DIR / preset
    if target.exists():
        deps_dir = target / "_deps"
        print(f"\n[AUTO-CLEAN] Removing {target} (preserving _deps/)...", flush=True)
        for item in list(target.iterdir()):
            if item.name != "_deps":
                if item.is_dir():
                    shutil.rmtree(item, ignore_errors=True)
                else:
                    item.unlink(missing_ok=True)
        return _run(["cmake", "--preset", preset])
    return False


def _build(preset: str) -> bool:
    return _run(["cmake", "--build", "--preset", preset])


def _generate_gdextension() -> None:
    EXAMPLE_ADDON_DIR.mkdir(parents=True, exist_ok=True)
    (EXAMPLE_ADDON_DIR / "godot-autopilot.gdextension").write_text(
        "[configuration]\n"
        'entry_symbol = "GDExtensionEntryPoint"\n'
        'compatibility_minimum = "4.3"\n'
        "\n"
        "[libraries]\n"
        'windows.debug.x86_64 = "res://addons/godot-autopilot/godot-autopilot.dll"\n'
        'windows.release.x86_64 = "res://addons/godot-autopilot/godot-autopilot.dll"\n'
        'linux.debug.x86_64 = "res://addons/godot-autopilot/godot-autopilot.so"\n'
        'linux.release.x86_64 = "res://addons/godot-autopilot/godot-autopilot.so"\n'
        'macos.debug.universal = "res://addons/godot-autopilot/libgodot-autopilot.dylib"\n'
        'macos.release.universal = "res://addons/godot-autopilot/libgodot-autopilot.dylib"\n',
        encoding="utf-8",
    )
    print(f"  godot-autopilot.gdextension  (generated)", flush=True)


def _deploy(preset: str) -> None:
    binary_dir = BUILD_DIR / preset
    EXAMPLE_ADDON_DIR.mkdir(parents=True, exist_ok=True)
    # Remove any Godot hot-reload backup DLLs from previous runs to avoid loading errors
    for f in EXAMPLE_ADDON_DIR.glob("~*"):
        f.unlink(missing_ok=True)
    _generate_gdextension()
    system = platform.system().lower()
    lib_name = PLATFORM_LIBS.get(system)
    if not lib_name:
        print(f"[ERROR] Unsupported platform: {system}", flush=True)
        sys.exit(1)
    lib_src = binary_dir / lib_name
    if lib_src.exists():
        shutil.copy2(lib_src, EXAMPLE_ADDON_DIR / lib_name)
        size_kb = lib_src.stat().st_size / 1024
        print(f"  {lib_name}  ({size_kb:.0f} KB)", flush=True)
    else:
        print(f"[WARN] {lib_name} not found in {binary_dir}", flush=True)
    if system == "windows":
        pdb_src = binary_dir / PLATFORM_PDB
        if pdb_src.exists():
            shutil.copy2(pdb_src, EXAMPLE_ADDON_DIR / PLATFORM_PDB)
            size_kb = pdb_src.stat().st_size / 1024
            print(f"  {PLATFORM_PDB}  ({size_kb:.0f} KB)", flush=True)
    print(f"  -> {EXAMPLE_ADDON_DIR}", flush=True)
    _validate_addon_integrity()


def _validate_addon_integrity() -> None:
    system = platform.system().lower()
    platform_key = {"windows": "windows", "linux": "linux", "darwin": "macos"}.get(system)
    if not platform_key:
        print(f"[ERROR] Unsupported platform: {system}", flush=True)
        sys.exit(1)
    gdextension_path = EXAMPLE_ADDON_DIR / "godot-autopilot.gdextension"
    content = gdextension_path.read_text(encoding="utf-8")
    in_libraries = False
    missing = []
    for line in content.splitlines():
        stripped = line.strip()
        if stripped.startswith("["):
            in_libraries = stripped == "[libraries]"
        elif in_libraries and "=" in stripped:
            key, _, value = stripped.partition("=")
            if key.split(".", 1)[0] == platform_key:
                res_path = value.strip().strip('"')
                disk_path = EXAMPLE_DIR / res_path.removeprefix("res://")
                if not disk_path.exists():
                    missing.append(res_path)
    if missing:
        for res_path in missing:
            print(
                f"[ERROR] Incomplete addon: {res_path} missing; export will fail in host projects",
                flush=True,
            )
        sys.exit(1)


def _collect_platform_libs(libs_dir: Path) -> int:
    collected = 0
    for lib_name in PLATFORM_LIBS.values():
        matches = list(libs_dir.rglob(lib_name))
        if not matches:
            continue
        shutil.copy2(matches[0], EXAMPLE_ADDON_DIR / lib_name)
        print(f"  {lib_name}  <- {matches[0].relative_to(libs_dir)}", flush=True)
        collected += 1
    return collected


def _package_addon(libs_dir: Path | None = None) -> None:
    if libs_dir is not None:
        EXAMPLE_ADDON_DIR.mkdir(parents=True, exist_ok=True)
        for f in EXAMPLE_ADDON_DIR.glob("~*"):
            f.unlink(missing_ok=True)
        count = _collect_platform_libs(libs_dir)
        if count != len(PLATFORM_LIBS):
            print(
                f"[ERROR] Expected {len(PLATFORM_LIBS)} platform libraries under {libs_dir}, found {count}",
                flush=True,
            )
            sys.exit(1)
        _generate_gdextension()
    elif not EXAMPLE_ADDON_DIR.exists():
        print("[ERROR] Addons not deployed; run a build first (e.g. uv run build.py)", flush=True)
        sys.exit(1)
    dist_dir = PROJECT_ROOT / "dist"
    dist_dir.mkdir(exist_ok=True)
    zip_path = shutil.make_archive(
        str(dist_dir / f"godot-autopilot-{ADDON_VERSION}"),
        "zip",
        root_dir=EXAMPLE_DIR,
        base_dir="addons/godot-autopilot",
    )
    size_kb = Path(zip_path).stat().st_size / 1024
    print(f"[PACKAGE] {zip_path} ({size_kb:.0f} KB)", flush=True)


def _clean() -> None:
    godot_cache = EXAMPLE_DIR / ".godot"
    if godot_cache.exists():
        print(f"[CLEAN] Removing {godot_cache}...", flush=True)
        shutil.rmtree(godot_cache, ignore_errors=True)
    if EXAMPLE_ADDON_DIR.exists():
        print(f"[CLEAN] Removing {EXAMPLE_ADDON_DIR}...", flush=True)
        shutil.rmtree(EXAMPLE_ADDON_DIR, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description="Godot-Autopilot build & deploy")
    parser.add_argument("--release", action="store_true", help="Release build (default: Debug)")
    parser.add_argument("--debug", action="store_true", help="Debug build (default)")
    parser.add_argument("--package", action="store_true", help="Package addons as dist/godot-autopilot-<version>.zip")
    parser.add_argument(
        "--libs-dir",
        type=Path,
        default=None,
        help="Collect cross-platform libraries from this directory before packaging (requires --package)",
    )
    args = parser.parse_args()

    if args.release and args.debug:
        print("[ERROR] Cannot specify both --release and --debug", flush=True)
        sys.exit(1)

    if args.libs_dir is not None and not args.package:
        print("[ERROR] --libs-dir requires --package", flush=True)
        sys.exit(1)

    if args.package and not args.release and not args.debug:
        print("[PACKAGE] Packaging existing addons...", flush=True)
        _package_addon(args.libs_dir)
        return

    config = "Release" if args.release else "Debug"
    preset = "release" if args.release else "debug"

    if args.release:
        _clean()

    print(f"[BUILD] Config={config}", flush=True)
    if not _configure(preset):
        sys.exit(1)
    if not _build(preset):
        sys.exit(1)

    print(f"\n[DEPLOY] Copying artifacts to example/addons/...", flush=True)
    _deploy(preset)

    if args.package:
        _package_addon(args.libs_dir)

    print(f"\n[DONE] Build + deploy complete.", flush=True)


if __name__ == "__main__":
    main()
