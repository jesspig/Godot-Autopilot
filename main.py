#!/usr/bin/env python3
"""Godot-Autopilot 构建入口。

Usage:
    uv run main.py build [--release|--debug]
    uv run main.py deploy [--release|--debug] [--demos 名称...]
    uv run main.py package [--libs-dir <dir>]
    uv run main.py clean
    uv run main.py test [--l1 默认|--l2 [用例名]|--single <用例名>|--all] [--preset debug]
    uv run main.py tui
"""

import argparse
import sys
from pathlib import Path

from scripts import cmake_ops, clean_ops, deploy, package, test_ops
from scripts.config import DEMO_NAMES, resolve_demos


def _cmd_build(args: argparse.Namespace) -> None:
    if args.release and args.debug:
        print("[ERROR] Cannot specify both --release and --debug", flush=True)
        sys.exit(1)

    preset = "release" if args.release else "debug"
    config = "Release" if args.release else "Debug"

    print(f"[BUILD] Config={config}", flush=True)
    if not cmake_ops._configure(preset):
        sys.exit(1)
    if not cmake_ops._build(preset):
        sys.exit(1)

    print(f"\n[DONE] Build complete.", flush=True)


def _cmd_deploy(args: argparse.Namespace) -> None:
    if args.release and args.debug:
        print("[ERROR] Cannot specify both --release and --debug", flush=True)
        sys.exit(1)
    try:
        demos = resolve_demos(args.demos)
    except ValueError as e:
        print(f"[ERROR] {e}", flush=True)
        sys.exit(1)

    preset = "release" if args.release else "debug"

    if args.release:
        clean_ops._clean()

    print(f"\n[DEPLOY] Copying {preset} artifacts to demo/*/addons/ and tests/testbed/addons/...", flush=True)
    deploy._deploy(preset, demos)

    print(f"\n[DONE] Deploy complete.", flush=True)


def _cmd_package(args: argparse.Namespace) -> None:
    print("[PACKAGE] Packaging existing addons...", flush=True)
    package._package_addon(args.libs_dir)


def _cmd_clean(_args: argparse.Namespace) -> None:
    clean_ops._clean()
    print("[DONE] Clean complete.", flush=True)


def _cmd_test(args: argparse.Namespace) -> None:
    if args.single:
        code = test_ops.run_single(args.single, args.preset)
    elif args.all:
        code = test_ops.run_all(args.preset)
    elif args.l2 is not None:
        code = test_ops.run_l2(args.l2, args.preset)
    else:
        code = test_ops.run_l1(args.preset)
    if code != 0:
        sys.exit(code)


def _cmd_tui(_args: argparse.Namespace) -> None:
    from scripts import tui

    tui.run_tui()


def main() -> None:
    parser = argparse.ArgumentParser(description="Godot-Autopilot build & deploy")
    sub = parser.add_subparsers(dest="command", required=True)

    p_build = sub.add_parser("build", help="配置并编译（不部署）")
    p_build.add_argument("--release", action="store_true", help="Release build (default: Debug)")
    p_build.add_argument("--debug", action="store_true", help="Debug build (default)")
    p_build.set_defaults(func=_cmd_build)

    p_deploy = sub.add_parser("deploy", help="部署已构建产物到 demo/ 与 tests/testbed/")
    p_deploy.add_argument("--release", action="store_true", help="部署 Release 产物（先清理，default: Debug）")
    p_deploy.add_argument("--debug", action="store_true", help="部署 Debug 产物（默认）")
    p_deploy.add_argument(
        "--demos",
        nargs="*",
        default=None,
        help=f"仅部署到指定 demo（L2 测试床始终部署，可选：{', '.join(DEMO_NAMES)}）",
    )
    p_deploy.set_defaults(func=_cmd_deploy)

    p_pkg = sub.add_parser("package", help="打包已部署的 addons")
    p_pkg.add_argument("--libs-dir", type=Path, default=None, help="收集三平台库后打包")
    p_pkg.set_defaults(func=_cmd_package)

    p_clean = sub.add_parser("clean", help="清理各 demo 与 L2 测试床的 .godot 与 addons")
    p_clean.set_defaults(func=_cmd_clean)

    p_test = sub.add_parser("test", help="跑测试（默认 L1 快筛）")
    p_test.add_argument("--preset", default="debug", choices=["debug", "release"], help="ctest 预设（默认 debug）")
    p_test.add_argument("--l2", nargs="?", const="", default=None, help="跑 L2（可附用例名单跑，如 --l2 gda_runner_01_scene）")
    p_test.add_argument("--single", default=None, help="单跑用例/守卫（如 --single migration_guard）")
    p_test.add_argument("--all", action="store_true", help="全量 ctest（含 L2，约 12 分钟）")
    p_test.set_defaults(func=_cmd_test)

    p_tui = sub.add_parser("tui", help="交互式构建控制台（需终端）")
    p_tui.set_defaults(func=_cmd_tui)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
