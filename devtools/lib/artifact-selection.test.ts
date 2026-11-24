import path from "node:path";

import { describe, expect, test } from "bun:test";

import { isDebugPath, selectUniqueArtifact } from "./artifact-selection";

const LIB = "godot-autopilot.dll";

describe("artifact selection", () => {
  test("selects the single matching artifact", () => {
    const candidate = path.join("build", "release", LIB);
    expect(selectUniqueArtifact([candidate], LIB, "release")).toEqual({
      failure: "none",
      selected: candidate,
    });
  });

  test("rejects ambiguous matches", () => {
    const a = path.join("build", "debug", LIB);
    const b = path.join("build", "release", LIB);
    expect(selectUniqueArtifact([a, b], LIB, "release")).toEqual({
      failure: "ambiguous",
      selected: null,
    });
  });

  test("rejects missing artifacts", () => {
    expect(selectUniqueArtifact([path.join("build", "release", "other.dll")], LIB, "release").failure).toBe("missing");
  });

  test("rejects debug artifacts in release packages", () => {
    expect(isDebugPath(path.join("build", "Debug", LIB))).toBe(true);
    expect(selectUniqueArtifact([path.join("build", "Debug", LIB)], LIB, "release").failure).toBe("wrong_config");
    expect(selectUniqueArtifact([path.join("build", "Debug", LIB)], LIB, "debug").failure).toBe("none");
  });

  test("ignores symbol files so they are never packaged", () => {
    const pdb = path.join("build", "release", "godot-autopilot.pdb");
    const dll = path.join("build", "release", LIB);
    expect(selectUniqueArtifact([pdb, dll], LIB, "release")).toEqual({
      failure: "none",
      selected: dll,
    });
  });
});
