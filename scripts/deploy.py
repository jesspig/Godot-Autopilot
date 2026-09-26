"""插件部署：gdextension 生成、平台库复制、完整性校验。"""

import platform
import shutil
import sys
from pathlib import Path

from .config import BUILD_DIR, DEMO_DIRS, PLATFORM_LIBS, PLATFORM_PDB, _addon_dir


def _generate_gdextension(addon_dir: Path) -> None:
    addon_dir.mkdir(parents=True, exist_ok=True)
    (addon_dir / "godot-autopilot.gdextension").write_text(
        "[configuration]\n"
        'entry_symbol = "GDExtensionEntryPoint"\n'
        'compatibility_minimum = "4.7"\n'
        "\n"
        "[libraries]\n"
        'windows.debug.x86_64 = "res://addons/godot-autopilot/godot-autopilot.dll"\n'
        'windows.release.x86_64 = "res://addons/godot-autopilot/godot-autopilot.dll"\n'
        'linux.debug.x86_64 = "res://addons/godot-autopilot/libgodot-autopilot.so"\n'
        'linux.release.x86_64 = "res://addons/godot-autopilot/libgodot-autopilot.so"\n'
        'macos.debug.universal = "res://addons/godot-autopilot/libgodot-autopilot.dylib"\n'
        'macos.release.universal = "res://addons/godot-autopilot/libgodot-autopilot.dylib"\n',
        encoding="utf-8",
    )
    print(f"  godot-autopilot.gdextension  (generated)", flush=True)


def _deploy(preset: str, demos: list[Path] | None = None) -> None:
    binary_dir = BUILD_DIR / preset
    system = platform.system().lower()
    lib_name = PLATFORM_LIBS.get(system)
    if not lib_name:
        print(f"[ERROR] Unsupported platform: {system}", flush=True)
        sys.exit(1)
    for demo_dir in demos or DEMO_DIRS:
        addon_dir = _addon_dir(demo_dir)
        addon_dir.mkdir(parents=True, exist_ok=True)
        # Remove any Godot hot-reload backup DLLs from previous runs to avoid loading errors
        for f in addon_dir.glob("~*"):
            f.unlink(missing_ok=True)
        _generate_gdextension(addon_dir)
        lib_src = binary_dir / lib_name
        if lib_src.exists():
            shutil.copy2(lib_src, addon_dir / lib_name)
            size_kb = lib_src.stat().st_size / 1024
            print(f"  {lib_name}  ({size_kb:.0f} KB)", flush=True)
        else:
            print(f"[WARN] {lib_name} not found in {binary_dir}", flush=True)
        if system == "windows":
            pdb_src = binary_dir / PLATFORM_PDB
            if pdb_src.exists():
                shutil.copy2(pdb_src, addon_dir / PLATFORM_PDB)
                size_kb = pdb_src.stat().st_size / 1024
                print(f"  {PLATFORM_PDB}  ({size_kb:.0f} KB)", flush=True)
        print(f"  -> {addon_dir}", flush=True)
        _validate_addon_integrity(addon_dir, demo_dir)


def _validate_addon_integrity(addon_dir: Path, demo_dir: Path) -> None:
    system = platform.system().lower()
    platform_key = {"windows": "windows", "linux": "linux", "darwin": "macos"}.get(system)
    if not platform_key:
        print(f"[ERROR] Unsupported platform: {system}", flush=True)
        sys.exit(1)
    gdextension_path = addon_dir / "godot-autopilot.gdextension"
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
                disk_path = demo_dir / res_path.removeprefix("res://")
                if not disk_path.exists():
                    missing.append(res_path)
    if missing:
        for res_path in missing:
            print(
                f"[ERROR] Incomplete addon: {res_path} missing; export will fail in host projects",
                flush=True,
            )
        sys.exit(1)
