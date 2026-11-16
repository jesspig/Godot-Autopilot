import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import {
  discoverProjectsIn,
  isJamIntake,
  readJamMetadata,
} from "../../devtools/lib/jam_projects";

let tmp: string;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-jam-projects-"));
});

afterEach(() => {
  fs.rmSync(tmp, { recursive: true, force: true });
});

function writeProject(dir: string, kind: "intake" | "project" | null): void {
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "project.godot"), 'config/name="probe"\n', "utf8");
  if (kind !== null) {
    fs.writeFileSync(
      path.join(dir, "jam.json"),
      `${JSON.stringify({ schema_version: 2, kind }, null, 2)}\n`,
      "utf8",
    );
  }
}

describe("jam project discovery", () => {
  test("finds nested projects, skips intakes and tool directories", () => {
    writeProject(path.join(tmp, "theme_a", "project"), "intake");
    writeProject(path.join(tmp, "theme_a", "neon-shift-abcd1234"), "project");
    writeProject(path.join(tmp, "theme_b", "nested", "deep-game-12345678"), "project");
    writeProject(path.join(tmp, "legacy"), null);
    writeProject(path.join(tmp, ".godot", "ignored"), "project");
    writeProject(path.join(tmp, "addons", "ignored"), "project");

    const found = discoverProjectsIn(tmp).map((dir) =>
      path.relative(tmp, dir).split(path.sep).join("/"),
    );
    expect(found).toEqual([
      "legacy",
      "theme_a/neon-shift-abcd1234",
      "theme_b/nested/deep-game-12345678",
    ]);
  });

  test("reads metadata and identifies intakes", () => {
    const intake = path.join(tmp, "theme_a", "project");
    writeProject(intake, "intake");
    expect(readJamMetadata(intake)?.kind).toBe("intake");
    expect(isJamIntake(intake)).toBe(true);

    const project = path.join(tmp, "theme_a", "neon-shift-abcd1234");
    writeProject(project, "project");
    expect(isJamIntake(project)).toBe(false);

    const invalid = path.join(tmp, "theme_a", "invalid");
    fs.mkdirSync(invalid, { recursive: true });
    fs.writeFileSync(path.join(invalid, "jam.json"), "{bad json", "utf8");
    expect(readJamMetadata(invalid)).toBeNull();
    expect(isJamIntake(invalid)).toBe(false);
  });
});
