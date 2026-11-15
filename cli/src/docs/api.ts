import { CliError, ExitCode } from "../errors";

export interface DumpHeader {
  version_major: number;
  version_minor: number;
  version_patch: number;
  version_status: string;
  version_build?: string;
  version_full_name?: string;
  precision?: string;
}

export interface EngineVersion {
  major: number;
  minor: number;
  patch?: number;
  status?: string;
  fullName?: string;
}

export type EntryKind = "class" | "method" | "property" | "signal" | "enum" | "constant" | "utility";

export interface ApiEntry {
  kind: EntryKind;
  className: string;
  name: string;
  signature: string;
  summary: string;
}

export interface ApiArgument {
  name: string;
  type: string;
  default?: string;
}

export interface ApiMethod {
  name: string;
  args: ApiArgument[];
  returnType: string;
  description: string;
  isStatic: boolean;
  isVirtual: boolean;
  isConst: boolean;
  signature: string;
}

export interface ApiProperty {
  name: string;
  type: string;
  description: string;
  signature: string;
}

export interface ApiSignal {
  name: string;
  args: ApiArgument[];
  description: string;
  signature: string;
}

export interface ApiEnumValue {
  name: string;
  value: unknown;
  description: string;
}

export interface ApiEnum {
  name: string;
  isBitfield: boolean;
  values: ApiEnumValue[];
  signature: string;
}

export interface ApiConstant {
  name: string;
  value: unknown;
  isBitfield: boolean;
  description: string;
  signature: string;
}

export interface ApiOperator {
  name: string;
  rightType: string;
  returnType: string;
  description: string;
  signature: string;
}

export interface ApiConstructor {
  name: string;
  args: ApiArgument[];
  description: string;
  signature: string;
}

export interface ApiClass {
  name: string;
  inherits: string;
  apiType: string;
  isRefcounted: boolean;
  isInstantiable: boolean;
  brief: string;
  description: string;
  methods: ApiMethod[];
  properties: ApiProperty[];
  signals: ApiSignal[];
  enums: ApiEnum[];
  constants: ApiConstant[];
  operators: ApiOperator[];
  constructors: ApiConstructor[];
}

export interface ApiIndex {
  header: DumpHeader;
  classes: Map<string, ApiClass>;
  builtins: Map<string, ApiClass>;
  utilityFunctions: ApiMethod[];
  globalEnums: ApiEnum[];
  globalConstants: ApiConstant[];
  globalScope: ApiClass;
  entries: ApiEntry[];
}

function asRecord(value: unknown): Record<string, unknown> {
  if (value !== null && typeof value === "object" && !Array.isArray(value)) {
    return value as Record<string, unknown>;
  }
  return {};
}

function asArray(value: unknown): unknown[] {
  return Array.isArray(value) ? value : [];
}

function asString(value: unknown, fallback = ""): string {
  return typeof value === "string" ? value : fallback;
}

function asBoolean(value: unknown, fallback = false): boolean {
  return typeof value === "boolean" ? value : fallback;
}

function asNumber(value: unknown, fallback = 0): number {
  return typeof value === "number" && Number.isFinite(value) ? value : fallback;
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export function plainText(value: string): string {
  return value
    .replace(/\[\/?(?:code|codeblock|i|b|u|br)\]/g, " ")
    .replace(/\[(?:param|method|member|signal|constant|enum|theme_item)\s+([^\]]+)\]/g, "$1")
    .replace(/\[([A-Za-z_@][A-Za-z0-9_@.]*)\]/g, "$1")
    .replace(/\[[^\]]*\]/g, "")
    .replace(/\s+/g, " ")
    .trim();
}

function truncate(value: string, limit = 240): string {
  if (value.length <= limit) return value;
  return `${value.slice(0, limit - 1).trimEnd()}…`;
}

function readHeader(value: unknown, label: string): DumpHeader {
  const record = asRecord(value);
  const major = record.version_major;
  const minor = record.version_minor;
  if (typeof major !== "number" || typeof minor !== "number") {
    throw new CliError(ExitCode.Transport, `${label} is missing version_major/version_minor`);
  }
  return {
    version_major: major,
    version_minor: minor,
    version_patch: asNumber(record.version_patch),
    version_status: asString(record.version_status, "stable"),
    ...(typeof record.version_build === "string" ? { version_build: record.version_build } : {}),
    ...(typeof record.version_full_name === "string" ? { version_full_name: record.version_full_name } : {}),
    ...(typeof record.precision === "string" ? { precision: record.precision } : {}),
  };
}

export function engineVersionFromHeader(header: DumpHeader): EngineVersion {
  return {
    major: header.version_major,
    minor: header.version_minor,
    patch: header.version_patch,
    status: header.version_status,
    ...(header.version_full_name !== undefined ? { fullName: header.version_full_name } : {}),
  };
}

function readArguments(value: unknown): ApiArgument[] {
  const args: ApiArgument[] = [];
  for (const item of asArray(value)) {
    const record = asRecord(item);
    const name = asString(record.name);
    if (name === "") continue;
    args.push({
      name,
      type: asString(record.type, "Variant"),
      ...(typeof record.default_value === "string" ? { default: record.default_value } : {}),
    });
  }
  return args;
}

