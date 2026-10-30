"""CMake 配置与编译。"""

import os
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

from .config import BUILD_DIR, PROJECT_ROOT, cached_generator, resolve_preset

DIAGNOSTIC_PROBE_BYTES = b"gda-temp-probe\n"
CONFIGURE_TAIL_LINES = 40


def _subprocess_env() -> dict[str, str]:
    """把当前解释器所在目录前置到子进程 PATH。

    cmake/skill_gen.cmake 需 find_package(Python3) 定位解释器以嵌入 skills；
    当前解释器目录（uv run 下为 .venv/Scripts）必然含可用的 python，前置到 PATH
    可避免依赖系统 PATH 顺序，也避免命中 Microsoft Store 的 python 存根。
    """
    env = os.environ.copy()
    env["PATH"] = str(Path(sys.executable).parent) + os.pathsep + env.get("PATH", "")
    return env


def _emit_output(chunk: bytes) -> None:
    """把子进程原始输出字节原样写到控制台，不做解码或改写。"""
    buffer = getattr(sys.stdout, "buffer", None)
    if buffer is not None:
        buffer.write(chunk)
        buffer.flush()
        return
    sys.stdout.write(chunk.decode("utf-8", errors="replace"))
    sys.stdout.flush()


def _open_tee_log(log_path: Path | None):
    if log_path is None:
        return None
    try:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        return log_path.open("wb")
    except OSError as exc:
        print(
            f"[WARN] cannot open tee log {log_path}: {exc} (output stays on the console)",
            file=sys.stderr,
            flush=True,
        )
        return None


def _run_inherited(cmd: list[str], cwd: Path | None) -> bool:
    try:
        return subprocess.run(cmd, cwd=cwd or PROJECT_ROOT, env=_subprocess_env()).returncode == 0
    except OSError as exc:
        print(f"[ERROR] failed to start {cmd[0]}: {exc}", file=sys.stderr, flush=True)
        return False


