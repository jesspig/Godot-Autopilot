import { CliError, ExitCode } from "../errors";
import type { ApiClass, ApiEntry, ApiIndex, EntryKind } from "./api";

export interface SearchOptions {
  className?: string;
  kind?: EntryKind;
  limit?: number;
}

export interface SearchHit extends ApiEntry {
  score: number;
}

export interface SearchResult {
  results: SearchHit[];
  total: number;
}

export interface ClassMember extends ApiEntry {
  section: string;
}

export interface ClassView {
  name: string;
  inherits: string;
  api_type: string;
  brief: string;
  description: string;
  counts: Record<string, number>;
  members?: ClassMember[];
  sections?: Record<string, unknown>;
}

const KIND_ORDER: EntryKind[] = ["class", "method", "property", "signal", "enum", "constant", "utility"];

const SECTION_ORDER = [
  "methods",
  "properties",
  "signals",
  "enums",
  "constants",
  "operators",
  "constructors",
] as const;

export type SectionName =
  | "brief"
  | "description"
  | "methods"
  | "properties"
  | "signals"
  | "enums"
  | "constants"
  | "operators"
  | "constructors";

export function parseSection(text: string | undefined): SectionName | "all" | undefined {
  if (text === undefined) return undefined;
  const value = text.trim();
  if (value === "all") return "all";
  if ((SECTION_ORDER as readonly string[]).includes(value) || value === "brief" || value === "description") {
    return value as SectionName;
  }
  throw new CliError(
    ExitCode.Usage,
    `--section must be brief, description, ${SECTION_ORDER.join(", ")}, or all; got: ${text}`,
  );
}

export function parseKind(text: string | undefined): EntryKind | undefined {
  if (text === undefined) return undefined;
  const value = text.trim();
  if ((KIND_ORDER as string[]).includes(value)) return value as EntryKind;
  throw new CliError(
    ExitCode.Usage,
    `--kind must be one of ${KIND_ORDER.join(", ")}; got: ${text}`,
  );
}

function kindRank(kind: EntryKind): number {
  return KIND_ORDER.indexOf(kind);
}

function scoreEntry(entry: ApiEntry, query: string): number {
  const name = entry.name.toLowerCase();
  if (name === query) return 100;
  if (name.startsWith(query)) return 80;
  if (name.includes(query)) return 60;
  if (entry.summary.toLowerCase().includes(query)) return 30;
  return 0;
}

export function searchDocs(index: ApiIndex, query: string, options: SearchOptions = {}): SearchResult {
  const normalized = query.trim().toLowerCase();
  const limit = Math.max(1, Math.min(options.limit ?? 20, 200));
  if (normalized === "") return { results: [], total: 0 };
  const hits: SearchHit[] = [];
  for (const entry of index.entries) {
    if (options.kind !== undefined && entry.kind !== options.kind) continue;
    if (
      options.className !== undefined &&
      entry.className.toLowerCase() !== options.className.trim().toLowerCase()
    ) {
      continue;
    }
    const score = scoreEntry(entry, normalized);
    if (score > 0) hits.push({ ...entry, score });
  }
  hits.sort(
    (left, right) =>
      right.score - left.score ||
      kindRank(left.kind) - kindRank(right.kind) ||
      left.className.localeCompare(right.className) ||
      left.name.localeCompare(right.name),
  );
  return { results: hits.slice(0, limit), total: hits.length };
}

export function getApiClass(index: ApiIndex, name: string): ApiClass | undefined {
  if (name === "@GlobalScope") return index.globalScope;
  const direct = index.classes.get(name) ?? index.builtins.get(name);
  if (direct !== undefined) return direct;
  const lower = name.toLowerCase();
  for (const item of index.classes.values()) {
    if (item.name.toLowerCase() === lower) return item;
  }
  for (const item of index.builtins.values()) {
    if (item.name.toLowerCase() === lower) return item;
  }
  return undefined;
}

function methodEntry(cls: ApiClass, name: string, signature: string, summary: string): ApiEntry {
  return {
    kind: cls.name === "@GlobalScope" ? "utility" : "method",
    className: cls.name,
    name,
    signature,
    summary,
  };
}

