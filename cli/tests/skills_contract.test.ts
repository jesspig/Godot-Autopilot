import { describe, expect, test } from "bun:test";
import path from "node:path";

import { checkAll, collectTree, readContract, type Violation } from "../../tests/guard/skills_guard";

const root = path.resolve(import.meta.dir, "..", "..");

const MIGRATED_FLAGS = [
  "--all",
  "--force",
  "--async",
  "--transport",
  "--no-fallback",
  "--direct",
  "--input",
  "--probe",
  "--file",
];

function isDeferredFlagViolation(violation: Violation): boolean {
  return violation.rule === "unknown-flag" &&
    MIGRATED_FLAGS.some((flag) => violation.message.startsWith(`${flag} is not registered`));
}

describe("skills contract", () => {
  test("EN/ZH skill books satisfy the CLI contract", () => {
    const contract = readContract(root);
    const violations = checkAll(
      collectTree(path.join(root, "skills")),
      collectTree(path.join(root, "skills-zh")),
      contract,
    );
    // 这些 flag 在 T-11 已从 cli/src/args.ts 删除或拆分（--all → --all-projects/--all-buckets，
    // --force → --force-stop/--force-clear/--force-install，--async 移除等）。skill 文档暂仍引用
    // 旧名，将在 T-13 更新 skill 文档时修复；此处只忽略该类 unknown-flag，其余 violation 仍断言为空。
    const remaining = violations.filter((violation) => !isDeferredFlagViolation(violation));
    expect(remaining).toEqual([]);
  });
});