function formatArgs(args: ApiArgument[]): string {
  return args.map((arg) => `${arg.name}: ${arg.type}`).join(", ");
}

function formatMethodSignature(name: string, args: ApiArgument[], returnType: string): string {
  const head = `${name}(${formatArgs(args)})`;
  if (returnType === "" || returnType === "void") return head;
  return `${head} -> ${returnType}`;
}

function readMethod(value: unknown): ApiMethod | undefined {
  const record = asRecord(value);
  const name = asString(record.name);
  if (name === "") return undefined;
  const args = readArguments(record.arguments);
  const returnType = asString(asRecord(record.return_value).type, asString(record.return_type, "void"));
  return {
    name,
    args,
    returnType,
    description: plainText(asString(record.description)),
    isStatic: asBoolean(record.is_static),
    isVirtual: asBoolean(record.is_virtual),
    isConst: asBoolean(record.is_const),
    signature: formatMethodSignature(name, args, returnType),
  };
}

function readMethods(value: unknown): ApiMethod[] {
  const methods: ApiMethod[] = [];
  for (const item of asArray(value)) {
    const method = readMethod(item);
    if (method !== undefined) methods.push(method);
  }
  return methods;
}

function readProperty(value: unknown): ApiProperty | undefined {
  const record = asRecord(value);
  const name = asString(record.name);
  if (name === "") return undefined;
  const type = asString(record.type, "Variant");
  return {
    name,
    type,
    description: plainText(asString(record.description)),
    signature: `${name}: ${type}`,
  };
}

function readProperties(value: unknown): ApiProperty[] {
  const properties: ApiProperty[] = [];
  for (const item of asArray(value)) {
    const property = readProperty(item);
    if (property !== undefined) properties.push(property);
  }
  return properties;
}

function readSignal(value: unknown): ApiSignal | undefined {
  const record = asRecord(value);
  const name = asString(record.name);
  if (name === "") return undefined;
  const args = readArguments(record.arguments);
  return {
    name,
    args,
    description: plainText(asString(record.description)),
    signature: `${name}(${formatArgs(args)})`,
  };
}

function readSignals(value: unknown): ApiSignal[] {
  const signals: ApiSignal[] = [];
  for (const item of asArray(value)) {
    const signal = readSignal(item);
    if (signal !== undefined) signals.push(signal);
  }
  return signals;
}

function readEnum(value: unknown): ApiEnum | undefined {
  const record = asRecord(value);
  const name = asString(record.name);
  if (name === "") return undefined;
  const values: ApiEnumValue[] = [];
  for (const item of asArray(record.values)) {
    const entry = asRecord(item);
    const entryName = asString(entry.name);
    if (entryName === "") continue;
    values.push({
      name: entryName,
      value: entry.value,
      description: plainText(asString(entry.description)),
    });
  }
  return {
    name,
    isBitfield: asBoolean(record.is_bitfield),
    values,
    signature: `enum ${name}`,
  };
}

function readEnums(value: unknown): ApiEnum[] {
  const enums: ApiEnum[] = [];
  for (const item of asArray(value)) {
    const entry = readEnum(item);
    if (entry !== undefined) enums.push(entry);
  }
  return enums;
}

function readConstant(value: unknown): ApiConstant | undefined {
  const record = asRecord(value);
  const name = asString(record.name);
  if (name === "") return undefined;
  const raw = record.value;
  return {
    name,
    value: raw,
    isBitfield: asBoolean(record.is_bitfield),
    description: plainText(asString(record.description)),
    signature: `${name} = ${String(raw)}`,
  };
}

function readConstants(value: unknown): ApiConstant[] {
  const constants: ApiConstant[] = [];
  for (const item of asArray(value)) {
    const constant = readConstant(item);
    if (constant !== undefined) constants.push(constant);
  }
  return constants;
}

function readOperator(value: unknown): ApiOperator | undefined {
  const record = asRecord(value);
  const name = asString(record.name);
  if (name === "") return undefined;
  const rightType = asString(record.right_type);
  const returnType = asString(record.return_type, "Variant");
  const signature = rightType === ""
    ? `${name} -> ${returnType}`
    : `${name} ${rightType} -> ${returnType}`;
  return {
    name,
    rightType,
    returnType,
    description: plainText(asString(record.description)),
    signature,
  };
}

function readOperators(value: unknown): ApiOperator[] {
  const operators: ApiOperator[] = [];
  for (const item of asArray(value)) {
    const operator = readOperator(item);
    if (operator !== undefined) operators.push(operator);
  }
  return operators;
}

function readConstructor(value: unknown, className: string): ApiConstructor | undefined {
  const record = asRecord(value);
  const args = readArguments(record.arguments);
  return {
    name: className,
    args,
    description: plainText(asString(record.description)),
    signature: `${className}(${formatArgs(args)})`,
  };
}

function readConstructors(value: unknown, className: string): ApiConstructor[] {
  const constructors: ApiConstructor[] = [];
  for (const item of asArray(value)) {
    const constructor = readConstructor(item, className);
    if (constructor !== undefined) constructors.push(constructor);
  }
  return constructors;
}

