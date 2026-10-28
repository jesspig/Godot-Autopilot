"""CMake 池路由映射生成器：从 pools.json 渲染 pool_map.cmake。

读取 --pools 指定的 pools.json 单一事实源，校验 version/mode 与池名、用例的
唯一性（含顶层 isolated 元素不重复、不与任何池的 cases 相交）后，按 pools.json
顺序产出首行注释、GDA_POOL_MODE、GDA_POOL_NAMES（分号连接）、
GDA_POOL_ISOLATED（分号连接，缺省空串）以及每用例一条
set(GDA_POOL_<用例名大写且连字符转下划线> "<池名>") 语句，供
tests/CMakeLists.txt 在 configure 期 include 完成静态路由；isolated 用例不产出
路由变量。
"""

import argparse
import json
import sys
from pathlib import Path

EXPECTED_VERSION = 1
EXPECTED_MODE = "pool"
VARIABLE_PREFIX = "GDA_POOL_"
MODE_VARIABLE = "GDA_POOL_MODE"
NAMES_VARIABLE = "GDA_POOL_NAMES"
ISOLATED_VARIABLE = "GDA_POOL_ISOLATED"
HEADER_COMMENT = "# generated from pools.json - do not edit"


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="从 pools.json 生成 CMake 池路由映射")
    parser.add_argument("--pools", required=True, help="pools.json 路径")
    parser.add_argument("--out", required=True, help="要写出的 CMake 文件路径")
    return parser.parse_args(argv)


def require_mapping(value: object, path: str) -> dict:
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected object, got {type(value).__name__}")
    return value


def require_field(container: dict, key: str, path: str) -> object:
    if key not in container:
        raise ValueError(f"{path}.{key}: missing required field")
    return container[key]


def cmake_variable_name(case: str) -> str:
    return VARIABLE_PREFIX + case.upper().replace("-", "_")


def load_pool_assignment(
    pools_path: Path,
) -> tuple[list[str], dict[str, str], list[str], str]:
    try:
        payload = json.loads(pools_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"{pools_path}: invalid JSON: {exc}") from exc
    root = require_mapping(payload, "pools.json")
    version = require_field(root, "version", "pools.json")
    if version != EXPECTED_VERSION:
        raise ValueError(f"pools.json.version: expected {EXPECTED_VERSION}, got {version!r}")
    mode = require_field(root, "mode", "pools.json")
    if mode != EXPECTED_MODE:
        raise ValueError(f"pools.json.mode: expected {EXPECTED_MODE!r}, got {mode!r}")
    pools = require_field(root, "pools", "pools.json")
    if not isinstance(pools, list) or not pools:
        raise ValueError("pools.json.pools: expected non-empty array")
    pool_names: list[str] = []
    case_owner: dict[str, str] = {}
    for index, entry in enumerate(pools):
        pool = require_mapping(entry, f"pools.json.pools[{index}]")
        name = require_field(pool, "name", f"pools.json.pools[{index}]")
        if not isinstance(name, str) or not name:
            raise ValueError(
                f"pools.json.pools[{index}].name: expected non-empty string, got {name!r}"
            )
        if name in pool_names:
            raise ValueError(f"pools.json.pools[{index}].name: duplicate pool name {name!r}")
        pool_names.append(name)
        cases = require_field(pool, "cases", f"pools.json.pools[{index}]")
        if not isinstance(cases, list) or not cases:
            raise ValueError(
                f"pools.json.pools[{index}].cases: expected non-empty array of case names"
            )
        for position, case in enumerate(cases):
            if not isinstance(case, str) or not case:
                raise ValueError(
                    f"pools.json.pools[{index}].cases[{position}]: "
                    f"expected non-empty case name, got {case!r}"
                )
            if case in case_owner:
                raise ValueError(
                    f"pools.json.pools[{index}].cases[{position}]: duplicate case {case!r} "
                    f"(already mapped by pool {case_owner[case]!r})"
                )
            case_owner[case] = name
    isolated = root.get("isolated", [])
    if not isinstance(isolated, list):
        raise ValueError(
            f"pools.json.isolated: expected array, got {type(isolated).__name__}"
        )
    isolated_cases: list[str] = []
    for position, case in enumerate(isolated):
        if not isinstance(case, str) or not case:
            raise ValueError(
                f"pools.json.isolated[{position}]: "
                f"expected non-empty case name, got {case!r}"
            )
        if case in isolated_cases:
            raise ValueError(
                f"pools.json.isolated[{position}]: duplicate case {case!r}"
            )
        owner = case_owner.get(case)
        if owner is not None:
            raise ValueError(
                f"pools.json.isolated[{position}]: case {case!r} is also routed to "
                f"pool {owner!r}; isolated cases must not appear in any pool"
            )
        isolated_cases.append(case)
    return pool_names, case_owner, isolated_cases, mode


def render_pool_map(pool_names: list[str], case_owner: dict[str, str],
                    isolated_cases: list[str], mode: str) -> str:
    lines = [
        HEADER_COMMENT,
        f'set({MODE_VARIABLE} "{mode}")',
        f'set({NAMES_VARIABLE} "{";".join(pool_names)}")',
        f'set({ISOLATED_VARIABLE} "{";".join(isolated_cases)}")',
    ]
    lines.extend(
        f'set({cmake_variable_name(case)} "{pool}")' for case, pool in case_owner.items()
    )
    return "\n".join(lines) + "\n"


def write_pool_map(out_path: Path, text: str) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        pool_names, case_owner, isolated_cases, mode = load_pool_assignment(
            Path(args.pools)
        )
        write_pool_map(
            Path(args.out),
            render_pool_map(pool_names, case_owner, isolated_cases, mode),
        )
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(
        f"wrote pool map: mode={mode}, pools={';'.join(pool_names)}, "
        f"isolated={';'.join(isolated_cases)}, cases={len(case_owner)} -> {args.out}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
