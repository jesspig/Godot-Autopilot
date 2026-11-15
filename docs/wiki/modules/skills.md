---
type: 模块文档
title: 技能卡与 references
description: 1 张入口技能卡与 9 篇 references 的目录结构、词数预算、构建管线、guard 规则、MCP 资源计数与安装清理
tags:
  - 模块
  - 技能
  - references
  - MCP
timestamp: "2026-11-16T10:00:00+08:00"
resource:
  - skills/
  - skills-zh/
  - cli/scripts/build-skills-asset.ts
  - tools/embed_skills.ts
  - tests/guard/skills_guard.ts
---

# 技能卡与 references（`skills/`、`skills-zh/`）

> 审计日期：2026-11-16（未升版，VERSION 保持 0.3.3：10 册自包含技能书收敛为 1 张入口卡 + 9 篇 references；MCP skills 资源 21→12；安装器新增 `--prune`）。

## 目录结构

```text
skills/godot-autopilot/SKILL.md
skills/godot-autopilot/references/setup.md
skills/godot-autopilot/references/tools.md
skills/godot-autopilot/references/scene-system.md
skills/godot-autopilot/references/scripting.md
skills/godot-autopilot/references/resources.md
skills/godot-autopilot/references/runtime.md
skills/godot-autopilot/references/content.md
skills/godot-autopilot/references/csharp.md
skills/godot-autopilot/references/servers.md
skills-zh/godot-autopilot/…（与 EN 同构）
```

- 每个语言目录恰好 1 个 `SKILL.md` + 9 个 `references/*.md`，共 10 个文件。
- `SKILL.md` 的 frontmatter 只有 `name` 与 `description`；references 不带 frontmatter，首行为 `#` 标题。
- EN 与 ZH 的相对路径集合必须完全相等；章节结构保持一致。
- 入口卡承担连接、发现、`gda docs` 规则、9 组任务路由、症状直达、confirm 与变更/观测/验证环；领域细节下沉到 references。

## 词数预算

| 文件 | 预算 | 当前（EN） |
|---|---|---|
| `SKILL.md` | 1,800 | 1,344 |
| `references/setup.md` | 800 | 717 |
| `references/tools.md` | 1,100 | 1,099 |
| `references/scene-system.md` | 1,300 | 1,300 |
| `references/scripting.md` | 900 | 900 |
| `references/resources.md` | 900 | 898 |
| `references/runtime.md` | 1,300 | 1,300 |
| `references/content.md` | 800 | 797 |
| `references/csharp.md` | 800 | 799 |
| `references/servers.md` | 900 | 892 |
| 合计 | 10,600 | 10,046 |

- 硬上限 12,000 词；单文件硬上限 2,000 词。
- 统计口径：按空白分词（`split(/\s+/)`），代码块与正文同权；EN 与 ZH 同预算。
- 新增或扩写 reference 时先改本表，再写内容；超预算优先删示例与重复段落。

## 构建管线

- `bun run build:skills-asset` 执行 [`cli/scripts/build-skills-asset.ts`](../../../cli/scripts/build-skills-asset.ts)，生成 `cli/assets/skills.json`：`{version, files, files_zh, retired}`。
- 构建期 [`cmake/skill_gen.cmake`](../../../cmake/skill_gen.cmake) 调用 [`tools/embed_skills.ts`](../../../tools/embed_skills.ts)，把 `skills/` 嵌入 `build/generated/skill_content_embedded.h`；`SKILL_COUNT=1`。
- `retired` 是退役目录名数组（当前 9 项），供安装器精确清理；它不参与内容嵌入。
- 两个生成物都由脚本产出，禁止手工编辑；改内容后重新运行构建脚本或重新 configure。

## Guard 与测试

- [`tests/guard/skills_guard.ts`](../../../tests/guard/skills_guard.ts)：`BOOKS` 只含 `godot-autopilot`；接受 `SKILL.md` 与 `references/*.md`，拒绝其它路径与非 Markdown；检查禁用词、`gda` 命令/flag 契约、EN/ZH 命令 parity、asset 漂移。
- [`tests/unit/skill_gen_test.cpp`](../../../tests/unit/skill_gen_test.cpp)：`kSkillCount=1`、`kSkillFileCount=10`；每个技能必须有 `SKILL.md`，辅助文件必须在 `references/` 下；backtick 词白名单覆盖 references 使用的状态字段。
- [`tests/unit/skill_resources_test.cpp`](../../../tests/unit/skill_resources_test.cpp)：技能资源计数 12，文件 URI 指向 `references/*.md`。
- 常用验证：`bun tests/guard/skills_guard.ts`、`ctest --preset debug -R "Skill|skills_guard"`、`bun test cli/tests`。

## MCP 资源

技能资源共 **12** 个：

- 1 个目录：`godot://skills`；
- 1 个技能：`godot://skills/godot-autopilot`；
- 10 个文件：`godot://skills/godot-autopilot/SKILL.md` 与 9 篇 references。

`gda skills list|show [--file <rel>] [--zh]` 在无运行实例时读取仓库或内置 asset；`show` 的 `--file` 必须是技能目录内的相对路径。

## 安装与清理

```bash
gda install skills
gda install skills --zh --offline
gda install skills --path "<project>/.agents/skills" --force
gda install skills --prune
```

- 默认目标 `~/.agents/skills`；项目级用 `--path`。
- `--prune` 只删除 `retired` 名单与目标目录现有条目的交集；未加时输出 `stale`，不删除。
- 语言切换需要 `--force`，因为目录名相同、内容不同。

## 维护步骤（改结构时）

1. 同步修改 `skills/` 与 `skills-zh/`，保持相对路径与章节结构一致。
2. 更新上方词数预算与实测值。
3. 若文件数变化：同步 `tools/embed_skills.ts` 的 `SKILL_COUNT`、`tests/unit/skill_gen_test.cpp` 的 `kSkillFileCount`、`tests/unit/skill_resources_test.cpp` 的资源计数；若技能数变化还要改 `skills_guard.ts` 的 `BOOKS`。
4. 若退役目录变化：改 `cli/scripts/build-skills-asset.ts` 的 `RETIRED_SKILLS`，并重新生成 asset。
5. 依次运行 `bun run build:skills-asset`、guard、`ctest --preset debug -R "Skill|skills_guard"`、`bun test cli/tests`。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| `asset-drift` | 改了内容但没重生成 asset | `bun run build:skills-asset` |
| `unexpected-file` | 出现 `references/` 之外的辅助路径 | 移到 `references/*.md` 或删除 |
| `unknown-flag` | 技能正文引用了未注册的 CLI flag | 先在 `cli/src/args.ts` 注册 |
| 安装后旧目录仍在 | 未传 `--prune` | `gda install skills --prune` |
| 资源计数不符 | asset 或嵌入头未重建 | 重新生成 asset 并重新 configure/build |
