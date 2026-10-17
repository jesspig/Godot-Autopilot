#!/usr/bin/env python3
"""Wrap `bun test cli/tests` as a ctest point.

bun 缺失时打印 SKIP 并 exit 0（ctest 视为通过）；存在时以仓库根为 cwd
执行 bun test cli/tests，stdout/stderr 与退出码原样透传。
"""

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    bun = shutil.which("bun")
    if bun is None:
        print("SKIP: bun not found on PATH, cli unit tests skipped")
        return 0
    proc = subprocess.run([bun, "test", "cli/tests"], cwd=ROOT)
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
