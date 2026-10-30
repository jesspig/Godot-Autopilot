"""清理：各 demo、L2 测试床与池床的引擎缓存与部署产物。"""

import shutil

from .config import DEPLOY_TARGETS, POOL_BEDS, _addon_dir


def _clean() -> None:
    for project_dir in [*DEPLOY_TARGETS, *POOL_BEDS]:
        godot_cache = project_dir / ".godot"
        if godot_cache.exists():
            print(f"[CLEAN] Removing {godot_cache}...", flush=True)
            shutil.rmtree(godot_cache, ignore_errors=True)
        addon_dir = _addon_dir(project_dir)
        if addon_dir.exists():
            print(f"[CLEAN] Removing {addon_dir}...", flush=True)
            shutil.rmtree(addon_dir, ignore_errors=True)
