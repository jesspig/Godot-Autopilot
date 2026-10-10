"""CMake 配置与编译。"""

import shutil
import subprocess
from pathlib import Path

from .config import BUILD_DIR, PROJECT_ROOT


def _run(cmd: list[str], cwd: Path | None = None) -> bool:
    print(f"\n$ {' '.join(cmd)}", flush=True)
    return subprocess.run(cmd, cwd=cwd or PROJECT_ROOT).returncode == 0


def _configure(preset: str) -> bool:
    if _run(["cmake", "--preset", preset]):
        return True
    target = BUILD_DIR / preset
    if target.exists():
        print(f"\n[AUTO-CLEAN] Removing {target} (preserving _deps/)...", flush=True)
        for item in list(target.iterdir()):
            # 回退路径（third_party/ submodule 未检出）下 _deps 仍是 FetchContent
            # 缓存，删除会导致重新下载，故始终跳过。
            if item.name != "_deps":
                if item.is_dir():
                    shutil.rmtree(item, ignore_errors=True)
                else:
                    item.unlink(missing_ok=True)
        return _run(["cmake", "--preset", preset])
    return False


def _build(preset: str) -> bool:
    return _run(["cmake", "--build", "--preset", preset])
