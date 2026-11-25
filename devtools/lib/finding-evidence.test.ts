import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

import { describe, expect, test } from "bun:test";

const ROOT = path.resolve(import.meta.dir, "..", "..");
const COMMON_DIR = execFileSync("git", ["rev-parse", "--git-common-dir"], {
  cwd: ROOT,
  encoding: "utf8",
}).trim();
const HANDOFF = process.env.GDA_HANDOFF_ROOT ?? path.join(path.dirname(COMMON_DIR), ".handoff");
const STATE_FILE = path.join(HANDOFF, "STATE.md");
const HAS_HANDOFF = fs.existsSync(STATE_FILE);

function stateText(): string {
  return fs.readFileSync(STATE_FILE, "utf8");
}

function findingStatus(id: string): string | null {
  const match = new RegExp(`"${id}":\\s*(\\{[^}]*\\}|"[^"]*")`).exec(stateText());
  if (match === null) return null;
  const literal = match[1];
  if (literal.startsWith("{")) {
    const status = /"status":\s*"([^"]+)"/.exec(literal);
    return status === null ? null : status[1];
  }
  return literal.slice(1, -1);
}

function evidencePath(id: string): string | null {
  const match = new RegExp(`"${id}":\\s*\\{[^}]*"evidence":\\s*"([^"]+)"`).exec(stateText());
  return match === null ? null : match[1];
}

describe.skipIf(!HAS_HANDOFF)("finding evidence matrix", () => {
  test("all 40 findings are closed", () => {
    const open: string[] = [];
    for (let i = 1; i <= 40; i++) {
      const id = `CR-${String(i).padStart(2, "0")}`;
      const status = findingStatus(id);
      if (status !== "verified_fixed") open.push(`${id}=${status}`);
    }
    expect(open).toEqual([]);
  });

  test("every closed finding points to an existing evidence file", () => {
    const missing: string[] = [];
    for (let i = 1; i <= 40; i++) {
      const id = `CR-${String(i).padStart(2, "0")}`;
      const evidence = evidencePath(id);
      if (evidence === null ||
          !fs.existsSync(path.join(path.dirname(HANDOFF), evidence))) {
        missing.push(`${id}=${evidence}`);
      }
    }
    expect(missing).toEqual([]);
  });
});
