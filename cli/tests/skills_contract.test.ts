import { describe, expect, test } from "bun:test";
import path from "node:path";

import { checkAll, collectTree, readContract } from "../../tests/guard/skills_guard";

const root = path.resolve(import.meta.dir, "..", "..");

describe("skills contract", () => {
  test("skill book satisfies the CLI contract", () => {
    const contract = readContract(root);
    const violations = checkAll(collectTree(path.join(root, "skills")), contract);
    expect(violations).toEqual([]);
  });
});
