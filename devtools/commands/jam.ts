import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import { EXAMPLES_DIR, PROJECT_ROOT } from "../lib/config";
import { errorMessage } from "../lib/cmake";
import {
  JAM_METADATA_FILE,
  discoverProjectsIn,
  isJamIntake,
  readJamMetadata,
} from "../lib/jam_projects";

export type SourceKind = "official" | "archive" | "community";

export type GameJamTheme = {
  id: string;
  theme: string;
  organizer: string;
  event: string;
  edition: string;
  year: number;
  source: string;
  source_url: string;
  source_title?: string;
  source_kind?: SourceKind;
  rules_url?: string;
  verified_at?: string;
  tags: string[];
  design_constraints: string[];
  keywords: string[];
  concept_ideas: string[];
};

export type GameJamSample = {
  draw_id: string;
  drawn_at: string;
  keywords: string[];
  concept_ideas: string[];
};

export type JamProjectRecord = {
  name: string;
  slug: string;
  hash8: string;
  theme_id: string;
  theme: string;
  organizer: string;
  event: string;
  edition: string;
  year: number;
  source: string;
  source_url: string;
  source_title?: string;
  design_constraints: string[];
  tags: string[];
  sampled: {
    keywords: string[];
    concept_ideas: string[];
  };
  drawn_at: string;
  created_at: string;
  path: string;
  relative_path: string;
};

export type JamOptions = {
  action: "draw" | "create" | "list" | "projects" | "index" | "e2e";
  theme: string | null;
  name: string | null;
  slug: string | null;
  out: string | null;
  json: boolean;
  reset?: boolean;
  random?: () => number;
  now?: () => Date;
  examplesDir?: string;
};

type CreateProjectOptions = {
  examplesDir: string;
  theme: GameJamTheme;
  name: string;
  slug: string | null;
  out: string | null;
  now: () => Date;
};

type CreatedProject = {
  path: string;
  relativePath: string;
  slug: string;
  hash8: string;
  createdAt: string;
};

const THEME_DATA_PATH = path.join(import.meta.dir, "..", "data", "gamejam", "themes.json");
const BRIEF_TEMPLATE_PATH = path.join(import.meta.dir, "..", "templates", "gamejam", "brief.md");
const THEME_ID_PATTERN = /^[a-z0-9_]+$/u;
const SLUG_PATTERN = /^[a-z0-9]+(?:-[a-z0-9]+)*$/u;
const VERIFIED_AT_PATTERN = /^\d{4}-\d{2}-\d{2}$/u;
const THEME_SCHEMA_VERSION = 2;
const MIN_THEME_ITEMS = 5;
const MIN_SAMPLE_ITEMS = 3;
const MAX_SAMPLE_ITEMS = 5;
const MAX_SLUG_LENGTH = 40;
const MAX_PROJECT_NAME_LENGTH = 80;
const INTAKE_DIRECTORY = "project";
const INTAKE_PROJECT_NAME = "project";
const E2E_DIRECTORY = "e2e";
const E2E_PROJECT_NAME = "e2e";
const PROJECT_FILE = "project.godot";
const BRIEF_FILE = "GAMEJAM.md";
const BRIEF_REQUIRED_PLACEHOLDERS = ["theme", "source", "keywords", "concept_ideas"] as const;
const SOURCE_KINDS = new Set<SourceKind>(["official", "archive", "community"]);

class JamUsageError extends Error {}

function writeLine(text = ""): void {
  process.stdout.write(`${text}\n`);
}

function writeJson(value: unknown): void {
  writeLine(JSON.stringify(value, null, 2));
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function assertSingleLine(value: string, label: string): string {
  if (/[\r\n]/u.test(value)) throw new Error(`${label} must be a single line`);
  return value;
}

function readString(value: unknown, label: string): string {
  if (typeof value !== "string" || value.trim() === "") {
    throw new Error(`${label} must be a non-empty string`);
  }
  return assertSingleLine(value, label);
}

function readOptionalString(value: unknown, label: string): string | undefined {
  if (value === undefined) return undefined;
  return readString(value, label);
}

function readUrl(value: unknown, label: string): string {
  const text = readString(value, label);
  let parsed: URL;
  try {
    parsed = new URL(text);
  } catch {
    throw new Error(`${label} must be a valid URL`);
  }
  if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
    throw new Error(`${label} must use http or https`);
  }
  return text;
}

