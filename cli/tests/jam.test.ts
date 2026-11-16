import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import {
  loadThemes,
  nextAvailableDir,
  pickTheme,
  renderBrief,
  renderProjectGodot,
  runJam,
  validateThemes,
} from "../../devtools/commands/jam";

let tmp: string;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-jam-"));
});

afterEach(() => {
  fs.rmSync(tmp, { recursive: true, force: true });
});

function captureStdout(): { read: () => string; restore: () => void } {
  const original = process.stdout.write;
  let output = "";
  process.stdout.write = ((chunk: unknown) => {
    output += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return {
    read: () => output,
    restore: () => {
      process.stdout.write = original;
    },
  };
}

describe("game jam generator", () => {
  test("loads the preset themes without duplicate ids", () => {
    const themes = loadThemes();
    const ids = themes.map((theme) => theme.id);
    expect(themes.length).toBeGreaterThanOrEqual(4);
    expect(new Set(ids).size).toBe(ids.length);
    for (const id of ["shift", "visibility", "built_to_scale", "stuck_in_a_loop"]) {
      expect(ids).toContain(id);
    }
  });

  test("renders every theme field into the brief", () => {
    const theme = loadThemes().find((item) => item.id === "shift");
    expect(theme).toBeDefined();
    if (theme === undefined) return;
    const template = [
      "# {{theme}}",
      "",
      "> 来源：{{source}}",
      "",
      "{{keywords}}",
      "",
      "{{concept_ideas}}",
      "",
    ].join("\n");
    const brief = renderBrief(template, theme);
    expect(brief).toContain("# SHIFT");
    expect(brief).toContain("GitHub Game Off 2026");
    if (theme.source_url !== undefined) expect(brief).toContain(theme.source_url);
    for (const keyword of theme.keywords) expect(brief).toContain(`- ${keyword}`);
    for (const idea of theme.concept_ideas) expect(brief).toContain(`- ${idea}`);
  });

  test("renders a minimal Godot project", () => {
    const theme = loadThemes().find((item) => item.id === "visibility");
    expect(theme).toBeDefined();
    if (theme === undefined) return;
    const project = renderProjectGodot(theme);
    expect(project).toContain("config_version=5");
    expect(project).toContain('config/name="VISIBILITY"');
    expect(project).toContain('renderer/rendering_method="gl_compatibility"');
  });

  test("creates project.godot and GAMEJAM.md", () => {
    const target = path.join(tmp, "shift");
    const code = runJam({ theme: "shift", out: target, list: false, json: false });
    expect(code).toBe(0);
    expect(fs.existsSync(path.join(target, "project.godot"))).toBe(true);
    expect(fs.existsSync(path.join(target, "GAMEJAM.md"))).toBe(true);
    const brief = fs.readFileSync(path.join(target, "GAMEJAM.md"), "utf8");
    expect(brief).toContain("# SHIFT");
    expect(brief).toContain("## 评审维度");
    expect(brief).toContain("Godot 4.x");
    expect(fs.readFileSync(path.join(target, "project.godot"), "utf8")).toContain(
      'config/name="SHIFT"',
    );
  });

  test("rejects an unknown theme without creating a directory", () => {
    const target = path.join(tmp, "unknown");
    const code = runJam({ theme: "not_a_theme", out: target, list: false, json: false });
    expect(code).toBe(2);
    expect(fs.existsSync(target)).toBe(false);
  });

  test("does not overwrite an explicit output directory", () => {
    const target = path.join(tmp, "existing");
    fs.mkdirSync(target);
    const sentinel = path.join(target, "sentinel.txt");
    fs.writeFileSync(sentinel, "keep", "utf8");
    const code = runJam({ theme: "visibility", out: target, list: false, json: false });
    expect(code).toBe(1);
    expect(fs.readFileSync(sentinel, "utf8")).toBe("keep");
    expect(fs.existsSync(path.join(target, "project.godot"))).toBe(false);
  });

  test("adds a numeric suffix when the default directory is taken", () => {
    const first = nextAvailableDir(tmp, "shift");
    fs.mkdirSync(first);
    expect(first).toBe(path.join(tmp, "shift"));
    expect(nextAvailableDir(tmp, "shift")).toBe(path.join(tmp, "shift-2"));
  });

  test("prefers themes that are not present yet", () => {
    const themes = loadThemes();
    const unused = themes[themes.length - 1];
    if (unused === undefined) return;
    const used = new Set(
      themes.filter((theme) => theme.id !== unused.id).map((theme) => theme.id),
    );
    expect(pickTheme(themes, used, () => 0).id).toBe(unused.id);
  });

  test("validates theme data", () => {
    const valid = {
      themes: [
        {
          id: "probe",
          theme: "PROBE",
          source: "test",
          keywords: ["one"],
          concept_ideas: ["idea"],
        },
      ],
    };
    expect(validateThemes(valid)).toHaveLength(1);
    expect(() => validateThemes({ themes: [] })).toThrow(/at least one/);
    expect(() => validateThemes({ themes: [valid.themes[0], valid.themes[0]] })).toThrow(
      /duplicate/,
    );
    expect(() =>
      validateThemes({ themes: [{ ...valid.themes[0], id: "Bad-Id" }] }),
    ).toThrow(/must match/);
    expect(() =>
      validateThemes({ themes: [{ ...valid.themes[0], keywords: [] }] }),
    ).toThrow(/keywords/);
  });

  test("lists themes as JSON", () => {
    const capture = captureStdout();
    try {
      const code = runJam({ theme: null, out: null, list: true, json: true });
      expect(code).toBe(0);
    } finally {
      capture.restore();
    }
    const parsed = JSON.parse(capture.read()) as {
      ok: boolean;
      themes: Array<{ id: string; theme: string; source: string; source_url?: string }>;
    };
    expect(parsed.ok).toBe(true);
    expect(parsed.themes.length).toBeGreaterThanOrEqual(4);
    const shift = parsed.themes.find((theme) => theme.id === "shift");
    expect(shift?.source_url).toContain("github.blog");
  });

  test("prints a machine-readable generation result", () => {
    const target = path.join(tmp, "json-result");
    const capture = captureStdout();
    try {
      const code = runJam({ theme: "visibility", out: target, list: false, json: true });
      expect(code).toBe(0);
    } finally {
      capture.restore();
    }
    const parsed = JSON.parse(capture.read()) as {
      ok: boolean;
      id: string;
      theme: string;
      source: string;
      path: string;
      relative_path: string;
    };
    expect(parsed.ok).toBe(true);
    expect(parsed.id).toBe("visibility");
    expect(parsed.theme).toBe("VISIBILITY");
    expect(parsed.path).toBe(target);
    expect(fs.existsSync(parsed.path)).toBe(true);
  });
});
