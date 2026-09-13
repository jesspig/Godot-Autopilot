#!/usr/bin/env python3
"""构建期把 src/util/skill_templates/ 嵌入为 C++ 生成头（契约 gda_embed_contract.md §3）。"""

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path, PurePath

# Windows 控制台默认 cp1252，中文日志会触发 UnicodeEncodeError；CI 经 PYTHONUTF8 双保险
sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

SKILL_COUNT = 8
# 与 src/util/skill_templates/registry.json 的册数保持同步
NAME_PATTERN = re.compile(r"^[a-z0-9]+(-[a-z0-9]+)*$")
MAX_NAME_LEN = 64
MAX_DESCRIPTION_LEN = 1024
BASE_DELIM = "gda_s"
DELIM_CANDIDATES = [BASE_DELIM] + [f"{BASE_DELIM}{i}" for i in range(1, 10)]


def die(errors):
    for err in errors:
        print(f"[embed_skills] 错误: {err}", file=sys.stderr)
    sys.exit(1)


def read_text(path):
    # newline="" 保留原始换行，保证嵌入内容与 .md 逐字节一致
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read()


def cpp_string_literal(text):
    # JSON 字符串转义与 C++ 字符串字面量兼容，兜底 name/path 中的特殊字符
    return json.dumps(text, ensure_ascii=False)


def validate_and_load(templates_dir):
    errors = []
    registry_path = templates_dir / "registry.json"
    if not registry_path.is_file():
        die([f"registry.json 不存在: {registry_path}"])

    raw = read_text(registry_path)
    try:
        registry = json.loads(raw)
    except json.JSONDecodeError as exc:
        die([f"registry.json 不是合法 JSON: {exc}"])

    if not isinstance(registry, dict) or not isinstance(registry.get("skills"), list):
        die(["registry.json 顶层须为含 skills 数组的对象"])

    skills = registry["skills"]
    if len(skills) != SKILL_COUNT:
        errors.append(f"skills 须恰 {SKILL_COUNT} 条，实际 {len(skills)} 条")

    entries = []
    seen_names = set()
    source_counts = Counter()
    for index, skill in enumerate(skills):
        label = f"skills[{index}]"
        if not isinstance(skill, dict):
            errors.append(f"{label}: 须为对象")
            continue
        name = skill.get("name")
        description = skill.get("description")
        files = skill.get("files")

        if not isinstance(name, str):
            errors.append(f"{label}.name: 须为字符串")
        elif not NAME_PATTERN.fullmatch(name):
            errors.append(f"{label}.name 非法: {name!r}（须匹配 ^[a-z0-9]+(-[a-z0-9]+)*$）")
        elif len(name) > MAX_NAME_LEN:
            errors.append(f"{label}.name 超长: {len(name)} > {MAX_NAME_LEN}")
        elif name in seen_names:
            errors.append(f"{label}.name 重复: {name!r}")
        else:
            seen_names.add(name)

        if not isinstance(description, str) or not description:
            errors.append(f"{label}.description: 须为非空字符串")
        elif len(description) > MAX_DESCRIPTION_LEN:
            errors.append(f"{label}.description 超长: {len(description)} > {MAX_DESCRIPTION_LEN}")

        if not isinstance(files, list) or not files:
            errors.append(f"{label}.files: 须为非空数组")
            continue

        seen_paths = set()
        parsed_files = []
        files_valid = True
        for file_index, file_entry in enumerate(files):
            file_label = f"{label}.files[{file_index}]"
            if (not isinstance(file_entry, dict)
                    or not isinstance(file_entry.get("path"), str)
                    or not isinstance(file_entry.get("source"), str)):
                errors.append(f"{file_label}: 须为含 path/source 字符串字段的对象")
                files_valid = False
                continue
            path = file_entry["path"]
            source = file_entry["source"]

            if file_index == 0:
                if path != "SKILL.md":
                    errors.append(f"{file_label}.path 首项须为 \"SKILL.md\"，实际 {path!r}")
            elif not path.startswith("references/"):
                errors.append(f"{file_label}.path 非首项须以 references/ 开头，实际 {path!r}")
            if path in seen_paths:
                errors.append(f"{file_label}.path 重复: {path!r}")
            seen_paths.add(path)

            if not source.endswith(".md"):
                errors.append(f"{file_label}.source 须为 .md 文件: {source!r}")
            if PurePath(source).name != source:
                errors.append(f"{file_label}.source 须为模板目录内的平铺文件名: {source!r}")
            source_path = templates_dir / source
            if not source_path.is_file():
                errors.append(f"{file_label}.source 不存在: {source!r}")
                files_valid = False
                continue

            source_counts[source] += 1
            body = read_text(source_path)
            if body.startswith("\ufeff"):
                errors.append(f"{file_label}.source 含 UTF-8 BOM: {source!r}")
                files_valid = False
                continue
            if files_valid:
                parsed_files.append((path, body))

        if files_valid and parsed_files:
            entries.append((name, description, parsed_files))

    template_md_files = {p.name for p in templates_dir.glob("*.md")}
    referenced_sources = set(source_counts)
    for source, count in sorted(source_counts.items()):
        if count > 1:
            errors.append(f"source 被引用 {count} 次（每个 .md 恰须一条引用）: {source!r}")
    for orphan in sorted(template_md_files - referenced_sources):
        errors.append(f"模板目录存在未被引用的孤儿 .md: {orphan!r}")
    for dangling in sorted(referenced_sources - template_md_files):
        errors.append(f"source 未对应模板目录中的 .md 文件: {dangling!r}")

    return entries, errors


