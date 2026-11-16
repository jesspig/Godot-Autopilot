import fs from "node:fs";
import path from "node:path";

import { EXAMPLES_DIR, PROJECT_ROOT } from "../lib/config";
import { errorMessage } from "../lib/cmake";

export type GameJamTheme = {
  id: string;
  theme: string;
  source: string;
  source_url?: string;
  keywords: string[];
  concept_ideas: string[];
};

export type JamOptions = {
  theme: string | null;
  out: string | null;
  list: boolean;
  json: boolean;
  random?: () => number;
};

const THEME_DATA_PATH = path.join(import.meta.dir, "..", "data", "gamejam", "themes.json");
const THEME_TEMPLATE_PATH = path.join(import.meta.dir, "..", "templates", "gamejam", "brief.md");
const THEME_ID_PATTERN = /^[a-z0-9_]+$/u;
const TEMPLATE_PLACEHOLDERS = ["{{theme}}", "{{source}}", "{{keywords}}", "{{concept_ideas}}"] as const;
const TEMPLATE_PLACEHOLDER_SET = new Set<string>(TEMPLATE_PLACEHOLDERS);

class JamUsageError extends Error {}

function writeLine(text = ""): void {
  process.stdout.write(`${text}\n`);
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function readString(value: unknown, label: string): string {
  if (typeof value !== "string" || value.trim() === "") {
    throw new Error(`${label} must be a non-empty string`);
  }
  return value;
}

function readStringArray(value: unknown, label: string): string[] {
  if (!Array.isArray(value) || value.length === 0) {
    throw new Error(`${label} must be a non-empty array`);
  }
  return value.map((item, index) => {
    if (typeof item !== "string" || item.trim() === "") {
      throw new Error(`${label}[${index}] must be a non-empty string`);
    }
    return item;
  });
}

export function validateThemes(value: unknown): GameJamTheme[] {
  if (!isRecord(value) || !Array.isArray(value.themes)) {
    throw new Error("theme data must be an object with a themes array");
  }
  if (value.themes.length === 0) throw new Error("theme data must contain at least one theme");
  const ids = new Set<string>();
  return value.themes.map((item, index) => {
    if (!isRecord(item)) throw new Error(`theme[${index}] must be an object`);
    const id = readString(item.id, `theme[${index}].id`);
    if (!THEME_ID_PATTERN.test(id)) {
      throw new Error(`theme[${index}].id must match ${THEME_ID_PATTERN}`);
    }
    if (ids.has(id)) throw new Error(`duplicate theme id: ${id}`);
    ids.add(id);
    const theme = readString(item.theme, `theme[${index}].theme`);
    if (/[\\\r\n"]/u.test(theme)) {
      throw new Error(`theme[${index}].theme must not contain quotes, backslashes or newlines`);
    }
    const sourceUrl = item.source_url;
    if (sourceUrl !== undefined && (typeof sourceUrl !== "string" || sourceUrl.trim() === "")) {
      throw new Error(`theme[${index}].source_url must be a non-empty string when present`);
    }
    return {
      id,
      theme,
      source: readString(item.source, `theme[${index}].source`),
      ...(typeof sourceUrl === "string" ? { source_url: sourceUrl } : {}),
      keywords: readStringArray(item.keywords, `theme[${index}].keywords`),
      concept_ideas: readStringArray(item.concept_ideas, `theme[${index}].concept_ideas`),
    };
  });
}

export function loadThemes(): GameJamTheme[] {
  let raw: string;
  try {
    raw = fs.readFileSync(THEME_DATA_PATH, "utf8");
  } catch (error) {
    throw new Error(`cannot read theme data ${THEME_DATA_PATH}: ${errorMessage(error)}`);
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch (error) {
    throw new Error(`invalid JSON in ${THEME_DATA_PATH}: ${errorMessage(error)}`);
  }
  try {
    return validateThemes(parsed);
  } catch (error) {
    throw new Error(`${THEME_DATA_PATH}: ${errorMessage(error)}`);
  }
}

function loadTemplate(): string {
  try {
    return fs.readFileSync(THEME_TEMPLATE_PATH, "utf8");
  } catch (error) {
    throw new Error(`cannot read Game Jam template ${THEME_TEMPLATE_PATH}: ${errorMessage(error)}`);
  }
}

export function renderBrief(template: string, theme: GameJamTheme): string {
  for (const placeholder of TEMPLATE_PLACEHOLDERS) {
    if (!template.includes(placeholder)) throw new Error(`template is missing ${placeholder}`);
  }
  const unknown = (template.match(/\{\{[^{}]+\}\}/gu) ?? []).filter(
    (token) => !TEMPLATE_PLACEHOLDER_SET.has(token),
  );
  if (unknown.length > 0) throw new Error(`unknown template placeholder: ${unknown[0]}`);
  const source =
    theme.source_url === undefined ? theme.source : `${theme.source}（${theme.source_url}）`;
  const keywords = theme.keywords.map((keyword) => `- ${keyword}`).join("\n");
  const conceptIdeas = theme.concept_ideas.map((idea) => `- ${idea}`).join("\n");
  return template
    .replaceAll("{{theme}}", theme.theme)
    .replaceAll("{{source}}", source)
    .replaceAll("{{keywords}}", keywords)
    .replaceAll("{{concept_ideas}}", conceptIdeas);
}

export function renderProjectGodot(theme: GameJamTheme): string {
  return [
    "config_version=5",
    "",
    "[application]",
    "",
    `config/name="${theme.theme}"`,
    "",
    "[rendering]",
    "",
    'renderer/rendering_method="gl_compatibility"',
    'renderer/rendering_method.mobile="gl_compatibility"',
    "",
  ].join("\n");
}

export function pickTheme(
  themes: readonly GameJamTheme[],
  usedIds: ReadonlySet<string>,
  random: () => number,
): GameJamTheme {
  if (themes.length === 0) throw new Error("no themes available");
  const unused = themes.filter((theme) => !usedIds.has(theme.id));
  const pool = unused.length > 0 ? unused : themes;
  const index = Math.floor(random() * pool.length);
  return pool[Math.min(Math.max(index, 0), pool.length - 1)];
}

export function nextAvailableDir(baseDir: string, id: string): string {
  const first = path.join(baseDir, id);
  if (!fs.existsSync(first)) return first;
  for (let suffix = 2; ; suffix += 1) {
    const candidate = path.join(baseDir, `${id}-${suffix}`);
    if (!fs.existsSync(candidate)) return candidate;
  }
}

function existingThemeIds(themes: readonly GameJamTheme[]): Set<string> {
  const ids = new Set<string>();
  for (const theme of themes) {
    if (fs.existsSync(path.join(EXAMPLES_DIR, theme.id))) ids.add(theme.id);
  }
  return ids;
}

function findTheme(themes: readonly GameJamTheme[], id: string): GameJamTheme {
  const theme = themes.find((item) => item.id === id);
  if (theme === undefined) {
    const available = themes.map((item) => item.id).join(", ");
    throw new JamUsageError(`unknown theme: ${id} (available: ${available})`);
  }
  return theme;
}

function resolveOutputDir(theme: GameJamTheme, explicitOut: string | null): string {
  if (explicitOut === null) return nextAvailableDir(EXAMPLES_DIR, theme.id);
  const target = path.resolve(process.cwd(), explicitOut);
  if (fs.existsSync(target)) throw new Error(`output directory already exists: ${target}`);
  return target;
}

export function createJamProject(
  theme: GameJamTheme,
  targetDir: string,
  template: string,
): void {
  if (fs.existsSync(targetDir)) {
    throw new Error(`output directory already exists: ${targetDir}`);
  }
  const projectGodot = renderProjectGodot(theme);
  const brief = renderBrief(template, theme);
  fs.mkdirSync(targetDir, { recursive: true });
  try {
    fs.writeFileSync(path.join(targetDir, "project.godot"), projectGodot, "utf8");
    fs.writeFileSync(path.join(targetDir, "GAMEJAM.md"), brief, "utf8");
  } catch (error) {
    fs.rmSync(targetDir, { recursive: true, force: true });
    throw error;
  }
}

function listThemes(themes: readonly GameJamTheme[], json: boolean): number {
  if (json) {
    const entries = themes.map((theme) => ({
      id: theme.id,
      theme: theme.theme,
      source: theme.source,
      ...(theme.source_url === undefined ? {} : { source_url: theme.source_url }),
    }));
    writeLine(JSON.stringify({ ok: true, themes: entries }, null, 2));
    return 0;
  }
  for (const theme of themes) {
    writeLine(`${theme.id}\t${theme.theme}\t${theme.source}`);
  }
  return 0;
}

export function runJam(options: JamOptions): number {
  try {
    const themes = loadThemes();
    if (options.list) return listThemes(themes, options.json);
    const theme =
      options.theme === null
        ? pickTheme(themes, existingThemeIds(themes), options.random ?? Math.random)
        : findTheme(themes, options.theme);
    const targetDir = resolveOutputDir(theme, options.out);
    createJamProject(theme, targetDir, loadTemplate());
    const relativePath = path.relative(PROJECT_ROOT, targetDir).split(path.sep).join("/");
    if (options.json) {
      writeLine(
        JSON.stringify(
          {
            ok: true,
            id: theme.id,
            theme: theme.theme,
            source: theme.source,
            path: targetDir,
            relative_path: relativePath,
          },
          null,
          2,
        ),
      );
    } else {
      writeLine(`[JAM] ${theme.theme} (${theme.source})`);
      writeLine(`[JAM] project: ${relativePath}`);
      writeLine("[JAM] files: project.godot, GAMEJAM.md");
      writeLine(`[JAM] next: bun run gda-dev-tools deploy --project ${relativePath}`);
    }
    return 0;
  } catch (error) {
    console.error(`[ERROR] ${errorMessage(error)}`);
    return error instanceof JamUsageError ? 2 : 1;
  }
}
