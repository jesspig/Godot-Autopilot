"""清理：各 demo 的引擎缓存与部署产物。"""

import shutil

from .config import DEMO_DIRS, _addon_dir


def _clean() -> None:
    for demo_dir in DEMO_DIRS:
        godot_cache = demo_dir / ".godot"
        if godot_cache.exists():
            print(f"[CLEAN] Removing {godot_cache}...", flush=True)
            shutil.rmtree(godot_cache, ignore_errors=True)
        addon_dir = _addon_dir(demo_dir)
        if addon_dir.exists():
            print(f"[CLEAN] Removing {addon_dir}...", flush=True)
            shutil.rmtree(addon_dir, ignore_errors=True)
