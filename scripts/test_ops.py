"""测试运行：全量默认门与单跑（ctest 薄封装，口径见 docs/wiki/tests.md）。"""

import subprocess

from .config import PROJECT_ROOT, resolve_preset


def _ctest(extra: list[str], preset: str = "debug") -> int:
    cmd = ["ctest", "--preset", resolve_preset(preset), *extra]
    print(f"\n$ {' '.join(cmd)}", flush=True)
    return subprocess.run(cmd, cwd=PROJECT_ROOT).returncode


def run_default(preset: str = "debug") -> int:
    """全量默认门：ctest --preset <resolved>，无过滤。"""
    return _ctest([], preset)


def run_single(name: str, preset: str = "debug") -> int:
    return _ctest(["-R", name], preset)
