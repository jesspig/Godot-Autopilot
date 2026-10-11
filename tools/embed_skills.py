#!/usr/bin/env python3
"""构建期把 skills/ 标准目录布局嵌入为 C++ 生成头。"""

import argparse
import json
import sys
from pathlib import Path
import re

# Windows 控制台默认 cp1252，中文日志会触发 UnicodeEncodeError；CI 经 PYTHONUTF8 双保险
sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

SKILL_COUNT = 9
# 与 skills/ 下的子目录数保持同步
NAME_PATTERN = re.compile(r"^[a-z0-9]+(-[a-z0-9]+)*$")
MAX_NAME_LEN = 64
MAX_DESCRIPTION_LEN = 1024
BASE_DELIM = "gda_s"
DELIM_CANDIDATES = [BASE_DELIM] + [f"{BASE_DELIM}{i}" for i in range(1, 10)]


def die(errors):
    for err in errors:
        print(f"[embed_skills] 错误: {err}", file=sys.stderr)
    sys.exit(1)


def read_bytes(path):
    with open(path, "rb") as f:
        return f.read()


def unescape_quoted(inner, label, errors):
    out = []
    i = 0
    while i < len(inner):
        c = inner[i]
        if c == "\\":
            if i + 1 >= len(inner):
                errors.append(f"{label}: 字符串尾部悬空转义")
                return None
            nxt = inner[i + 1]
            if nxt == '"' or nxt == "\\":
                out.append(nxt)
                i += 2
            else:
                errors.append(f"{label}: 非法转义 \\{nxt}")
                return None
        else:
            out.append(c)
            i += 1
    return "".join(out)


def parse_skill_md(data, label, errors):
    if data.startswith(b"\xef\xbb\xbf"):
        errors.append(f"{label}: 含 UTF-8 BOM")
        return None
    nl = b"\r\n" if data.startswith(b"---\r\n") else b"\n"
    if not data.startswith(b"---" + nl):
        errors.append(f"{label}: 首行须为 ---")
        return None
    lines = data.split(nl)
    try:
        end = lines.index(b"---", 1)
    except ValueError:
        errors.append(f"{label}: frontmatter 缺少结束 ---")
        return None
    fields = {}
    for raw in lines[1:end]:
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            errors.append(f"{label}: frontmatter 非 UTF-8")
            return None
        if not text.strip():
            errors.append(f"{label}: frontmatter 内不允许空行")
            return None
        if ":" not in text:
            errors.append(f"{label}: frontmatter 行缺键: {text!r}")
            return None
        key, _, value = text.partition(":")
        key = key.strip()
        value = value.strip()
        if key in fields:
            errors.append(f"{label}: frontmatter 键重复: {key!r}")
            return None
        if key not in ("name", "description"):
            errors.append(f"{label}: frontmatter 未知键: {key!r}")
            return None
        fields[key] = value
    for key in ("name", "description"):
        if key not in fields:
            errors.append(f"{label}: frontmatter 缺键: {key!r}")
            return None
    name = fields["name"]
    if len(name) >= 2 and name[0] == '"' and name[-1] == '"':
        errors.append(f"{label}: name 须为裸字符串")
        return None
    desc_raw = fields["description"]
    if not (len(desc_raw) >= 2 and desc_raw[0] == '"' and desc_raw[-1] == '"'):
        errors.append(f"{label}: description 须为双引号字符串")
        return None
    description = unescape_quoted(desc_raw[1:-1], label, errors)
    if description is None:
        return None
    rest = lines[end + 1:]
    while rest and rest[0] == b"":
        rest = rest[1:]
    try:
        body = nl.join(rest).decode("utf-8")
    except UnicodeDecodeError:
        errors.append(f"{label}: 正文非 UTF-8")
        return None
    return name, description, body


