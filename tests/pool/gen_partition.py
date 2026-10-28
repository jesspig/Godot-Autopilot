"""池分区单一事实源生成器：从 ctest 报告装箱生成 tests/pool/pools.json。

读取 --reports 目录下全部 report-*.json（含 generated_at/total_files/passed_files/
files[].{name,passed,duration_ms,steps}）；files[].name 按 tests/config/*.json 建立
的「基名 ↔ 顶层 name 字段」映射归一到用例基名（config 缺 name 字段视为等于基名，
两条途径都未命中的条目视为未知报告忽略，stderr 打一行 info，stdout 契约不变仍恰一
行汇总）；同名基名取 generated_at 最新一份的 duration_ms；deny 池固定装载
03_tools_contract/09_editor_ui/47_authorization_gate 三例，其余非 deny 用例按
duration_ms 降序做 LPT 贪心装箱均衡装入 main-1..main-4，特例 38_csharp_bridge 固
定排入 main-1 首位；写盘前按 tests/config/*.json 全集校验并集、无重复、allow 取值
与池名集合，基名完全无报告时报 missing（含字段路径），违例抛出含字段路径的错误并
以非零码退出。
"""

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CONFIG_DIR = REPO_ROOT / "tests" / "config"
REPORT_GLOB = "report-*.json"
POOL_VERSION = 1
POOL_MODE = "pool"
DENY_POOL_NAME = "deny"
MAIN_POOL_NAMES: tuple[str, ...] = ("main-1", "main-2", "main-3", "main-4")
POOL_NAMES: tuple[str, ...] = (DENY_POOL_NAME, *MAIN_POOL_NAMES)
DENY_CASES: tuple[str, ...] = (
    "03_tools_contract",
    "09_editor_ui",
    "47_authorization_gate",
)
PINNED_CASE = "38_csharp_bridge"
PINNED_POOL_NAME = "main-1"
ALLOW_NONE = "none"
ALLOW_ALL = "all"
ALLOW_VALUES = (ALLOW_NONE, ALLOW_ALL)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="从 report-*.json 装箱生成 pools.json")
    parser.add_argument("--reports", required=True, help="report-*.json 所在目录")
    parser.add_argument("--out", required=True, help="要写出的 pools.json 路径")
    return parser.parse_args(argv)


def require_mapping(value: object, path: str) -> dict:
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected object, got {type(value).__name__}")
    return value


def require_field(container: dict, key: str, path: str) -> object:
    if key not in container:
        raise ValueError(f"{path}.{key}: missing required field")
    return container[key]


def parse_generated_at(raw: object, path: str) -> datetime:
    if not isinstance(raw, str) or not raw.strip():
        raise ValueError(f"{path}: expected ISO 8601 string, got {raw!r}")
    text = raw.strip()
    if text.endswith(("Z", "z")):
        text = text[:-1] + "+00:00"
    try:
        moment = datetime.fromisoformat(text)
    except ValueError as exc:
        raise ValueError(f"{path}: cannot parse ISO 8601 timestamp {raw!r}: {exc}") from exc
    if moment.tzinfo is None:
        moment = moment.replace(tzinfo=timezone.utc)
    return moment


def config_case_index(config_dir: Path = CONFIG_DIR) -> tuple[list[str], dict[str, str]]:
    if not config_dir.is_dir():
        raise ValueError(f"tests/config: directory not found: {config_dir}")
    paths = sorted(config_dir.glob("*.json"))
    if not paths:
        raise ValueError(f"tests/config: no *.json case files found in {config_dir}")
    entries: list[tuple[str, str]] = []
    for path in paths:
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            raise ValueError(f"tests/config/{path.name}: invalid JSON: {exc}") from exc
        payload = require_mapping(payload, f"tests/config/{path.name}")
        name = payload.get("name", path.stem)
        if not isinstance(name, str) or not name:
            raise ValueError(
                f"tests/config/{path.name}.name: expected non-empty string, got {name!r}"
            )
        entries.append((path.stem, name))
    case_names = sorted(stem for stem, _ in entries)
    name_set = set(case_names)
    aliases: dict[str, str] = {}
    for stem, name in entries:
        if name in name_set and name != stem:
            raise ValueError(
                f"tests/config/{stem}.json.name: {name!r} equals another case file stem; "
                f"report names would be ambiguous"
            )
        owner = aliases.get(name)
        if owner is not None:
            raise ValueError(
                f"tests/config/{stem}.json.name: {name!r} already provided by "
                f"{owner}.json; report names would be ambiguous"
            )
        aliases[name] = stem
    return case_names, aliases


def collect_report_cases(
    report_path: Path,
    newest: dict[str, tuple[datetime, str, int | float]],
    case_names: set[str],
    aliases: dict[str, str],
    ignored: list[str],
) -> None:
    try:
        payload = json.loads(report_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"{report_path.name}: invalid JSON: {exc}") from exc
    report = require_mapping(payload, report_path.name)
    generated_at = parse_generated_at(
        require_field(report, "generated_at", report_path.name),
        f"{report_path.name}.generated_at",
    )
    files = require_field(report, "files", report_path.name)
    if not isinstance(files, list):
        raise ValueError(f"{report_path.name}.files: expected array, got {type(files).__name__}")
    for index, entry in enumerate(files):
        entry = require_mapping(entry, f"{report_path.name}.files[{index}]")
        name = require_field(entry, "name", f"{report_path.name}.files[{index}]")
        if not isinstance(name, str) or not name:
            raise ValueError(
                f"{report_path.name}.files[{index}].name: expected non-empty string, got {name!r}"
            )
        duration = require_field(entry, "duration_ms", f"{report_path.name}.files[{index}]")
        if isinstance(duration, bool) or not isinstance(duration, (int, float)) or duration < 0:
            raise ValueError(
                f"{report_path.name}.files[{index}].duration_ms: "
                f"expected non-negative number, got {duration!r}"
            )
        if name in case_names:
            case = name
        else:
            case = aliases.get(name)
        if case is None:
            ignored.append(name)
            continue
        known = newest.get(case)
        if known is None or generated_at > known[0]:
            newest[case] = (generated_at, report_path.name, duration)