def _run(cmd: list[str], cwd: Path | None = None, log_path: Path | None = None) -> bool:
    """运行子进程；log_path 非空时把输出同时写到控制台与该日志（tee）。

    子进程 stdout/stderr 合并为同一路原始字节，控制台与日志拿到同一份内容；
    日志只额外写入命令行与退出码两条元数据行，不改写子进程原始输出。
    """
    print(f"\n$ {' '.join(cmd)}", flush=True)
    log = _open_tee_log(log_path)
    if log is None:
        return _run_inherited(cmd, cwd)
    with log:
        log.write(f"$ {' '.join(cmd)}\n".encode("utf-8"))
        try:
            process = subprocess.Popen(
                cmd,
                cwd=cwd or PROJECT_ROOT,
                env=_subprocess_env(),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
        except OSError as exc:
            message = f"[ERROR] failed to start {cmd[0]}: {exc}\n"
            sys.stderr.write(message)
            sys.stderr.flush()
            log.write(message.encode("utf-8"))
            log.write(b"[exit failed-to-start]\n")
            return False
        stream = process.stdout
        if stream is None:
            code = process.wait()
            log.write(f"[exit {code}]\n".encode("utf-8"))
            return code == 0
        for chunk in iter(lambda: stream.read1(65536), b""):
            _emit_output(chunk)
            log.write(chunk)
            log.flush()
        code = process.wait()
        log.write(f"[exit {code}]\n".encode("utf-8"))
        return code == 0


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


def _cache_entry(cache: Path, key: str) -> str | None:
    """读 CMakeCache.txt 中某个键的值；条目形如 KEY:TYPE=VALUE。"""
    if not cache.is_file():
        return None
    for line in cache.read_text(encoding="utf-8", errors="ignore").splitlines():
        name, _, rest = line.partition(":")
        if name.strip() == key:
            value = rest.partition("=")[2].strip()
            return value or None
    return None


def _generator_changed(target: Path, want_ninja: bool) -> bool:
    cached = cached_generator(target)
    return cached is not None and (cached == "Ninja") != want_ninja


def _file_tail(path: Path, count: int) -> str:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    rows = text.splitlines()
    return "\n".join(rows[-count:])


def _temp_write_probe() -> tuple[bool, str]:
    try:
        with tempfile.NamedTemporaryFile(
            prefix="gda-fs-probe-", suffix=".tmp", delete=False
        ) as probe:
            probe.write(DIAGNOSTIC_PROBE_BYTES)
            probe_path = Path(probe.name)
    except OSError as exc:
        return False, f"FAILED at create/write: {exc}"
    try:
        probe_path.unlink()
    except OSError as exc:
        return False, f"FAILED at delete: {exc} ({probe_path})"
    return True, f"OK ({probe_path})"


def _cmake_version() -> str:
    try:
        result = subprocess.run(
            ["cmake", "--version"],
            capture_output=True,
            text=True,
            env=_subprocess_env(),
        )
    except OSError as exc:
        return f"unavailable ({exc})"
    output = (result.stdout or result.stderr or "").strip().splitlines()
    return output[0] if output else f"no output (exit {result.returncode})"


def _toolchain_hints(
    preset: str, resolved: str, cache: Path, temp_ok: bool
) -> list[str]:
    hints: list[str] = []
    if resolved == preset:
        hints.append(
            f"retry without ninja on PATH to select the portable preset, "
            f"or run: cmake --preset {preset}-portable"
        )
    else:
        hints.append(
            f"the portable preset {resolved} is already selected; "
            f"if the cache is corrupt, remove build/{preset} and rerun"
        )
    rc_compiler = _cache_entry(cache, "CMAKE_RC_COMPILER")
    if rc_compiler:
        hints.append(
            f"pass an explicit resource compiler: "
            f"cmake --preset {resolved} -DCMAKE_RC_COMPILER=<rc.exe or llvm-rc.exe> "
            f"(cached: {rc_compiler})"
        )
    else:
        hints.append(
            "install the Windows SDK resource compiler or pass "
            "-DCMAKE_RC_COMPILER=<path to rc.exe / llvm-rc.exe>"
        )
    if not temp_ok:
        hints.append(
            "point TMP and TEMP at a writable directory (the temp write probe failed) and rerun"
        )
    return hints


def _preserve_failure_scene(preset: str, target: Path) -> Path | None:
    """把配置失败的现场复制到 build/<preset>.failed-<ts>/，返回该目录。"""
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    scene = BUILD_DIR / f"{preset}.failed-{stamp}"
    suffix = 1
    while scene.exists():
        scene = BUILD_DIR / f"{preset}.failed-{stamp}-{suffix}"
        suffix += 1
    try:
        scene.mkdir(parents=True)
    except OSError as exc:
        print(
            f"[WARN] cannot create the failure scene {scene}: {exc}",
            file=sys.stderr,
            flush=True,
        )
        return None
    for source, relative in (
        (target / "CMakeCache.txt", "CMakeCache.txt"),
        (target / "CMakeFiles" / "CMakeConfigureLog.yaml", "CMakeFiles/CMakeConfigureLog.yaml"),
        (target / "configure.log", "configure.log"),
    ):
        if not source.is_file():
            continue
        destination = scene / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        try:
            shutil.copy2(source, destination)
        except OSError as exc:
            print(f"[WARN] cannot preserve {source}: {exc}", file=sys.stderr, flush=True)
    return scene


def _diagnose_configure_failure(preset: str) -> None:
    """输出配置失败诊断段，并把现场与诊断写入 build/<preset>.failed-<ts>/（IF-04）。"""
    target = BUILD_DIR / preset
    resolved = resolve_preset(preset)
    cache = target / "CMakeCache.txt"
    scene = _preserve_failure_scene(preset, target)
    temp_ok, temp_detail = _temp_write_probe()
    lines = [
        "[DIAGNOSE] CMake configure failed",
        f"  preset: {preset} (resolved: {resolved})",
        f"  cmake --version: {_cmake_version()}",
        f"  TMP={os.environ.get('TMP', '<unset>')}",
        f"  TEMP={os.environ.get('TEMP', '<unset>')}",
        f"  tempfile.gettempdir()={tempfile.gettempdir()}",
        f"  temp write probe: {temp_detail}",
        f"  CMAKE_C_COMPILER={_cache_entry(cache, 'CMAKE_C_COMPILER') or '<unset>'}",
        f"  CMAKE_RC_COMPILER={_cache_entry(cache, 'CMAKE_RC_COMPILER') or '<unset>'}",
        f"  CMakeCache.txt={'present' if cache.is_file() else 'missing'}",
        f"  CMakeConfigureLog.yaml={'present' if (target / 'CMakeFiles' / 'CMakeConfigureLog.yaml').is_file() else 'missing'}",
        f"  failure scene: {scene if scene is not None else '<not created>'}",
    ]
    lines.extend(f"  suggestion: {hint}" for hint in _toolchain_hints(preset, resolved, cache, temp_ok))
    text = "\n".join(lines) + "\n"
    print("\n" + text, end="", flush=True)
    if scene is None:
        print(
            "[WARN] failure scene unavailable; diagnostics were printed to stdout only",
            file=sys.stderr,
            flush=True,
        )
        return
    configure_tail = _file_tail(target / "configure.log", CONFIGURE_TAIL_LINES)
    if configure_tail:
        text += "\n--- configure.log tail ---\n" + configure_tail + "\n"
    try:
        (scene / "diagnostics.log").write_text(text, encoding="utf-8")
    except OSError as exc:
        print(f"[WARN] diagnostics.log write failed: {exc}", file=sys.stderr, flush=True)


def _configure(preset: str) -> bool:
    resolved = resolve_preset(preset)
    target = BUILD_DIR / preset
    want_ninja = resolved == preset
    configure_log = target / "configure.log"
    command = ["cmake", "--preset", resolved]

    # portable preset 省略 generator，CMake 会沿用 cache 里已有的生成器而非重新发现。
    # 若环境在此期间变了（有/无 ninja），cache 会留下不可用的生成器路径，构建必然失败。
    # 故这里主动比对并清空，而不是等 configure 自己报错。
    if _generator_changed(target, want_ninja):
        print(
            f"\n[AUTO-CLEAN] Generator changed: {cached_generator(target)} -> "
            f"{'Ninja' if want_ninja else 'platform default'}",
            flush=True,
        )
        _purge(target)

    if _run(command, log_path=configure_log):
        return True

    _diagnose_configure_failure(preset)

    # 失败重试只在生成器确实变化时有意义：重试前清空缓存让 CMake 重新发现生成器；
    # 生成器未变时清缓存只会重复同一失败，直接给出诊断并退出。
    if _generator_changed(target, want_ninja):
        print(
            "\n[AUTO-CLEAN] Generator still mismatched; purging and retrying configure once",
            flush=True,
        )
        _purge(target)
        return _run(command, log_path=configure_log)

    print(
        "\n[AUTO-CLEAN] Skipped: configure failure reason unchanged (原因未变，跳过无意义重试).",
        flush=True,
    )
    return False


def _build(preset: str) -> bool:
    return _run(
        ["cmake", "--build", "--preset", resolve_preset(preset)],
        log_path=BUILD_DIR / preset / "build.log",
    )