function readClass(value: unknown, builtin: boolean): ApiClass | undefined {
  const record = asRecord(value);
  const name = asString(record.name);
  if (name === "") return undefined;
  const inherits = asString(record.inherits);
  return {
    name,
    inherits,
    apiType: builtin ? "builtin" : asString(record.api_type, "core"),
    isRefcounted: asBoolean(record.is_refcounted),
    isInstantiable: asBoolean(record.is_instantiable, builtin),
    brief: plainText(asString(record.brief_description)),
    description: plainText(asString(record.description)),
    methods: readMethods(record.methods),
    properties: readProperties(record.properties),
    signals: readSignals(record.signals),
    enums: readEnums(record.enums),
    constants: readConstants(record.constants),
    operators: readOperators(record.operators),
    constructors: readConstructors(record.constructors, name),
  };
}

function pushEntry(
  entries: ApiEntry[],
  kind: EntryKind,
  className: string,
  name: string,
  signature: string,
  summary: string,
): void {
  entries.push({ kind, className, name, signature, summary: truncate(summary) });
}

function pushClassEntries(entries: ApiEntry[], classes: Map<string, ApiClass>, kind: "class"): void {
  for (const cls of classes.values()) {
    pushEntry(
      entries,
      kind,
      cls.name,
      cls.name,
      cls.inherits === "" ? `class ${cls.name}` : `class ${cls.name} extends ${cls.inherits}`,
      cls.brief,
    );
    for (const method of cls.methods) {
      pushEntry(entries, "method", cls.name, method.name, method.signature, method.description);
    }
    for (const property of cls.properties) {
      pushEntry(entries, "property", cls.name, property.name, property.signature, property.description);
    }
    for (const signal of cls.signals) {
      pushEntry(entries, "signal", cls.name, signal.name, signal.signature, signal.description);
    }
    for (const item of cls.enums) {
      pushEntry(entries, "enum", cls.name, item.name, item.signature, item.values.map((v) => v.name).join(", "));
    }
    for (const constant of cls.constants) {
      pushEntry(entries, "constant", cls.name, constant.name, constant.signature, constant.description);
    }
  }
}

function synthesizeGlobalScope(
  utilityFunctions: ApiMethod[],
  globalEnums: ApiEnum[],
  globalConstants: ApiConstant[],
): ApiClass {
  return {
    name: "@GlobalScope",
    inherits: "",
    apiType: "global",
    isRefcounted: false,
    isInstantiable: false,
    brief: "Global scope utility functions, enums, and constants.",
    description: "",
    methods: utilityFunctions,
    properties: [],
    signals: [],
    enums: globalEnums,
    constants: globalConstants,
    operators: [],
    constructors: [],
  };
}

function buildEntries(
  classes: Map<string, ApiClass>,
  builtins: Map<string, ApiClass>,
  utilityFunctions: ApiMethod[],
  globalEnums: ApiEnum[],
  globalConstants: ApiConstant[],
): ApiEntry[] {
  const entries: ApiEntry[] = [];
  pushClassEntries(entries, classes, "class");
  pushClassEntries(entries, builtins, "class");
  for (const method of utilityFunctions) {
    pushEntry(entries, "utility", "@GlobalScope", method.name, method.signature, method.description);
  }
  for (const item of globalEnums) {
    pushEntry(entries, "enum", "@GlobalScope", item.name, item.signature, item.values.map((v) => v.name).join(", "));
  }
  for (const constant of globalConstants) {
    pushEntry(entries, "constant", "@GlobalScope", constant.name, constant.signature, constant.description);
  }
  return entries;
}

export function parseDump(text: string): ApiIndex {
  let raw: unknown;
  try {
    raw = JSON.parse(text);
  } catch (error) {
    throw new CliError(ExitCode.Transport, `engine docs are not valid JSON: ${errorMessage(error)}`);
  }
  const root = asRecord(raw);
  const header = readHeader(root.header, "engine docs header");
  const classes = new Map<string, ApiClass>();
  for (const item of asArray(root.classes)) {
    const cls = readClass(item, false);
    if (cls !== undefined) classes.set(cls.name, cls);
  }
  const builtins = new Map<string, ApiClass>();
  for (const item of asArray(root.builtin_classes)) {
    const cls = readClass(item, true);
    if (cls !== undefined) builtins.set(cls.name, cls);
  }
  const utilityFunctions: ApiMethod[] = [];
  for (const item of asArray(root.utility_functions)) {
    const method = readMethod(item);
    if (method !== undefined) utilityFunctions.push(method);
  }
  const globalEnums = readEnums(root.global_enums);
  const globalConstants = readConstants(root.global_constants);
  const globalScope = synthesizeGlobalScope(utilityFunctions, globalEnums, globalConstants);
  return {
    header,
    classes,
    builtins,
    utilityFunctions,
    globalEnums,
    globalConstants,
    globalScope,
    entries: buildEntries(classes, builtins, utilityFunctions, globalEnums, globalConstants),
  };
}