function classMembers(cls: ApiClass): ClassMember[] {
  const members: ClassMember[] = [];
  for (const method of cls.methods) {
    members.push({ ...methodEntry(cls, method.name, method.signature, method.description), section: "methods" });
  }
  for (const property of cls.properties) {
    members.push({
      kind: "property",
      className: cls.name,
      name: property.name,
      signature: property.signature,
      summary: property.description,
      section: "properties",
    });
  }
  for (const signal of cls.signals) {
    members.push({
      kind: "signal",
      className: cls.name,
      name: signal.name,
      signature: signal.signature,
      summary: signal.description,
      section: "signals",
    });
  }
  for (const item of cls.enums) {
    members.push({
      kind: "enum",
      className: cls.name,
      name: item.name,
      signature: item.signature,
      summary: item.values.map((value) => value.name).join(", "),
      section: "enums",
    });
    for (const value of item.values) {
      members.push({
        kind: "constant",
        className: cls.name,
        name: value.name,
        signature: `${item.name}.${value.name} = ${String(value.value)}`,
        summary: value.description,
        section: "enums",
      });
    }
  }
  for (const constant of cls.constants) {
    members.push({
      kind: "constant",
      className: cls.name,
      name: constant.name,
      signature: constant.signature,
      summary: constant.description,
      section: "constants",
    });
  }
  for (const operator of cls.operators) {
    members.push({
      kind: "method",
      className: cls.name,
      name: operator.name,
      signature: operator.signature,
      summary: operator.description,
      section: "operators",
    });
  }
  for (const constructor of cls.constructors) {
    members.push({
      kind: "method",
      className: cls.name,
      name: constructor.name,
      signature: constructor.signature,
      summary: constructor.description,
      section: "constructors",
    });
  }
  const sectionIndex = (section: string): number => {
    const index = (SECTION_ORDER as readonly string[]).indexOf(section);
    return index === -1 ? SECTION_ORDER.length : index;
  };
  return members.sort(
    (left, right) =>
      sectionIndex(left.section) - sectionIndex(right.section) ||
      left.name.localeCompare(right.name) ||
      left.signature.localeCompare(right.signature),
  );
}

export function resolveMember(
  index: ApiIndex,
  className: string,
  name: string,
  kind?: EntryKind,
): ClassMember[] {
  const cls = getApiClass(index, className);
  if (cls === undefined) {
    throw new CliError(
      ExitCode.ToolFailure,
      `class not found: ${className}`,
      "Run 'gda docs search <name>' to find the class, or pass --docs-file for an external dump.",
    );
  }
  const members = classMembers(cls).filter((member) => kind === undefined || member.kind === kind);
  const exact = members.filter((member) => member.name === name);
  if (exact.length > 0) return exact;
  const lower = name.toLowerCase();
  return members.filter((member) => member.name.toLowerCase() === lower);
}

export function getClassView(
  index: ApiIndex,
  name: string,
  options: { section?: SectionName | "all"; member?: string; full?: boolean } = {},
): ClassView {
  const cls = getApiClass(index, name);
  if (cls === undefined) {
    throw new CliError(
      ExitCode.ToolFailure,
      `class not found: ${name}`,
      "Run 'gda docs search <name>' to find the class, or pass --docs-file for an external dump.",
    );
  }
  const members = classMembers(cls);
  const counts: Record<string, number> = {
    methods: cls.methods.length,
    properties: cls.properties.length,
    signals: cls.signals.length,
    enums: cls.enums.length,
    constants: cls.constants.length,
    operators: cls.operators.length,
    constructors: cls.constructors.length,
  };
  const sectionValue = (section: (typeof SECTION_ORDER)[number]): unknown => {
    switch (section) {
      case "methods":
        return cls.methods;
      case "properties":
        return cls.properties;
      case "signals":
        return cls.signals;
      case "enums":
        return cls.enums;
      case "constants":
        return cls.constants;
      case "operators":
        return cls.operators;
      case "constructors":
        return cls.constructors;
    }
  };
  const base: ClassView = {
    name: cls.name,
    inherits: cls.inherits,
    api_type: cls.apiType,
    brief: cls.brief,
    description: cls.description,
    counts,
  };
  if (options.member !== undefined && options.member.trim() !== "") {
    const needle = options.member.trim().toLowerCase();
    return {
      ...base,
      members: members.filter(
        (member) =>
          member.name.toLowerCase().includes(needle) ||
          member.signature.toLowerCase().includes(needle),
      ),
    };
  }
  if (options.full === true || options.section === "all") {
    const sections: Record<string, unknown> = {};
    for (const section of SECTION_ORDER) {
      sections[section] = sectionValue(section);
    }
    return { ...base, sections };
  }
  if (options.section === undefined) return base;
  if (options.section === "brief") return { ...base, sections: { brief: cls.brief } };
  if (options.section === "description") return { ...base, sections: { description: cls.description } };
  return {
    ...base,
    sections: {
      [options.section]: sectionValue(options.section),
    },
  };
}
