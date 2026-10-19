"""构建脚本共享常量：路径、示例项目列表、平台库映射。"""

import shutil
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_ROOT / "build"


def resolve_preset(preset: str) -> str:
    """按 ninja 可用性解析实际使用的 CMake preset。

    Ninja 在 Windows 上通常只存在于 VS 开发环境（DevShell），普通终端探测不到，
    此时回退到省略 generator 的 <preset>-portable，由 CMake 自动发现平台默认生成器。
    两条路径共用同一 binaryDir，切换时由 cmake_ops 的 AUTO-CLEAN 重建缓存。
    """
    return preset if shutil.which("ninja") else f"{preset}-portable"


# 多配置生成器把产物再放进一层以配置命名的子目录（Debug/Release）
_MULTI_CONFIG_PREFIXES = ("Visual Studio", "Xcode", "Ninja Multi-Config")


def cached_generator(target: Path) -> str | None:
    """读构建目录 CMakeCache.txt 里的 CMAKE_GENERATOR，无缓存则返回 None。"""
    cache = target / "CMakeCache.txt"
    if not cache.is_file():
        return None
    for line in cache.read_text(encoding="utf-8", errors="ignore").splitlines():
        # 条目形如 CMAKE_GENERATOR:INTERNAL=Ninja，类型在首个冒号之后。
        name, _, rest = line.partition(":")
        if name.strip() == "CMAKE_GENERATOR":
            return rest.partition("=")[2].strip()
    return None


def artifact_dir(preset: str) -> Path:
    """产物所在目录。多配置生成器（Visual Studio / Xcode / Ninja Multi-Config）
    会再加一层配置名子目录，单配置生成器（Unix Makefiles / Ninja）直接是 build/<preset>。

    依据构建目录里实际记录的生成器判断，而非预设名——手动用 --preset debug-portable
    而 PATH 恰好有 ninja 时生成器是单配置的，产物仍在 build/debug。
    """
    base = BUILD_DIR / preset
    generator = cached_generator(base)
    if generator and generator.startswith(_MULTI_CONFIG_PREFIXES):
        return base / ("Debug" if preset == "debug" else "Release")
    return base

DEMO_NAMES: tuple[str, ...] = (
    "2d_dodge_the_creeps",
    "3d_squash_the_creeps",
    "gui_control_gallery",
    "viewport_gui_in_3d",
    "networking_multiplayer_pong",
)

DEMO_DIRS: list[Path] = [PROJECT_ROOT / "demo" / name for name in DEMO_NAMES]

TESTBED_DIR = PROJECT_ROOT / "tests" / "testbed"

# 插件写入的全部工程：demo 是交付物，testbed 只服务 L2 回归，不进 --demos 与打包
DEPLOY_TARGETS: list[Path] = [*DEMO_DIRS, TESTBED_DIR]


def _addon_dir(demo_dir: Path) -> Path:
    return demo_dir / "addons" / "godot-autopilot"


PLATFORM_LIBS: dict[str, str] = {
    "windows": "godot-autopilot.dll",
    "linux": "libgodot-autopilot.so",
    "darwin": "libgodot-autopilot.dylib",
}

PLATFORM_PDB = "godot-autopilot.pdb"

BRIDGE_DIR = PROJECT_ROOT / "csharp_bridge"

# C# 桥源码清单：随插件包分发，缺任一即视为打包失败（Godot 侧按文件名加载）
BRIDGE_SOURCES: tuple[str, ...] = (
    "BridgeCall.cs",
    "BridgeConvert.cs",
    "BridgeEvents.cs",
    "BridgeGetSet.cs",
    "GdaCSharpBridge.cs",
    "GdaCSharpRuntime.cs",
)

ADDON_VERSION = (PROJECT_ROOT / "VERSION").read_text(encoding="utf-8").strip()


def resolve_demos(names: list[str] | None) -> list[Path]:
    """把 demo 名列表解析为工程目录。

    None 表示未传 --demos，返回全部 demo；空列表表示显式传了空的 --demos
    （`--demos` 后不跟名字），返回空列表，即不部署任何 demo。
    """
    if names is None:
        return list(DEMO_DIRS)
    known = {d.name: d for d in DEMO_DIRS}
    unknown = [n for n in names if n not in known]
    if unknown:
        raise ValueError(f"未知的 demo：{', '.join(unknown)}（可选：{', '.join(known)}）")
    return [known[n] for n in names]
