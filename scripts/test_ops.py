"""测试运行：L1 快筛、单跑、L2、全量（ctest 薄封装，口径见 docs/wiki/tests.md）。"""

import subprocess
from pathlib import Path

from .config import PROJECT_ROOT

L1_EXCLUDE_PATTERN = "^gda_runner_"
L2_ONLY_PATTERN = "^gda_runner_"


def _ctest(extra: list[str], preset: str = "debug") -> int:
    cmd = ["ctest", "--preset", preset, *extra]
    print(f"\n$ {' '.join(cmd)}", flush=True)
    return subprocess.run(cmd, cwd=PROJECT_ROOT).returncode


def run_l1(preset: str = "debug") -> int:
    return _ctest(["-E", L1_EXCLUDE_PATTERN], preset)


def run_single(name: str, preset: str = "debug") -> int:
    return _ctest(["-R", name], preset)


def run_l2(name: str | None = None, preset: str = "debug") -> int:
    if name:
        return _ctest(["-R", name], preset)
    return _ctest(["-R", L2_ONLY_PATTERN], preset)


def run_all(preset: str = "debug") -> int:
    return _ctest([], preset)
