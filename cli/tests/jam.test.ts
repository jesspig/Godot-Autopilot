import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import {
  collectUsedThemeIds,
  loadThemes,
  pickTheme,
  renderBrief,
  renderProjectGodot,
  runJam,
  sampleItems,
  sampleTheme,
  scanJamProjects,
  slugifyProjectName,
  validateThemes,
  type GameJamSample,
  type GameJamTheme,
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

function makeRawTheme(overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    id: "probe_2026_probe",
    theme: "PROBE",
    organizer: "Probe",
    event: "Probe Jam",
    edition: "2026",
    year: 2026,
    source_url: "https://example.com/probe",
    source_title: "Probe Jam 2026 theme",
    source_kind: "official",
    rules_url: "https://example.com/probe/rules",
    verified_at: "2026-11-17",
    tags: ["genre:puzzle"],
    design_constraints: ["游戏必须是双人合作模式"],
    keywords: ["方向一", "方向二", "方向三", "方向四", "方向五"],
    concept_ideas: ["点子一", "点子二", "点子三", "点子四", "点子五"],
    ...overrides,
  };
}

function makeTheme(overrides: Partial<GameJamTheme> = {}): GameJamTheme {
  const raw = makeRawTheme(overrides as Record<string, unknown>);
  return {
    id: String(raw.id),
    theme: String(raw.theme),
    organizer: String(raw.organizer),
    event: String(raw.event),
    edition: String(raw.edition),
    year: Number(raw.year),
    source: `${String(raw.event)} ${String(raw.edition)}`,
    source_url: String(raw.source_url),
    source_title: String(raw.source_title),
    source_kind: "official",
    rules_url: String(raw.rules_url),
    verified_at: String(raw.verified_at),
    tags: raw.tags as string[],
    design_constraints: raw.design_constraints as string[],
    keywords: raw.keywords as string[],
    concept_ideas: raw.concept_ideas as string[],
    ...overrides,
  };
}

function makeSample(theme: GameJamTheme): GameJamSample {
  return {
    draw_id: "20261117T000000000Z-0001",
    drawn_at: "2026-11-17T00:00:00.000Z",
    keywords: theme.keywords.slice(0, 3),
    concept_ideas: theme.concept_ideas.slice(0, 3),
  };
}

function writeIntakeFixture(themeId: string): string {
  const intakeDir = path.join(tmp, themeId, "project");
  fs.mkdirSync(intakeDir, { recursive: true });
  fs.writeFileSync(path.join(intakeDir, "project.godot"), 'config/name="project"\n', "utf8");
  fs.writeFileSync(path.join(intakeDir, "GAMEJAM.md"), "# fixture\n", "utf8");
  fs.writeFileSync(
    path.join(intakeDir, "jam.json"),
    `${JSON.stringify(
      {
        schema_version: 2,
        kind: "intake",
        draw_id: "draw-fixture",
        drawn_at: "2026-11-17T00:00:00.000Z",
        theme_id: themeId,
        sampled: {
          keywords: ["方向一", "方向二", "方向三"],
          concept_ideas: ["点子一", "点子二", "点子三"],
        },
      },
      null,
      2,
    )}\n`,
    "utf8",
  );
  return intakeDir;
}