def choose_delimiter(entries):
    texts = [description for _, description, _ in entries]
    texts += [body for _, _, files in entries for _, body in files]
    for candidate in DELIM_CANDIDATES:
        closer = f"){candidate}\""
        if not any(closer in text for text in texts):
            return candidate
    die([f"定界符候选 {DELIM_CANDIDATES} 全部与待嵌入内容碰撞，请人工处理"])


def render_header(entries, delimiter):
    lines = [
        "// 由 tools/embed_skills.py 生成，勿手改。",
        "// 源：src/util/skill_templates/（registry.json + *.md）",
        "#pragma once",
        "#include <string>",
        "#include <vector>",
        "",
        "namespace godot_autopilot::skill_gen::embedded {",
        "",
        "struct EmbeddedFile {",
        "  const char *path; // \"SKILL.md\" 或 \"references/XXX.md\"",
        "  const char *body;",
        "};",
        "",
        "struct EmbeddedSkill {",
        "  const char *name; // 与目标目录名一致",
        "  const char *description;",
        "  std::vector<EmbeddedFile> files; // files[0] 必为 SKILL.md",
        "};",
        "",
        "inline const std::vector<EmbeddedSkill> &all() {",
        "  static const std::vector<EmbeddedSkill> skills = {",
    ]
    for name, description, files in entries:
        lines.append(f"      {{{cpp_string_literal(name)}, R\"{delimiter}({description}){delimiter}\",")
        # 首个文件紧随开括号（契约示例 {{\"SKILL.md\", ...}}），其余换行缩进对齐
        parts = [
            f"{{{cpp_string_literal(path)}, R\"{delimiter}({body}){delimiter}\"}}"
            for path, body in files
        ]
        lines.append("       {" + ",\n        ".join(parts) + "}},")
    lines += [
        "  };",
        "  return skills;",
        "}",
        "",
        "} // namespace godot_autopilot::skill_gen::embedded",
    ]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description="把 skill 模板目录嵌入为 C++ 生成头")
    parser.add_argument("--templates", required=True, help="skill_templates 目录")
    parser.add_argument("--output", required=True, help="生成头输出路径")
    args = parser.parse_args()

    templates_dir = Path(args.templates)
    output_path = Path(args.output)
    if not templates_dir.is_dir():
        die([f"模板目录不存在: {templates_dir}"])

    entries, errors = validate_and_load(templates_dir)
    if errors:
        die(errors)

    delimiter = choose_delimiter(entries)
    content = render_header(entries, delimiter)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    # UTF-8 无 BOM、\n 换行
    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)

    file_count = sum(len(files) for _, _, files in entries)
    print(f"[embed_skills] 嵌入 {len(entries)} 册 skill / {file_count} 个文件，定界符 {delimiter!r} -> {output_path}")


if __name__ == "__main__":
    main()
