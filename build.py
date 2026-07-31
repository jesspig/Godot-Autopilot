#!/usr/bin/env python3
"""Godot-Self-Driving 构建脚本。

构建 GDExtension 并部署到 example/addons/。

Usage:
    uv run build.py                    # Debug 构建并部署
    uv run build.py --release          # 清理 + Release 构建并部署
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
EXAMPLE_ADDON_DIR = EXAMPLE_DIR / "addons" / "godot-self-driving"

PLATFORM_LIBS: dict[str, str] = {
    "windows": "godot-self-driving.dll",
    "linux": "libgodot-self-driving.so",
    "darwin": "libgodot-self-driving.dylib",
}

PLATFORM_PDB = "godot-self-driving.pdb"


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
    (EXAMPLE_ADDON_DIR / "godot-self-driving.gdextension").write_text(
        "[configuration]\n"
        'entry_symbol = "GDExtensionEntryPoint"\n'
        'compatibility_minimum = "4.3"\n'
        "\n"
        "[libraries]\n"
        'windows.debug.x86_64 = "res://addons/godot-self-driving/godot-self-driving.dll"\n'
        'windows.release.x86_64 = "res://addons/godot-self-driving/godot-self-driving.dll"\n'
        'linux.debug.x86_64 = "res://addons/godot-self-driving/godot-self-driving.so"\n'
        'linux.release.x86_64 = "res://addons/godot-self-driving/godot-self-driving.so"\n'
        'macos.debug.x86_64 = "res://addons/godot-self-driving/godot-self-driving.dylib"\n'
        'macos.release.x86_64 = "res://addons/godot-self-driving/godot-self-driving.dylib"\n',
        encoding="utf-8",
    )
    print(f"  godot-self-driving.gdextension  (generated)", flush=True)


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


def _clean() -> None:
    godot_cache = EXAMPLE_DIR / ".godot"
    if godot_cache.exists():
        print(f"[CLEAN] Removing {godot_cache}...", flush=True)
        shutil.rmtree(godot_cache, ignore_errors=True)
    if EXAMPLE_ADDON_DIR.exists():
        print(f"[CLEAN] Removing {EXAMPLE_ADDON_DIR}...", flush=True)
        shutil.rmtree(EXAMPLE_ADDON_DIR, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description="Godot-Self-Driving build & deploy")
    parser.add_argument("--release", action="store_true", help="Release build (default: Debug)")
    parser.add_argument("--debug", action="store_true", help="Debug build (default)")
    args = parser.parse_args()

    if args.release and args.debug:
        print("[ERROR] Cannot specify both --release and --debug", flush=True)
        sys.exit(1)

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

    print(f"\n[DONE] Build + deploy complete.", flush=True)


if __name__ == "__main__":
    main()