function readOptionalUrl(value: unknown, label: string): string | undefined {
  if (value === undefined) return undefined;
  return readUrl(value, label);
}

function readYear(value: unknown, label: string): number {
  if (typeof value !== "number" || !Number.isInteger(value) || value < 2000 || value > 2100) {
    throw new Error(`${label} must be an integer between 2000 and 2100`);
  }
  return value;
}

function readSourceKind(value: unknown, label: string): SourceKind | undefined {
  if (value === undefined) return undefined;
  if (typeof value !== "string" || !SOURCE_KINDS.has(value as SourceKind)) {
    throw new Error(`${label} must be one of official, archive, community`);
  }
  return value as SourceKind;
}

function readVerifiedAt(value: unknown, label: string): string | undefined {
  if (value === undefined) return undefined;
  const text = readString(value, label);
  if (!VERIFIED_AT_PATTERN.test(text)) throw new Error(`${label} must match YYYY-MM-DD`);
  return text;
}

function readStringArray(value: unknown, label: string, minItems = 1): string[] {
  if (!Array.isArray(value)) throw new Error(`${label} must be an array`);
  if (value.length < minItems) {
    throw new Error(`${label} must contain at least ${minItems} items`);
  }
  const result = value.map((item, index) => {
    if (typeof item !== "string" || item.trim() === "") {
      throw new Error(`${label}[${index}] must be a non-empty string`);
    }
    return assertSingleLine(item, `${label}[${index}]`);
  });
  if (new Set(result).size !== result.length) {
    throw new Error(`${label} must not contain duplicates`);
  }
  return result;
}

export function validateThemes(value: unknown): GameJamTheme[] {
  if (!isRecord(value)) throw new Error("theme data must be an object");
  if (value.schema_version !== THEME_SCHEMA_VERSION) {
    throw new Error(`schema_version must be ${THEME_SCHEMA_VERSION}`);
  }
  if (!Array.isArray(value.themes)) throw new Error("theme data must contain a themes array");
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
    const organizer = readString(item.organizer, `theme[${index}].organizer`);
    const event = readString(item.event, `theme[${index}].event`);
    const edition = readString(item.edition, `theme[${index}].edition`);
    const year = readYear(item.year, `theme[${index}].year`);
    const sourceUrl = readUrl(item.source_url, `theme[${index}].source_url`);
    const sourceTitle = readOptionalString(item.source_title, `theme[${index}].source_title`);
    const sourceKind = readSourceKind(item.source_kind, `theme[${index}].source_kind`);
    const rulesUrl = readOptionalUrl(item.rules_url, `theme[${index}].rules_url`);
    const verifiedAt = readVerifiedAt(item.verified_at, `theme[${index}].verified_at`);
    const tags = readStringArray(item.tags ?? [], `theme[${index}].tags`, 0);
    const designConstraints = readStringArray(
      item.design_constraints ?? [],
      `theme[${index}].design_constraints`,
      0,
    );
    const keywords = readStringArray(
      item.keywords,
      `theme[${index}].keywords`,
      MIN_THEME_ITEMS,
    );
    const conceptIdeas = readStringArray(
      item.concept_ideas,
      `theme[${index}].concept_ideas`,
      MIN_THEME_ITEMS,
    );
    return {
      id,
      theme,
      organizer,
      event,
      edition,
      year,
      source: `${event} ${edition}`,
      source_url: sourceUrl,
      ...(sourceTitle === undefined ? {} : { source_title: sourceTitle }),
      ...(sourceKind === undefined ? {} : { source_kind: sourceKind }),
      ...(rulesUrl === undefined ? {} : { rules_url: rulesUrl }),
      ...(verifiedAt === undefined ? {} : { verified_at: verifiedAt }),
      tags,
      design_constraints: designConstraints,
      keywords,
      concept_ideas: conceptIdeas,
    };
  });
}