describe("game jam themes", () => {
  test("loads a 50-theme v2 bank with unique ids and complete fields", () => {
    const themes = loadThemes();
    const ids = themes.map((theme) => theme.id);
    expect(themes.length).toBe(50);
    expect(new Set(ids).size).toBe(ids.length);
    for (const theme of themes) {
      expect(theme.source_url).toMatch(/^https?:\/\//u);
      expect(theme.keywords.length).toBeGreaterThanOrEqual(5);
      expect(theme.concept_ideas.length).toBeGreaterThanOrEqual(5);
      expect(theme.organizer.length).toBeGreaterThan(0);
      expect(theme.year).toBeGreaterThanOrEqual(2000);
    }
  });

  test("validates v2 theme data", () => {
    expect(validateThemes({ schema_version: 2, themes: [makeRawTheme()] })).toHaveLength(1);
    expect(() => validateThemes({ themes: [makeRawTheme()] })).toThrow(/schema_version/);
    expect(() =>
      validateThemes({ schema_version: 2, themes: [{ ...makeRawTheme(), source_url: undefined }] }),
    ).toThrow(/source_url/);
    expect(() =>
      validateThemes({ schema_version: 2, themes: [{ ...makeRawTheme(), source_url: "ftp://x" }] }),
    ).toThrow(/http or https/);
    expect(() =>
      validateThemes({ schema_version: 2, themes: [{ ...makeRawTheme(), year: 1999 }] }),
    ).toThrow(/year/);
    expect(() =>
      validateThemes({ schema_version: 2, themes: [makeRawTheme(), makeRawTheme()] }),
    ).toThrow(/duplicate/);
    expect(() =>
      validateThemes({
        schema_version: 2,
        themes: [{ ...makeRawTheme(), keywords: ["一", "二"] }],
      }),
    ).toThrow(/keywords/);
    expect(() =>
      validateThemes({
        schema_version: 2,
        themes: [{ ...makeRawTheme(), keywords: ["一", "一", "三", "四", "五"] }],
      }),
    ).toThrow(/duplicates/);
    expect(() =>
      validateThemes({
        schema_version: 2,
        themes: [{ ...makeRawTheme(), source_title: "bad\nline" }],
      }),
    ).toThrow(/single line/);
  });
});

describe("brief rendering", () => {
  test("renders only the sampled subset and design constraints", () => {
    const theme = makeTheme();
    const sample = makeSample(theme);
    const template = [
      "# {{theme}}",
      "> {{source}}（<{{source_url}}>） · {{organizer}} · {{year}}",
      "{{keywords}}",
      "{{concept_ideas}}",
      "{{design_constraints_section}}",
      "",
    ].join("\n");
    const brief = renderBrief(template, theme, sample);
    expect(brief).toContain("# PROBE");
    expect(brief).toContain("Probe Jam 2026");
    expect(brief).toContain(theme.source_url);
    expect(brief).toContain("方向一");
    expect(brief).not.toContain("方向五");
    expect(brief).toContain("点子一");
    expect(brief).not.toContain("点子五");
    expect(brief).toContain("## 创意约束");
    expect(brief).toContain("双人合作");
  });

  test("omits the constraints section when the jam has none", () => {
    const theme = makeTheme({ design_constraints: [] });
    const template = "{{theme}}\n{{source}}\n{{source_url}}\n{{organizer}}\n{{year}}\n{{keywords}}\n{{concept_ideas}}\n{{design_constraints_section}}\n";
    expect(renderBrief(template, theme, makeSample(theme))).not.toContain("## 创意约束");
  });

  test("rejects missing and unknown placeholders", () => {
    const theme = makeTheme();
    const sample = makeSample(theme);
    expect(() => renderBrief("{{theme}}", theme, sample)).toThrow(/missing/);
    expect(() =>
      renderBrief(
        "{{theme}}{{source}}{{source_url}}{{organizer}}{{year}}{{keywords}}{{concept_ideas}}{{design_constraints_section}}{{nope}}",
        theme,
        sample,
      ),
    ).toThrow(/unknown/);
  });
});

describe("sampling", () => {
  test("samples 3 to 5 unique items from the source array", () => {
    const items = ["一", "二", "三", "四", "五", "六", "七", "八"];
    const low = sampleItems(items, () => 0);
    const high = sampleItems(items, () => 0.999);
    expect(low).toHaveLength(3);
    expect(high).toHaveLength(5);
    expect(new Set(low).size).toBe(low.length);
    for (const item of low) expect(items).toContain(item);
  });

  test("uses different random sequences to produce different variants", () => {
    const theme = makeTheme();
    const first = sampleTheme(theme, () => 0.1, () => new Date("2026-11-17T00:00:00Z"));
    const second = sampleTheme(theme, () => 0.9, () => new Date("2026-11-17T00:00:01Z"));
    expect(first.draw_id).not.toBe(second.draw_id);
    expect(first.keywords).not.toEqual(second.keywords);
  });

  test("renders a minimal Godot project with the requested name", () => {
    const project = renderProjectGodot("Neon Shift");
    expect(project).toContain("config_version=5");
    expect(project).toContain('config/name="Neon Shift"');
    expect(project).toContain('renderer/rendering_method="gl_compatibility"');
  });

  test("derives ASCII slugs and leaves non-ASCII names to --slug", () => {
    expect(slugifyProjectName("Neon Shift")).toBe("neon-shift");
    expect(slugifyProjectName("Café  Rush")).toBe("cafe-rush");
    expect(slugifyProjectName("霓虹")).toBe("");
  });
});

describe("intake and project creation", () => {
  test("draws an intake with a sampled brief and refreshes it on redraw", () => {
    const theme = loadThemes()[0];
    const firstCapture = captureStdout();
    try {
      expect(
        runJam({
          action: "draw",
          theme: theme.id,
          name: null,
          slug: null,
          out: null,
          json: true,
          examplesDir: tmp,
          random: () => 0.1,
          now: () => new Date("2026-11-17T00:00:00Z"),
        }),
      ).toBe(0);
    } finally {
      firstCapture.restore();
    }
    const first = JSON.parse(firstCapture.read()) as {
      theme_id: string;
      intake_path: string;
      sampled_keyword_count: number;
      sampled_concept_idea_count: number;
    };
    expect(first.theme_id).toBe(theme.id);
    expect(first.sampled_keyword_count).toBeGreaterThanOrEqual(3);
    expect(first.sampled_keyword_count).toBeLessThanOrEqual(5);
    const intakeDir = path.join(tmp, theme.id, "project");
    const briefBefore = fs.readFileSync(path.join(intakeDir, "GAMEJAM.md"), "utf8");
    const metadataBefore = JSON.parse(
      fs.readFileSync(path.join(intakeDir, "jam.json"), "utf8"),
    ) as { kind: string; draw_id: string };
    expect(metadataBefore.kind).toBe("intake");
    expect(fs.readFileSync(path.join(intakeDir, "project.godot"), "utf8")).toContain(
      'config/name="project"',
    );

    const secondCapture = captureStdout();
    try {
      expect(
        runJam({
          action: "draw",
          theme: theme.id,
          name: null,
          slug: null,
          out: null,
          json: true,
          examplesDir: tmp,
          random: () => 0.9,
          now: () => new Date("2026-11-17T00:00:01Z"),
        }),
      ).toBe(0);
    } finally {
      secondCapture.restore();
    }
    const briefAfter = fs.readFileSync(path.join(intakeDir, "GAMEJAM.md"), "utf8");
    const metadataAfter = JSON.parse(
      fs.readFileSync(path.join(intakeDir, "jam.json"), "utf8"),
    ) as { draw_id: string };
    expect(briefAfter).not.toBe(briefBefore);
    expect(metadataAfter.draw_id).not.toBe(metadataBefore.draw_id);
  });

  test("refuses to overwrite a non-intake directory", () => {
    const theme = loadThemes()[0];
    const intakeDir = path.join(tmp, theme.id, "project");
    fs.mkdirSync(intakeDir, { recursive: true });
    const sentinel = path.join(intakeDir, "sentinel.txt");
    fs.writeFileSync(sentinel, "keep", "utf8");
    expect(
      runJam({
        action: "draw",
        theme: theme.id,
        name: null,
        slug: null,
        out: null,
        json: false,
        examplesDir: tmp,
        random: () => 0.1,
        now: () => new Date("2026-11-17T00:00:00Z"),
      }),
    ).toBe(1);
    expect(fs.readFileSync(sentinel, "utf8")).toBe("keep");
  });

  test("creates a named project from the intake without design doc templates", () => {
    const theme = loadThemes()[0];
    const now = () => new Date("2026-11-17T00:00:00Z");
    expect(
      runJam({
        action: "draw",
        theme: theme.id,
        name: null,
        slug: null,
        out: null,
        json: false,
        examplesDir: tmp,
        random: () => 0.1,
        now,
      }),
    ).toBe(0);
    const capture = captureStdout();
    try {
      expect(
        runJam({
          action: "create",
          theme: theme.id,
          name: "Neon Shift",
          slug: null,
          out: null,
          json: true,
          examplesDir: tmp,
          now,
        }),
      ).toBe(0);
    } finally {
      capture.restore();
    }
    const created = JSON.parse(capture.read()) as {
      path: string;
      slug: string;
      hash8: string;
      relative_path: string;
    };
    const expectedHash = crypto
      .createHash("sha256")
      .update(`Neon Shift\n${now().toISOString()}`)
      .digest("hex")
      .slice(0, 8);
    expect(created.slug).toBe("neon-shift");
    expect(created.hash8).toBe(expectedHash);
    expect(created.path).toBe(path.join(tmp, theme.id, `neon-shift-${expectedHash}`));
    expect(fs.readFileSync(path.join(created.path, "project.godot"), "utf8")).toContain(
      'config/name="Neon Shift"',
    );
    const metadata = JSON.parse(
      fs.readFileSync(path.join(created.path, "jam.json"), "utf8"),
    ) as { kind: string; sampled: { keywords: string[] }; name: string };
    expect(metadata.kind).toBe("project");
    expect(metadata.name).toBe("Neon Shift");
    expect(metadata.sampled.keywords.length).toBeGreaterThanOrEqual(3);
    expect(fs.existsSync(path.join(created.path, "docs"))).toBe(false);
  });

  test("retries the hash when the same name is created twice", () => {
    const theme = loadThemes()[0];
    const now = () => new Date("2026-11-17T00:00:00Z");
    runJam({
      action: "draw",
      theme: theme.id,
      name: null,
      slug: null,
      out: null,
      json: false,
      examplesDir: tmp,
      random: () => 0.1,
      now,
    });
    const paths: string[] = [];
    for (let attempt = 0; attempt < 2; attempt += 1) {
      const capture = captureStdout();
      try {
        expect(
          runJam({
            action: "create",
            theme: theme.id,
            name: "Neon Shift",
            slug: null,
            out: null,
            json: true,
            examplesDir: tmp,
            now,
          }),
        ).toBe(0);
      } finally {
        capture.restore();
      }
      paths.push((JSON.parse(capture.read()) as { path: string }).path);
    }
    expect(paths[0]).not.toBe(paths[1]);
  });

  test("requires an ASCII slug for non-ASCII names", () => {
    const theme = loadThemes()[0];
    runJam({
      action: "draw",
      theme: theme.id,
      name: null,
      slug: null,
      out: null,
      json: false,
      examplesDir: tmp,
      random: () => 0.1,
      now: () => new Date("2026-11-17T00:00:00Z"),
    });
    expect(
      runJam({
        action: "create",
        theme: theme.id,
        name: "霓虹",
        slug: null,
        out: null,
        json: false,
        examplesDir: tmp,
        now: () => new Date("2026-11-17T00:00:00Z"),
      }),
    ).toBe(2);
    expect(
      runJam({
        action: "create",
        theme: theme.id,
        name: "霓虹",
        slug: "neon",
        out: null,
        json: false,
        examplesDir: tmp,
        now: () => new Date("2026-11-17T00:00:00Z"),
      }),
    ).toBe(0);
  });

  test("refuses an existing explicit output directory", () => {
    const theme = loadThemes()[0];
    runJam({
      action: "draw",
      theme: theme.id,
      name: null,
      slug: null,
      out: null,
      json: false,
      examplesDir: tmp,
      random: () => 0.1,
      now: () => new Date("2026-11-17T00:00:00Z"),
    });
    const out = path.join(tmp, "custom");
    fs.mkdirSync(out);
    const sentinel = path.join(out, "sentinel.txt");
    fs.writeFileSync(sentinel, "keep", "utf8");
    expect(
      runJam({
        action: "create",
        theme: theme.id,
        name: "Neon Shift",
        slug: null,
        out,
        json: false,
        examplesDir: tmp,
        now: () => new Date("2026-11-17T00:00:00Z"),
      }),
    ).toBe(1);
    expect(fs.readFileSync(sentinel, "utf8")).toBe("keep");
  });

  test("reports a missing intake as a usage error", () => {
    const theme = loadThemes()[0];
    expect(
      runJam({
        action: "create",
        theme: theme.id,
        name: "Neon Shift",
        slug: null,
        out: null,
        json: false,
        examplesDir: tmp,
        now: () => new Date("2026-11-17T00:00:00Z"),
      }),
    ).toBe(2);
  });
});

describe("collection", () => {
  test("scans projects, marks used themes, lists projects and writes an index", () => {
    const theme = loadThemes()[0];
    const now = () => new Date("2026-11-17T00:00:00Z");
    runJam({
      action: "draw",
      theme: theme.id,
      name: null,
      slug: null,
      out: null,
      json: false,
      examplesDir: tmp,
      random: () => 0.1,
      now,
    });
    runJam({
      action: "create",
      theme: theme.id,
      name: "Neon Shift",
      slug: null,
      out: null,
      json: false,
      examplesDir: tmp,
      now,
    });
    const projects = scanJamProjects(tmp);
    expect(projects).toHaveLength(1);
    expect(collectUsedThemeIds(projects).has(theme.id)).toBe(true);
    expect(pickTheme(loadThemes(), collectUsedThemeIds(projects), () => 0).id).not.toBe(
      theme.id,
    );

    const listCapture = captureStdout();
    try {
      expect(
        runJam({
          action: "list",
          theme: null,
          name: null,
          slug: null,
          out: null,
          json: true,
          examplesDir: tmp,
        }),
      ).toBe(0);
    } finally {
      listCapture.restore();
    }
    const listed = JSON.parse(listCapture.read()) as {
      themes: Array<{
        id: string;
        used: boolean;
        source_url: string;
        design_constraints: string[];
      }>;
    };
    expect(listed.themes.find((item) => item.id === theme.id)?.used).toBe(true);
    expect(listed.themes[0]?.source_url).toMatch(/^https?:\/\//u);
    expect(Array.isArray(listed.themes[0]?.design_constraints)).toBe(true);

    const projectsCapture = captureStdout();
    try {
      expect(
        runJam({
          action: "projects",
          theme: null,
          name: null,
          slug: null,
          out: null,
          json: true,
          examplesDir: tmp,
        }),
      ).toBe(0);
    } finally {
      projectsCapture.restore();
    }
    const listedProjects = JSON.parse(projectsCapture.read()) as {
      projects: Array<{ name: string; theme_id: string }>;
    };
    expect(listedProjects.projects).toHaveLength(1);
    expect(listedProjects.projects[0]?.name).toBe("Neon Shift");

    expect(
      runJam({
        action: "index",
        theme: null,
        name: null,
        slug: null,
        out: null,
        json: false,
        examplesDir: tmp,
      }),
    ).toBe(0);
    const index = fs.readFileSync(path.join(tmp, "INDEX.md"), "utf8");
    expect(index).toContain("Neon Shift");
    expect(index).toContain(theme.theme);
  });

  test("creates a project from a fixture intake without loading theme data", () => {
    const themeId = "fixture_2026_theme";
    writeIntakeFixture(themeId);
    const metadata = JSON.parse(
      fs.readFileSync(path.join(tmp, themeId, "project", "jam.json"), "utf8"),
    ) as { kind: string };
    expect(metadata.kind).toBe("intake");
  });
});
