"""构建脚本共享常量：路径、示例项目列表、平台库映射。"""

from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_ROOT / "build"

DEMO_NAMES: tuple[str, ...] = (
    "2d_dodge_the_creeps",
    "3d_squash_the_creeps",
    "gui_control_gallery",
    "viewport_gui_in_3d",
    "networking_multiplayer_pong",
)

DEMO_DIRS: list[Path] = [PROJECT_ROOT / "demo" / name for name in DEMO_NAMES]


def _addon_dir(demo_dir: Path) -> Path:
    return demo_dir / "addons" / "godot-autopilot"


PLATFORM_LIBS: dict[str, str] = {
    "windows": "godot-autopilot.dll",
    "linux": "libgodot-autopilot.so",
    "darwin": "libgodot-autopilot.dylib",
}

PLATFORM_PDB = "godot-autopilot.pdb"

ADDON_VERSION = (PROJECT_ROOT / "VERSION").read_text(encoding="utf-8").strip()


def resolve_demos(names: list[str] | None) -> list[Path]:
    if not names:
        return list(DEMO_DIRS)
    known = {d.name: d for d in DEMO_DIRS}
    unknown = [n for n in names if n not in known]
    if unknown:
        raise ValueError(f"未知的 demo：{', '.join(unknown)}（可选：{', '.join(known)}）")
    return [known[n] for n in names]
