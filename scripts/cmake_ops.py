"""CMake 配置与编译。"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

from .config import BUILD_DIR, PROJECT_ROOT, cached_generator, resolve_preset


def _subprocess_env() -> dict[str, str]:
    """把当前解释器所在目录前置到子进程 PATH。

    cmake/skill_gen.cmake 需 find_package(Python3) 定位解释器以嵌入 skills；
    当前解释器目录（uv run 下为 .venv/Scripts）必然含可用的 python，前置到 PATH
    可避免依赖系统 PATH 顺序，也避免命中 Microsoft Store 的 python 存根。
    """
    env = os.environ.copy()
    env["PATH"] = str(Path(sys.executable).parent) + os.pathsep + env.get("PATH", "")
    return env


def _run(cmd: list[str], cwd: Path | None = None) -> bool:
    print(f"\n$ {' '.join(cmd)}", flush=True)
    return subprocess.run(
        cmd, cwd=cwd or PROJECT_ROOT, env=_subprocess_env()
    ).returncode == 0


def _purge(target: Path) -> None:
    print(f"\n[AUTO-CLEAN] Removing {target} (preserving _deps/)...", flush=True)
    for item in list(target.iterdir()):
        # 回退路径（third_party/ submodule 未检出）下 _deps 仍是 FetchContent
        # 缓存，删除会导致重新下载，故始终跳过。
        if item.name != "_deps":
            if item.is_dir():
                shutil.rmtree(item, ignore_errors=True)
            else:
                item.unlink(missing_ok=True)


def _configure(preset: str) -> bool:
    resolved = resolve_preset(preset)
    target = BUILD_DIR / preset
    want_ninja = resolved == preset

    # portable preset 省略 generator，CMake 会沿用 cache 里已有的生成器而非重新发现。
    # 若环境在此期间变了（有/无 ninja），cache 会留下不可用的生成器路径，构建必然失败。
    # 故这里主动比对并清空，而不是等 configure 自己报错。
    cached = cached_generator(target)
    if cached is not None and (cached == "Ninja") != want_ninja:
        print(
            f"\n[AUTO-CLEAN] Generator changed: {cached} -> "
            f"{'Ninja' if want_ninja else 'platform default'}",
            flush=True,
        )
        _purge(target)

    if _run(["cmake", "--preset", resolved]):
        return True
    if target.exists():
        _purge(target)
        return _run(["cmake", "--preset", resolved])
    return False


def _build(preset: str) -> bool:
    return _run(["cmake", "--build", "--preset", resolve_preset(preset)])