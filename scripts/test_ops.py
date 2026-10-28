"""测试运行：L1 快筛、单跑、L2、全量（ctest 薄封装，口径见 docs/wiki/tests.md）。"""

import subprocess
from pathlib import Path

from .config import PROJECT_ROOT, resolve_preset

L1_EXCLUDE_PATTERN = "^gda_runner_"
L2_ONLY_PATTERN = "^gda_runner_"

# 全量须两阶段：阶段 A 串行跑 L1 + 守卫 + CLI，-E 同时排除 L2 用例与池 fixtures；
# 阶段 B 只跑 L2 池化 fixtures 与用例，按池并行。
PHASE_A_EXCLUDE_PATTERN = "^gda_runner_|^pool_"
PHASE_B_INCLUDE_PATTERN = "^gda_runner_|^pool_"


def _ctest(extra: list[str], preset: str = "debug", jobs: int = 0) -> int:
    cmd = ["ctest", "--preset", resolve_preset(preset)]
    if jobs > 0:
        cmd += ["-j", str(jobs)]
    cmd += extra
    print(f"\n$ {' '.join(cmd)}", flush=True)
    return subprocess.run(cmd, cwd=PROJECT_ROOT).returncode


def run_l1(preset: str = "debug") -> int:
    return _ctest(["-E", L1_EXCLUDE_PATTERN], preset)


def run_single(name: str, preset: str = "debug") -> int:
    return _ctest(["-R", name], preset)


def run_l2(name: str | None = None, preset: str = "debug", jobs: int = 5) -> int:
    """全量 L2 按 -j jobs 并行；指定 name 时串行单跑。"""
    if name:
        return _ctest(["-R", name], preset)
    return _ctest(["-R", L2_ONLY_PATTERN], preset, jobs)


def run_all(preset: str = "debug", jobs: int = 5) -> int:
    """两阶段全量：阶段 A（L1 + 守卫 + CLI）串行，失败即中止；阶段 B（L2 池化 fixtures + 用例）并行。"""
    code = _ctest(["-E", PHASE_A_EXCLUDE_PATTERN], preset)
    if code != 0:
        return code
    return _ctest(["-R", PHASE_B_INCLUDE_PATTERN], preset, jobs)


def run_legacy_l2(preset: str = "debug") -> int:
    """强制 legacy 编排跑 L2：configure 覆盖 GDA_POOL_MODE=per-test，结束后提示恢复。"""
    resolved = resolve_preset(preset)
    configure = ["cmake", "--preset", resolved, "-DGDA_POOL_MODE=per-test"]
    try:
        print(f"\n$ {' '.join(configure)}", flush=True)
        code = subprocess.run(configure, cwd=PROJECT_ROOT).returncode
        print(f"[LEGACY] configure 退出码 {code}", flush=True)
        if code != 0:
            return code
        return _ctest(["-R", L2_ONLY_PATTERN], preset)
    finally:
        print(f"\n[LEGACY] 恢复池模式：cmake --preset {resolved} -UGDA_POOL_MODE", flush=True)