function loadTextFile(filePath: string, label: string): string {
  try {
    return fs.readFileSync(filePath, "utf8");
  } catch (error) {
    throw new Error(`cannot read ${label} ${filePath}: ${errorMessage(error)}`);
  }
}

export function loadThemes(): GameJamTheme[] {
  const raw = loadTextFile(THEME_DATA_PATH, "theme data");
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

function renderPlaceholderTemplate(
  template: string,
  values: Readonly<Record<string, string>>,
  required: readonly string[],
): string {
  for (const key of required) {
    if (!template.includes(`{{${key}}}`)) {
      throw new Error(`template is missing {{${key}}}`);
    }
  }
  return template.replace(/\{\{([a-z_]+)\}\}/gu, (token, key: string) => {
    const value = values[key];
    if (value === undefined) throw new Error(`unknown template placeholder: ${token}`);
    return value;
  });
}

function renderDesignConstraintsSection(theme: GameJamTheme): string {
  if (theme.design_constraints.length === 0) return "";
  const lines = [
    "## 创意约束",
    "",
    "本赛事对作品的设计或内容有以下要求，开发时必须遵守：",
    "",
  ];
  for (const constraint of theme.design_constraints) lines.push(`- ${constraint}`);
  lines.push("");
  return lines.join("\n");
}

export function renderBrief(
  template: string,
  theme: GameJamTheme,
  sample: GameJamSample,
): string {
  const values: Record<string, string> = {
    theme: theme.theme,
    source: theme.source,
    source_url: theme.source_url,
    organizer: theme.organizer,
    year: String(theme.year),
    keywords: sample.keywords.map((keyword) => `- ${keyword}`).join("\n"),
    concept_ideas: sample.concept_ideas.map((idea) => `- ${idea}`).join("\n"),
    design_constraints_section: renderDesignConstraintsSection(theme),
  };
  return renderPlaceholderTemplate(template, values, BRIEF_REQUIRED_PLACEHOLDERS);
}

export function renderProjectGodot(projectName: string): string {
  return [
    "config_version=5",
    "",
    "[application]",
    "",
    `config/name="${projectName}"`,
    "",
    "[rendering]",
    "",
    'renderer/rendering_method="gl_compatibility"',
    'renderer/rendering_method.mobile="gl_compatibility"',
    "",
  ].join("\n");
}

export function sampleItems(
  items: readonly string[],
  random: () => number,
  minItems = MIN_SAMPLE_ITEMS,
  maxItems = MAX_SAMPLE_ITEMS,
): string[] {
  if (items.length === 0) throw new Error("cannot sample an empty array");
  const upper = Math.min(maxItems, items.length);
  const lower = Math.min(minItems, upper);
  const count = Math.min(lower + Math.floor(random() * (upper - lower + 1)), upper);
  const pool = [...items];
  for (let index = pool.length - 1; index > 0; index -= 1) {
    const swapIndex = Math.min(Math.floor(random() * (index + 1)), index);
    [pool[index], pool[swapIndex]] = [pool[swapIndex], pool[index]];
  }
  return pool.slice(0, count);
}

function createDrawId(timestamp: string, random: () => number): string {
  const compact = timestamp.replace(/[-:.TZ]/gu, "");
  const suffix = Math.min(Math.floor(random() * 0x10000), 0xffff)
    .toString(16)
    .padStart(4, "0");
  return `${compact}-${suffix}`;
}

export function sampleTheme(
  theme: GameJamTheme,
  random: () => number,
  now: () => Date,
): GameJamSample {
  const drawnAt = now().toISOString();
  return {
    draw_id: createDrawId(drawnAt, random),
    drawn_at: drawnAt,
    keywords: sampleItems(theme.keywords, random),
    concept_ideas: sampleItems(theme.concept_ideas, random),
  };
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

export function slugifyProjectName(name: string): string {
  return name
    .normalize("NFKD")
    .replace(/[\u0300-\u036f]/gu, "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/gu, "-")
    .replace(/-+/gu, "-")
    .replace(/^-|-$/gu, "")
    .slice(0, MAX_SLUG_LENGTH)
    .replace(/-$/gu, "");
}

function validateProjectName(name: string): string {
  const trimmed = name.trim();
  if (trimmed === "") throw new JamUsageError("project name must not be empty");
  if (trimmed.length > MAX_PROJECT_NAME_LENGTH) {
    throw new JamUsageError(
      `project name must be at most ${MAX_PROJECT_NAME_LENGTH} characters`,
    );
  }
  if (/[\\\r\n"]/u.test(trimmed)) {
    throw new JamUsageError("project name must not contain quotes, backslashes or newlines");
  }
  return trimmed;
}

function validateSlug(slug: string): string {
  if (slug.length > MAX_SLUG_LENGTH || !SLUG_PATTERN.test(slug)) {
    throw new JamUsageError(
      `slug must match ${SLUG_PATTERN} and be at most ${MAX_SLUG_LENGTH} characters`,
    );
  }
  return slug;
}

function hashProjectName(name: string, timestamp: string, attempt: number): string {
  const payload =
    attempt === 0 ? `${name}\n${timestamp}` : `${name}\n${timestamp}\n${attempt}`;
  return crypto.createHash("sha256").update(payload).digest("hex").slice(0, 8);
}

function toProjectRelativePath(target: string): string {
  return path.relative(PROJECT_ROOT, target).split(path.sep).join("/");
}

function writeJsonFile(filePath: string, value: unknown): void {
  fs.writeFileSync(filePath, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function intakeDirFor(examplesDir: string, themeId: string): string {
  return path.join(examplesDir, themeId, INTAKE_DIRECTORY);
}

function readSample(metadata: Record<string, unknown>, label: string): GameJamSample {
  if (!isRecord(metadata.sampled)) throw new Error(`${label}.sampled must be an object`);
  return {
    draw_id: readString(metadata.draw_id, `${label}.draw_id`),
    drawn_at: readString(metadata.drawn_at, `${label}.drawn_at`),
    keywords: readStringArray(metadata.sampled.keywords, `${label}.sampled.keywords`, 1),
    concept_ideas: readStringArray(
      metadata.sampled.concept_ideas,
      `${label}.sampled.concept_ideas`,
      1,
    ),
  };
}

export function writeIntake(
  examplesDir: string,
  theme: GameJamTheme,
  sample: GameJamSample,
  briefTemplate: string,
): string {
  const intakeDir = intakeDirFor(examplesDir, theme.id);
  const existed = fs.existsSync(intakeDir);
  if (existed && !isJamIntake(intakeDir)) {
    throw new Error(`refusing to overwrite non-intake directory: ${intakeDir}`);
  }
  fs.mkdirSync(intakeDir, { recursive: true });
  try {
    fs.writeFileSync(
      path.join(intakeDir, PROJECT_FILE),
      renderProjectGodot(INTAKE_PROJECT_NAME),
      "utf8",
    );
    fs.writeFileSync(
      path.join(intakeDir, BRIEF_FILE),
      renderBrief(briefTemplate, theme, sample),
      "utf8",
    );
    writeJsonFile(path.join(intakeDir, JAM_METADATA_FILE), {
      schema_version: THEME_SCHEMA_VERSION,
      kind: "intake",
      draw_id: sample.draw_id,
      drawn_at: sample.drawn_at,
      theme_id: theme.id,
      theme: theme.theme,
      organizer: theme.organizer,
      event: theme.event,
      edition: theme.edition,
      year: theme.year,
      source: theme.source,
      source_url: theme.source_url,
      ...(theme.source_title === undefined ? {} : { source_title: theme.source_title }),
      ...(theme.rules_url === undefined ? {} : { rules_url: theme.rules_url }),
      tags: theme.tags,
      design_constraints: theme.design_constraints,
      sampled: {
        keywords: sample.keywords,
        concept_ideas: sample.concept_ideas,
      },
    });
  } catch (error) {
    if (!existed) fs.rmSync(intakeDir, { recursive: true, force: true });
    throw error;
  }
  return intakeDir;
}

export function createProjectFromIntake(options: CreateProjectOptions): CreatedProject {
  const intakeDir = intakeDirFor(options.examplesDir, options.theme.id);
  const metadata = readJamMetadata(intakeDir);
  if (metadata === null || metadata.kind !== "intake") {
    throw new JamUsageError(
      `missing intake project for ${options.theme.id}; run jam first: ${intakeDir}`,
    );
  }
  if (readString(metadata.theme_id, `${intakeDir}.theme_id`) !== options.theme.id) {
    throw new JamUsageError(`intake theme does not match ${options.theme.id}: ${intakeDir}`);
  }
  const sample = readSample(metadata, intakeDir);
  const projectName = validateProjectName(options.name);
  const slug =
    options.slug === null
      ? slugifyProjectName(projectName)
      : validateSlug(options.slug);
  if (slug === "") {
    throw new JamUsageError(
      "cannot derive an ASCII slug from the project name; pass --slug <ascii-slug>",
    );
  }
  const createdAt = options.now().toISOString();
  let hash8 = "";
  let targetDir = "";
  if (options.out !== null) {
    targetDir = path.resolve(process.cwd(), options.out);
    if (fs.existsSync(targetDir)) {
      throw new Error(`output directory already exists: ${targetDir}`);
    }
    hash8 = hashProjectName(projectName, createdAt, 0);
  } else {
    for (let attempt = 0; attempt < 10; attempt += 1) {
      hash8 = hashProjectName(projectName, createdAt, attempt);
      targetDir = path.join(options.examplesDir, options.theme.id, `${slug}-${hash8}`);
      if (!fs.existsSync(targetDir)) break;
      if (attempt === 9) throw new Error(`cannot allocate a project directory for ${slug}`);
    }
  }
  const brief = loadTextFile(path.join(intakeDir, BRIEF_FILE), "Game Jam brief");
  fs.mkdirSync(targetDir, { recursive: true });
  try {
    fs.writeFileSync(path.join(targetDir, PROJECT_FILE), renderProjectGodot(projectName), "utf8");
    fs.writeFileSync(path.join(targetDir, BRIEF_FILE), brief, "utf8");
    writeJsonFile(path.join(targetDir, JAM_METADATA_FILE), {
      schema_version: THEME_SCHEMA_VERSION,
      kind: "project",
      name: projectName,
      slug,
      hash8,
      theme_id: options.theme.id,
      theme: options.theme.theme,
      organizer: options.theme.organizer,
      event: options.theme.event,
      edition: options.theme.edition,
      year: options.theme.year,
      source: options.theme.source,
      source_url: options.theme.source_url,
      ...(options.theme.source_title === undefined
        ? {}
        : { source_title: options.theme.source_title }),
      ...(options.theme.rules_url === undefined
        ? {}
        : { rules_url: options.theme.rules_url }),
      tags: options.theme.tags,
      design_constraints: options.theme.design_constraints,
      sampled: {
        keywords: sample.keywords,
        concept_ideas: sample.concept_ideas,
      },
      draw_id: sample.draw_id,
      drawn_at: sample.drawn_at,
      created_at: createdAt,
    });
  } catch (error) {
    fs.rmSync(targetDir, { recursive: true, force: true });
    throw error;
  }
  return {
    path: targetDir,
    relativePath: toProjectRelativePath(targetDir),
    slug,
    hash8,
    createdAt,
  };
}

function readProjectRecord(dir: string): JamProjectRecord | null {
  const metadata = readJamMetadata(dir);
  if (metadata === null || metadata.kind !== "project") return null;
  try {
    const name = readString(metadata.name, `${dir}.name`);
    const slug = readString(metadata.slug, `${dir}.slug`);
    const hash8 = readString(metadata.hash8, `${dir}.hash8`);
    const themeId = readString(metadata.theme_id, `${dir}.theme_id`);
    const theme = readString(metadata.theme, `${dir}.theme`);
    const organizer = readString(metadata.organizer, `${dir}.organizer`);
    const event = readString(metadata.event, `${dir}.event`);
    const edition = readString(metadata.edition, `${dir}.edition`);
    const year = readYear(metadata.year, `${dir}.year`);
    const source = readString(metadata.source, `${dir}.source`);
    const sourceUrl = readUrl(metadata.source_url, `${dir}.source_url`);
    const sourceTitle = readOptionalString(metadata.source_title, `${dir}.source_title`);
    const designConstraints = readStringArray(
      metadata.design_constraints ?? [],
      `${dir}.design_constraints`,
      0,
    );
    const tags = readStringArray(metadata.tags ?? [], `${dir}.tags`, 0);
    const sampled = readSample(metadata, dir);
    const drawnAt = readString(metadata.drawn_at, `${dir}.drawn_at`);
    const createdAt = readString(metadata.created_at, `${dir}.created_at`);
    return {
      name,
      slug,
      hash8,
      theme_id: themeId,
      theme,
      organizer,
      event,
      edition,
      year,
      source,
      source_url: sourceUrl,
      ...(sourceTitle === undefined ? {} : { source_title: sourceTitle }),
      design_constraints: designConstraints,
      tags,
      sampled: {
        keywords: sampled.keywords,
        concept_ideas: sampled.concept_ideas,
      },
      drawn_at: drawnAt,
      created_at: createdAt,
      path: dir,
      relative_path: toProjectRelativePath(dir),
    };
  } catch {
    return null;
  }
}

export function scanJamProjects(examplesDir: string): JamProjectRecord[] {
  return discoverProjectsIn(examplesDir)
    .map((dir) => readProjectRecord(dir))
    .filter((record): record is JamProjectRecord => record !== null)
    .sort((left, right) => right.created_at.localeCompare(left.created_at));
}

export function collectUsedThemeIds(
  projects: readonly Pick<JamProjectRecord, "theme_id">[],
): Set<string> {
  return new Set(projects.map((project) => project.theme_id));
}

function findTheme(themes: readonly GameJamTheme[], input: string): GameJamTheme {
  const exact = themes.find((theme) => theme.id === input);
  if (exact !== undefined) return exact;
  const matches = themes.filter((theme) => theme.theme.toLowerCase() === input.toLowerCase());
  if (matches.length === 1) return matches[0];
  if (matches.length > 1) {
    throw new JamUsageError(
      `ambiguous theme: ${input} (${matches.map((theme) => theme.id).join(", ")})`,
    );
  }
  const available = themes.map((theme) => theme.id).join(", ");
  throw new JamUsageError(`unknown theme: ${input} (available: ${available})`);
}

function listThemes(
  themes: readonly GameJamTheme[],
  usedIds: ReadonlySet<string>,
  json: boolean,
): void {
  if (json) {
    writeJson({
      ok: true,
      themes: themes.map((theme) => ({
        id: theme.id,
        theme: theme.theme,
        organizer: theme.organizer,
        event: theme.event,
        edition: theme.edition,
        year: theme.year,
        source: theme.source,
        source_url: theme.source_url,
        ...(theme.source_title === undefined ? {} : { source_title: theme.source_title }),
        ...(theme.source_kind === undefined ? {} : { source_kind: theme.source_kind }),
        ...(theme.rules_url === undefined ? {} : { rules_url: theme.rules_url }),
        tags: theme.tags,
        design_constraints: theme.design_constraints,
        keyword_count: theme.keywords.length,
        concept_idea_count: theme.concept_ideas.length,
        used: usedIds.has(theme.id),
      })),
    });
    return;
  }
  for (const theme of themes) {
    writeLine(
      `${theme.id}\t${theme.theme}\t${theme.source}\t${theme.year}\t${
        usedIds.has(theme.id) ? "used" : "-"
      }`,
    );
  }
}

function listProjects(projects: readonly JamProjectRecord[], json: boolean): void {
  if (json) {
    writeJson({ ok: true, projects });
    return;
  }
  for (const project of projects) {
    writeLine(
      `${project.name}\t${project.theme}\t${project.source}\t${project.created_at}\t${project.relative_path}`,
    );
  }
}

function escapeTableCell(value: string): string {
  return value.replace(/\|/gu, "\\|");
}

function writeProjectIndex(examplesDir: string, projects: readonly JamProjectRecord[]): string {
  fs.mkdirSync(examplesDir, { recursive: true });
  const indexPath = path.join(examplesDir, "INDEX.md");
  const lines = [
    "# Game Jam 项目索引",
    "",
    "> 由 `bun run gda-dev-tools jam index` 生成。项目工程默认不入库。",
    "",
    "| 项目 | 主题 | 赛事 | 年份 | 创建时间 | 工程 |",
    "|---|---|---|---|---|---|",
  ];
  for (const project of projects) {
    const relative = path.relative(examplesDir, project.path).split(path.sep).join("/");
    lines.push(
      `| ${escapeTableCell(project.name)} | ${escapeTableCell(
        project.theme,
      )} | ${escapeTableCell(project.source)} | ${project.year} | ${project.created_at.slice(
        0,
        10,
      )} | [打开](${relative}) |`,
    );
  }
  lines.push("");
  fs.writeFileSync(indexPath, lines.join("\n"), "utf8");
  return indexPath;
}

function runE2e(
  themes: readonly GameJamTheme[],
  options: JamOptions,
  examplesDir: string,
  random: () => number,
  now: () => Date,
): number {
  const e2eDir = path.join(examplesDir, E2E_DIRECTORY);
  const existed = fs.existsSync(e2eDir);
  const previous = existed ? readJamMetadata(e2eDir) : null;
  if (existed && previous?.kind !== "e2e") {
    throw new JamUsageError(
      `refusing to ${options.reset === true ? "reset" : "overwrite"} non-e2e directory: ${e2eDir}`,
    );
  }
  const isReset = existed && options.reset === true;
  const isNew = !existed || isReset;
  const previousCreatedAt =
    previous !== null && typeof previous.created_at === "string" ? previous.created_at : null;
  const createdAt = isNew ? now().toISOString() : previousCreatedAt ?? now().toISOString();
  const theme =
    options.theme === null
      ? pickTheme(themes, new Set(), random)
      : findTheme(themes, options.theme);
  const sample = sampleTheme(theme, random, now);
  const briefTemplate = loadTextFile(BRIEF_TEMPLATE_PATH, "Game Jam template");
  if (isReset) fs.rmSync(e2eDir, { recursive: true, force: true });
  fs.mkdirSync(e2eDir, { recursive: true });
  try {
    const projectFile = path.join(e2eDir, PROJECT_FILE);
    if (!fs.existsSync(projectFile)) {
      fs.writeFileSync(projectFile, renderProjectGodot(E2E_PROJECT_NAME), "utf8");
    }
    fs.writeFileSync(
      path.join(e2eDir, BRIEF_FILE),
      renderBrief(briefTemplate, theme, sample),
      "utf8",
    );
    writeJsonFile(path.join(e2eDir, JAM_METADATA_FILE), {
      schema_version: THEME_SCHEMA_VERSION,
      kind: "e2e",
      name: E2E_PROJECT_NAME,
      slug: E2E_PROJECT_NAME,
      theme_id: theme.id,
      theme: theme.theme,
      organizer: theme.organizer,
      event: theme.event,
      edition: theme.edition,
      year: theme.year,
      source: theme.source,
      source_url: theme.source_url,
      ...(theme.source_title === undefined ? {} : { source_title: theme.source_title }),
      ...(theme.rules_url === undefined ? {} : { rules_url: theme.rules_url }),
      tags: theme.tags,
      design_constraints: theme.design_constraints,
      sampled: {
        keywords: sample.keywords,
        concept_ideas: sample.concept_ideas,
      },
      draw_id: sample.draw_id,
      drawn_at: sample.drawn_at,
      created_at: createdAt,
      updated_at: now().toISOString(),
    });
  } catch (error) {
    if (isNew) fs.rmSync(e2eDir, { recursive: true, force: true });
    throw error;
  }
  const relativePath = toProjectRelativePath(e2eDir);
  if (options.json) {
    writeJson({
      ok: true,
      kind: "e2e",
      theme_id: theme.id,
      theme: theme.theme,
      source: theme.source,
      path: e2eDir,
      relative_path: relativePath,
      draw_id: sample.draw_id,
      reset: isReset,
      sampled_keyword_count: sample.keywords.length,
      sampled_concept_idea_count: sample.concept_ideas.length,
    });
  } else {
    writeLine(`[JAM] e2e project: ${relativePath}`);
    writeLine(`[JAM] theme: ${theme.theme} (${theme.source})`);
    writeLine(
      `[JAM] next: bun run gda-dev-tools build && bun run gda-dev-tools deploy --project ${relativePath}`,
    );
  }
  return 0;
}

function runDraw(
  themes: readonly GameJamTheme[],
  options: JamOptions,
  examplesDir: string,
  random: () => number,
  now: () => Date,
): number {
  const projects = scanJamProjects(examplesDir);
  const theme =
    options.theme === null
      ? pickTheme(themes, collectUsedThemeIds(projects), random)
      : findTheme(themes, options.theme);
  const sample = sampleTheme(theme, random, now);
  const intakeDir = writeIntake(examplesDir, theme, sample, loadTextFile(BRIEF_TEMPLATE_PATH, "Game Jam template"));
  const intakeRelativePath = toProjectRelativePath(intakeDir);
  if (options.json) {
    writeJson({
      ok: true,
      theme_id: theme.id,
      theme: theme.theme,
      source: theme.source,
      source_url: theme.source_url,
      intake_path: intakeDir,
      intake_relative_path: intakeRelativePath,
      draw_id: sample.draw_id,
      sampled_keyword_count: sample.keywords.length,
      sampled_concept_idea_count: sample.concept_ideas.length,
    });
  } else {
    writeLine(`[JAM] ${theme.theme} (${theme.source})`);
    writeLine(`[JAM] intake: ${intakeRelativePath}`);
    writeLine(
      `[JAM] sampled: ${sample.keywords.length} keywords, ${sample.concept_ideas.length} concept ideas`,
    );
    writeLine(
      `[JAM] next: read ${intakeRelativePath}/GAMEJAM.md, then create a project with: bun run gda-dev-tools jam create --theme ${theme.id} --name "<project name>"`,
    );
  }
  return 0;
}

function runCreate(
  themes: readonly GameJamTheme[],
  options: JamOptions,
  examplesDir: string,
  now: () => Date,
): number {
  if (options.theme === null || options.name === null) {
    throw new JamUsageError("jam create requires --theme and --name");
  }
  const theme = findTheme(themes, options.theme);
  const created = createProjectFromIntake({
    examplesDir,
    theme,
    name: options.name,
    slug: options.slug,
    out: options.out,
    now,
  });
  if (options.json) {
    writeJson({
      ok: true,
      theme_id: theme.id,
      theme: theme.theme,
      name: options.name.trim(),
      slug: created.slug,
      hash8: created.hash8,
      path: created.path,
      relative_path: created.relativePath,
      project_file: path.join(created.path, PROJECT_FILE),
    });
  } else {
    writeLine(`[JAM] project: ${created.relativePath}`);
    writeLine(`[JAM] name: ${options.name.trim()} (${created.slug}-${created.hash8})`);
    writeLine(`[JAM] next: bun run gda-dev-tools deploy --project ${created.relativePath}`);
  }
  return 0;
}

export function runJam(options: JamOptions): number {
  try {
    const examplesDir = options.examplesDir ?? EXAMPLES_DIR;
    const random = options.random ?? Math.random;
    const now = options.now ?? (() => new Date());
    const themes = loadThemes();
    if (options.action === "list") {
      listThemes(themes, collectUsedThemeIds(scanJamProjects(examplesDir)), options.json);
      return 0;
    }
    if (options.action === "projects") {
      listProjects(scanJamProjects(examplesDir), options.json);
      return 0;
    }
    if (options.action === "index") {
      const projects = scanJamProjects(examplesDir);
      const indexPath = writeProjectIndex(examplesDir, projects);
      const relativePath = toProjectRelativePath(indexPath);
      if (options.json) {
        writeJson({ ok: true, path: indexPath, relative_path: relativePath, count: projects.length });
      } else {
        writeLine(`[JAM] index: ${relativePath} (${projects.length} projects)`);
      }
      return 0;
    }
    if (options.action === "e2e") {
      return runE2e(themes, options, examplesDir, random, now);
    }
    if (options.action === "create") {
      return runCreate(themes, options, examplesDir, now);
    }
    return runDraw(themes, options, examplesDir, random, now);
  } catch (error) {
    console.error(`[ERROR] ${errorMessage(error)}`);
    return error instanceof JamUsageError ? 2 : 1;
  }
}