def load_case_durations(
    reports_dir: Path,
    case_names: set[str],
    aliases: dict[str, str],
) -> tuple[dict[str, int | float], list[str]]:
    if not reports_dir.is_dir():
        raise ValueError(f"--reports {reports_dir}: directory does not exist")
    report_paths = sorted(reports_dir.glob(REPORT_GLOB))
    if not report_paths:
        raise ValueError(f"--reports {reports_dir}: no {REPORT_GLOB} files found")
    newest: dict[str, tuple[datetime, str, int | float]] = {}
    ignored: list[str] = []
    for report_path in report_paths:
        collect_report_cases(report_path, newest, case_names, aliases, ignored)
    case_durations = {name: entry[2] for name, entry in newest.items()}
    return case_durations, ignored


def build_partition(case_durations: dict[str, int | float]) -> list[dict]:
    members: dict[str, list[str]] = {name: [] for name in MAIN_POOL_NAMES}
    loads: dict[str, int | float] = {name: 0 for name in MAIN_POOL_NAMES}
    if PINNED_CASE in case_durations:
        members[PINNED_POOL_NAME].append(PINNED_CASE)
        loads[PINNED_POOL_NAME] = case_durations[PINNED_CASE]
    candidates = sorted(
        (
            (name, duration)
            for name, duration in case_durations.items()
            if name not in DENY_CASES and name != PINNED_CASE
        ),
        key=lambda item: (-item[1], item[0]),
    )
    for name, duration in candidates:
        target = min(MAIN_POOL_NAMES, key=lambda pool_name: loads[pool_name])
        members[target].append(name)
        loads[target] = loads[target] + duration
    pools = [{"name": DENY_POOL_NAME, "allow": ALLOW_NONE, "cases": list(DENY_CASES)}]
    pools.extend(
        {"name": pool_name, "allow": ALLOW_ALL, "cases": members[pool_name]}
        for pool_name in MAIN_POOL_NAMES
    )
    return pools


def validate_partition(pools: list[dict], config_names: list[str]) -> None:
    pool_names = [pool["name"] for pool in pools]
    if len(set(pool_names)) != len(pool_names):
        raise ValueError(f"pools[*].name: duplicate pool names in {pool_names}")
    if set(pool_names) != set(POOL_NAMES):
        raise ValueError(f"pools[*].name: expected exactly {list(POOL_NAMES)}, got {pool_names}")
    if pool_names != list(POOL_NAMES):
        raise ValueError(f"pools[*].name: expected order {list(POOL_NAMES)}, got {pool_names}")
    for index, pool in enumerate(pools):
        if pool["allow"] not in ALLOW_VALUES:
            raise ValueError(
                f"pools[{index}].allow: expected one of {list(ALLOW_VALUES)}, got {pool['allow']!r}"
            )
    if pools[0]["cases"] != list(DENY_CASES):
        raise ValueError(
            f"pools[0].cases: deny pool must be exactly {list(DENY_CASES)} in fixed order, "
            f"got {pools[0]['cases']}"
        )
    case_owner: dict[str, str] = {}
    for index, pool in enumerate(pools):
        cases = pool["cases"]
        if not isinstance(cases, list) or not cases:
            raise ValueError(f"pools[{index}].cases: expected non-empty array of case names")
        for position, case in enumerate(cases):
            if not isinstance(case, str) or not case:
                raise ValueError(
                    f"pools[{index}].cases[{position}]: expected non-empty case name, got {case!r}"
                )
            if case in case_owner:
                raise ValueError(
                    f"pools[{index}].cases[{position}]: duplicate case {case!r} "
                    f"(already in pool {case_owner[case]!r})"
                )
            case_owner[case] = pool["name"]
    partition_set = set(case_owner)
    config_set = set(config_names)
    missing = sorted(config_set - partition_set)
    extra = sorted(partition_set - config_set)
    if missing or extra:
        raise ValueError(
            f"pools[*].cases union vs tests/config/*.json: missing={missing} extra={extra}"
        )
    first_main_case = pools[1]["cases"][0]
    if first_main_case != PINNED_CASE:
        raise ValueError(
            f"pools[1].cases[0]: pinned case {PINNED_CASE!r} must be the first case of "
            f"{PINNED_POOL_NAME}, got {first_main_case!r}"
        )


def write_pools(out_path: Path, pools: list[dict]) -> None:
    payload = {"version": POOL_VERSION, "mode": POOL_MODE, "pools": pools}
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=2)
        handle.write("\n")


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        config_names, aliases = config_case_index(CONFIG_DIR)
        case_durations, ignored = load_case_durations(
            Path(args.reports), set(config_names), aliases
        )
        if ignored:
            print(
                f"info: ignored {len(ignored)} unknown report entries: "
                f"{sorted(set(ignored))}",
                file=sys.stderr,
            )
        pools = build_partition(case_durations)
        validate_partition(pools, config_names)
        write_pools(Path(args.out), pools)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    counts = {pool["name"]: len(pool["cases"]) for pool in pools}
    total = sum(counts[name] for name in POOL_NAMES)
    summary = ", ".join(f"{name}={counts[name]}" for name in POOL_NAMES)
    print(f"partitioned {total} cases: {summary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
