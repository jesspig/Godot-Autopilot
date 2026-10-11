"""Rich + questionary 交互界面：覆盖构建、打包、清理、状态查看全部命令。"""

import platform
import sys
from datetime import datetime
from pathlib import Path

import questionary
from questionary import Choice
from prompt_toolkit.styles import Style
from rich.console import Console
from rich.panel import Panel
from rich.table import Table

from . import cmake_ops, clean_ops, deploy, package, test_ops
from .config import DEPLOY_TARGETS, DEMO_NAMES, PLATFORM_LIBS, _addon_dir, resolve_demos

console = Console()

TUI_STYLE = Style(
    [
        ("qmark", "fg:#5f819d"),
        ("question", "bold"),
        ("answer", "fg:#FF9D00 bold"),
        ("pointer", "fg:#673ab7 bold"),
        ("highlighted", ""),
        ("selected", ""),
        ("separator", ""),
        ("instruction", ""),
        ("text", ""),
        ("disabled", "fg:#858585 italic"),
    ]
)


class _Cancelled(Exception):
    pass


def _ask(prompt):
    answer = prompt.ask()
    if answer is None:
        raise _Cancelled()
    return answer


def _require_tty() -> None:
    if not sys.stdin.isatty():
        console.print(
            "[red]tui 需要交互式终端，请在终端中运行；无终端环境请改用 "
            "main.py build / package / clean 命令。[/red]"
        )
        raise SystemExit(2)


def _fmt_mtime(path: Path) -> str:
    if not path.exists():
        return "—"
    return datetime.fromtimestamp(path.stat().st_mtime).strftime("%m-%d %H:%M")


def deploy_status_rows() -> list[tuple[str, str, str]]:
    system = platform.system().lower()
    lib_name = PLATFORM_LIBS.get(system, "")
    rows = []
    for project_dir in DEPLOY_TARGETS:
        addon_dir = _addon_dir(project_dir)
        gdext = addon_dir / "godot-autopilot.gdextension"
        lib = addon_dir / lib_name if lib_name else None
        rows.append(
            (
                project_dir.name,
                f"✓ {_fmt_mtime(gdext)}" if gdext.exists() else "✗",
                f"✓ {_fmt_mtime(lib)}" if lib is not None and lib.exists() else "✗",
            )
        )
    return rows


def render_status_table() -> Table:
    table = Table(title="插件部署状态")
    table.add_column("工程", style="cyan")
    table.add_column("gdextension")
    table.add_column("平台库")
    for name, gdext, lib in deploy_status_rows():
        table.add_row(name, gdext, lib)
    return table


def _action_build() -> None:
    preset_name = _ask(questionary.select("构建类型", choices=["Debug", "Release"], style=TUI_STYLE))
    preset = "release" if preset_name == "Release" else "debug"
    console.rule(f"[bold]构建 {preset_name}[/bold]")
    if not cmake_ops._configure(preset):
        console.print(Panel("configure 失败", style="red"))
        return
    if not cmake_ops._build(preset):
        console.print(Panel("编译失败", style="red"))
        return
    console.print(Panel(f"构建完成（{preset_name}）", style="green"))


def _action_deploy() -> None:
    preset_name = _ask(questionary.select("部署哪一版产物", choices=["Debug", "Release"], style=TUI_STYLE))
    preset = "release" if preset_name == "Release" else "debug"
    names = _ask(
        questionary.checkbox(
            "部署目标（空格多选，回车确认）",
            choices=[Choice(n, checked=True) for n in DEMO_NAMES],
            style=TUI_STYLE,
        )
    )
    if not names:
        console.print("[yellow]未选择任何 demo，已取消。[/yellow]")
        return
    if preset == "release" and not _ask(
        questionary.confirm("Release 部署前会清理各 demo 的 .godot 与 addons，继续？", default=False, style=TUI_STYLE)
    ):
        console.print("[yellow]已取消。[/yellow]")
        return
    if preset == "release":
        clean_ops._clean()
    deploy._deploy(preset, resolve_demos(names))
    console.print(Panel(f"部署完成（{preset_name}，{len(names)} 个 demo）", style="green"))
    console.print(render_status_table())


def _action_package() -> None:
    use_libs = _ask(questionary.confirm("是否从三平台库目录收集后再打包？", default=False, style=TUI_STYLE))
    libs_dir: Path | None = None
    if use_libs:
        raw = _ask(questionary.path("三平台库目录", style=TUI_STYLE))
        if not raw:
            console.print("[yellow]未输入目录，已取消。[/yellow]")
            return
        libs_dir = Path(raw)
        if not libs_dir.is_dir():
            console.print(f"[red]目录不存在：{libs_dir}[/red]")
            return
    package._package_addon(libs_dir)
    console.print(Panel("打包完成", style="green"))


def _action_clean() -> None:
    if not _ask(questionary.confirm("清理全部 demo 的 .godot 与 addons？", default=False, style=TUI_STYLE)):
        console.print("[yellow]已取消。[/yellow]")
        return
    clean_ops._clean()
    console.print(Panel("清理完成", style="green"))


def _action_test() -> None:
    kind = _ask(
        questionary.select(
            "测试范围",
            choices=["L1 快筛", "L2 全量", "L2 单个", "单跑用例/守卫", "全量"],
            style=TUI_STYLE,
        )
    )
    preset_name = _ask(questionary.select("测试预设", choices=["debug", "release"], style=TUI_STYLE))
    if kind == "L1 快筛":
        code = test_ops.run_l1(preset_name)
    elif kind == "L2 全量":
        console.print("[yellow]L2 全量约 12 分钟，需要 GODOT_PATH 指向引擎。[/yellow]")
        code = test_ops.run_l2(None, preset_name)
    elif kind == "L2 单个":
        name = _ask(questionary.text("用例名（如 gda_runner_01_scene）：", style=TUI_STYLE))
        if not name.strip():
            console.print("[yellow]用例名为空，已取消。[/yellow]")
            return
        code = test_ops.run_l2(name.strip(), preset_name)
    elif kind == "单跑用例/守卫":
        name = _ask(questionary.text("用例名（如 migration_guard）：", style=TUI_STYLE))
        if not name.strip():
            console.print("[yellow]用例名为空，已取消。[/yellow]")
            return
        code = test_ops.run_single(name.strip(), preset_name)
    else:
        code = test_ops.run_all(preset_name)
    console.print(Panel("测试通过" if code == 0 else f"测试失败（退出码 {code}）", style="green" if code == 0 else "red"))


def run_tui() -> None:
    _require_tty()
    console.print(Panel("Godot-Autopilot 构建控制台", style="bold blue"))
    while True:
        try:
            action = _ask(
                questionary.select(
                    "选择操作",
                    choices=["构建", "部署", "打包", "清理", "跑测试", "查看部署状态", "退出"],
                    style=TUI_STYLE,
                )
            )
        except _Cancelled:
            return
        try:
            if action == "构建":
                _action_build()
            elif action == "部署":
                _action_deploy()
            elif action == "打包":
                _action_package()
            elif action == "清理":
                _action_clean()
            elif action == "跑测试":
                _action_test()
            elif action == "查看部署状态":
                console.print(render_status_table())
            else:
                return
        except _Cancelled:
            console.print("[yellow]已取消，返回主菜单。[/yellow]")