def validate_and_load(skills_dir):
    errors = []
    if not skills_dir.is_dir():
        die([f"skills 目录不存在: {skills_dir}"])

    entries = []
    seen_names = set()
    referenced = set()
    for child in sorted(skills_dir.iterdir(), key=lambda p: p.name):
        if not child.is_dir():
            errors.append(f"skills/ 根下不允许零散文件: {child.name!r}")
            continue
        name = child.name
        label = f"skills/{name}"
        if not NAME_PATTERN.fullmatch(name):
            errors.append(f"{label}: 目录名非法（须匹配 ^[a-z0-9]+(-[a-z0-9]+)*$）")
        elif len(name) > MAX_NAME_LEN:
            errors.append(f"{label}: 目录名超长: {len(name)} > {MAX_NAME_LEN}")
        elif name in seen_names:
            errors.append(f"{label}: skill 重复")
        else:
            seen_names.add(name)

        skill_md = child / "SKILL.md"
        if not skill_md.is_file():
            errors.append(f"{label}: 缺 SKILL.md")
            continue
        parsed = parse_skill_md(read_bytes(skill_md), label + "/SKILL.md", errors)
        if parsed is None:
            continue
        parsed_name, description, body = parsed
        if parsed_name != name:
            errors.append(f"{label}/SKILL.md: frontmatter name {parsed_name!r} 与目录名不一致")
            continue
        if not description:
            errors.append(f"{label}/SKILL.md: description 须为非空字符串")
            continue
        if len(description) > MAX_DESCRIPTION_LEN:
            errors.append(f"{label}/SKILL.md: description 超长: {len(description)} > {MAX_DESCRIPTION_LEN}")
            continue
        referenced.add(skill_md)

        files = [("SKILL.md", body)]
        files_ok = True
        for sub in sorted(child.iterdir(), key=lambda p: p.name):
            if sub == skill_md:
                continue
            if sub.is_dir():
                if sub.name not in ("references", "scripts"):
                    errors.append(f"{label}: 未知子目录: {sub.name!r}")
                    files_ok = False
                    continue
                for item in sorted(sub.iterdir(), key=lambda p: p.name):
                    if not item.is_file():
                        errors.append(f"{label}/{sub.name}: 不允许嵌套目录: {item.name!r}")
                        files_ok = False
                        continue
                    rel = f"{sub.name}/{item.name}"
                    if sub.name == "references":
                        if not item.name.endswith(".md"):
                            errors.append(f"{label}: {rel} 须为 .md 文件")
                            files_ok = False
                            continue
                    else:
                        if not item.name.endswith(".mjs"):
                            errors.append(f"{label}: {rel} 须为 .mjs 文件")
                            files_ok = False
                            continue
                    raw = read_bytes(item)
                    if raw.startswith(b"\xef\xbb\xbf"):
                        errors.append(f"{label}: {rel} 含 UTF-8 BOM")
                        files_ok = False
                        continue
                    try:
                        files.append((rel, raw.decode("utf-8")))
                    except UnicodeDecodeError:
                        errors.append(f"{label}: {rel} 非 UTF-8")
                        files_ok = False
                        continue
                    referenced.add(item)
            else:
                errors.append(f"{label}: 根下不允许零散文件: {sub.name!r}")
                files_ok = False
        if files_ok:
            entries.append((name, description, files))

    if len(entries) != SKILL_COUNT:
        errors.append(f"skills 须恰 {SKILL_COUNT} 个，实际 {len(entries)} 个")

    for path in sorted(skills_dir.rglob("*")):
        if path.is_file() and path not in referenced:
            try:
                rel = path.relative_to(skills_dir).as_posix()
            except ValueError:
                rel = path.name
            errors.append(f"存在未被引用的孤儿文件: {rel!r}")

    return entries, errors


def cpp_string_literal(text):
    # JSON 字符串转义与 C++ 字符串字面量兼容，兜底 name/path 中的特殊字符
    return json.dumps(text, ensure_ascii=False)


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
        "// 源：skills/（<name>/SKILL.md + references/*.md + scripts/*.mjs）",
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
        # 首个文件紧随开括号（契约示例 {{"SKILL.md", ...}}），其余换行缩进对齐
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
    parser = argparse.ArgumentParser(description="把 skills 标准目录嵌入为 C++ 生成头")
    parser.add_argument("--skills", required=True, help="skills 目录")
    parser.add_argument("--output", required=True, help="生成头输出路径")
    args = parser.parse_args()

    skills_dir = Path(args.skills)
    entries, errors = validate_and_load(skills_dir)
    if errors:
        die(errors)

    delimiter = choose_delimiter(entries)
    content = render_header(entries, delimiter)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    # UTF-8 无 BOM、\n 换行
    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)

    file_count = sum(len(files) for _, _, files in entries)
    print(f"[embed_skills] 嵌入 {len(entries)} 册 skill / {file_count} 个文件，定界符 {delimiter!r} -> {output_path}")


if __name__ == "__main__":
    main()
