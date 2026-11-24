import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { describe, expect, test } from "bun:test";

import { configureCsharpRuntime } from "../src/commands/install";

function makeProject(content: string): string {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "gda-csharp-runtime-"));
  fs.writeFileSync(path.join(root, "project.godot"), content, "utf8");
  fs.mkdirSync(path.join(root, "addons", "godot-autopilot"), { recursive: true });
  fs.writeFileSync(
    path.join(root, "addons", "godot-autopilot", "GdaCSharpRuntime.cs"),
    "",
    "utf8",
  );
  return root;
}

describe("C# runtime install configuration", () => {
  test("skips a GDScript project", () => {
    const root = makeProject("[application]\n\nconfig/name=\"Game\"\n");
    const result = configureCsharpRuntime(root);
    expect(result.status).toBe("skipped");
    expect(result.reason).toBe("gdscript_project");
    fs.rmSync(root, { recursive: true, force: true });
  });

  test("adds the autoload once to a C# project", () => {
    const root = makeProject(
      "[application]\n\nconfig/name=\"Game\"\n\n[dotnet]\n\nproject/assembly_name=\"Game\"\n",
    );
    const first = configureCsharpRuntime(root);
    expect(first.status).toBe("configured");
    const content = fs.readFileSync(path.join(root, "project.godot"), "utf8");
    expect(content).toContain(
      'GdaCSharpRuntime="*res://addons/godot-autopilot/GdaCSharpRuntime.cs"',
    );
    const second = configureCsharpRuntime(root);
    expect(second.status).toBe("already_configured");
    fs.rmSync(root, { recursive: true, force: true });
  });
});
