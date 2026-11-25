import fs from "node:fs";
import path from "node:path";

import { describe, expect, test } from "bun:test";

import { collectTree } from "../../../tests/guard/skills_guard";

const root = path.resolve(import.meta.dir, "..", "..", "..");
const EXPECTED_TREE = [
  "godot-autopilot/SKILL.md",
  "godot-autopilot/references/content.md",
  "godot-autopilot/references/csharp.md",
  "godot-autopilot/references/resources.md",
  "godot-autopilot/references/runtime.md",
  "godot-autopilot/references/scene-system.md",
  "godot-autopilot/references/scripting.md",
  "godot-autopilot/references/servers.md",
  "godot-autopilot/references/setup.md",
  "godot-autopilot/references/tools.md",
] as const;

type SkillsAsset = {
  files?: Record<string, unknown>;
  files_zh?: Record<string, unknown>;
};

function treePaths(tree: ReturnType<typeof collectTree>): string[] {
  return Object.keys(tree).sort();
}

describe("Skills tree contract", () => {
  const english = collectTree(path.join(root, "skills"));
  const chinese = collectTree(path.join(root, "skills-zh"));

  test("English tree matches the current snapshot", () => {
    expect(treePaths(english)).toEqual(EXPECTED_TREE);
  });

  test("Chinese tree matches the current snapshot", () => {
    expect(treePaths(chinese)).toEqual(EXPECTED_TREE);
  });

  test("English and Chinese trees stay in lockstep", () => {
    expect(treePaths(chinese)).toEqual(treePaths(english));
  });

  test("embedded skill asset mirrors both trees", () => {
    const asset = JSON.parse(
      fs.readFileSync(path.join(root, "cli", "assets", "skills.json"), "utf8"),
    ) as SkillsAsset;
    expect(Object.keys(asset.files ?? {}).sort()).toEqual(EXPECTED_TREE);
    expect(Object.keys(asset.files_zh ?? {}).sort()).toEqual(EXPECTED_TREE);
  });
});
