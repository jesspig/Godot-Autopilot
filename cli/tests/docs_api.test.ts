import { describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import { parseDump } from "../src/docs/api";
import {
  getClassView,
  parseKind,
  parseSection,
  resolveMember,
  searchDocs,
} from "../src/docs/search";
import { CliError, ExitCode } from "../src/errors";

const fixture = path.join(import.meta.dir, "fixtures", "docs", "extension_api.json");
const index = parseDump(fs.readFileSync(fixture, "utf8"));

function expectCliError(action: () => unknown, code: number): void {
  let caught: unknown;
  try {
    action();
  } catch (error) {
    caught = error;
  }
  expect(caught).toBeInstanceOf(CliError);
  expect((caught as CliError).code).toBe(code);
}

describe("docs api", () => {
  test("解析类、内建类、全局函数与全局枚举", () => {
    expect(index.classes.size).toBe(5);
    expect(index.builtins.size).toBe(1);
    expect(index.utilityFunctions.length).toBe(4);
    expect(index.globalEnums.length).toBe(2);
    expect(index.globalConstants.length).toBe(1);
    expect(index.entries.length).toBeGreaterThan(50);
  });

  test("合成 @GlobalScope 并可查询全局函数与枚举", () => {
    const counts = getClassView(index, "@GlobalScope").counts;
    expect(counts.methods).toBe(4);
    expect(counts.enums).toBe(2);
    expect(counts.constants).toBe(1);
    const matches = resolveMember(index, "@GlobalScope", "sin");
    expect(matches.length).toBe(1);
    expect(matches[0]?.kind).toBe("utility");
    expect(matches[0]?.signature.startsWith("sin(")).toBe(true);
  });

  test("保留继承链与类签名", () => {
    const node2d = index.classes.get("Node2D");
    expect(node2d?.inherits).toBe("CanvasItem");
    const result = searchDocs(index, "Node2D", { limit: 1 });
    expect(result.results[0]?.signature).toBe("class Node2D extends CanvasItem");
  });

  test("搜索按精确、前缀、子串排序并支持 kind/class/limit", () => {
    const exact = searchDocs(index, "Tween", { limit: 5 });
    expect(exact.results[0]?.name).toBe("Tween");
    expect(exact.results[0]?.score).toBe(100);
    const limited = searchDocs(index, "position", { limit: 2 });
    expect(limited.results.length).toBe(2);
    expect(limited.total).toBeGreaterThan(2);
    const properties = searchDocs(index, "position", { kind: "property" });
    expect(properties.results.length).toBeGreaterThan(0);
    expect(properties.results.every((item) => item.kind === "property")).toBe(true);
    const scoped = searchDocs(index, "position", { className: "Node2D", kind: "property" });
    expect(scoped.results.length).toBeGreaterThan(0);
    expect(scoped.results.every((item) => item.className === "Node2D")).toBe(true);
  });

  test("resolveMember 识别方法、属性、枚举值与枚举名", () => {
    const method = resolveMember(index, "Node2D", "look_at");
    expect(method.length).toBe(1);
    expect(method[0]?.kind).toBe("method");
    expect(method[0]?.signature).toBe("look_at(point: Vector2)");
    const property = resolveMember(index, "Node2D", "position");
    expect(property.length).toBe(1);
    expect(property[0]?.kind).toBe("property");
    expect(property[0]?.signature).toBe("position: Vector2");
    const value = resolveMember(index, "Node", "PROCESS_MODE_INHERIT");
    expect(value.length).toBe(1);
    expect(value[0]?.kind).toBe("constant");
    const enumName = resolveMember(index, "Node", "ProcessMode");
    expect(enumName.some((item) => item.kind === "enum")).toBe(true);
  });

  test("同名方法与属性返回全部匹配", () => {
    const dump = JSON.stringify({
      header: { version_major: 1, version_minor: 0, version_patch: 0, version_status: "stable" },
      classes: [
        {
          name: "Dup",
          inherits: "",
          api_type: "core",
          brief_description: "Dup class",
          description: "",
          methods: [{ name: "value", return_type: "int", arguments: [] }],
          properties: [{ name: "value", type: "int" }],
          signals: [],
          enums: [],
          constants: [],
        },
      ],
      builtin_classes: [],
      utility_functions: [],
      global_enums: [],
      global_constants: [],
    });
    const dup = parseDump(dump);
    const matches = resolveMember(dup, "Dup", "value");
    expect(matches.map((item) => item.kind).sort()).toEqual(["method", "property"]);
  });

  test("类视图默认只给摘要与计数，按 section/member/full 展开", () => {
    const base = getClassView(index, "Node2D");
    expect(base.counts.methods).toBeGreaterThan(0);
    expect(base.counts.properties).toBeGreaterThan(0);
    expect(base.sections).toBeUndefined();
    const methods = getClassView(index, "Node2D", { section: "methods" });
    expect(Array.isArray(methods.sections?.methods)).toBe(true);
    const member = getClassView(index, "Node2D", { member: "position" });
    expect(member.members?.some((item) => item.name === "position" && item.kind === "property")).toBe(true);
    const full = getClassView(index, "Node2D", { full: true });
    expect(full.sections?.methods).toBeDefined();
    expect(full.sections?.properties).toBeDefined();
  });

  test("Vector2 内建类包含成员、运算符与构造器", () => {
    const view = getClassView(index, "Vector2");
    expect(view.api_type).toBe("builtin");
    expect(view.counts.operators).toBeGreaterThan(0);
    expect(view.counts.constructors).toBeGreaterThan(0);
  });

  test("非法 section/kind 报用法错误", () => {
    expectCliError(() => parseSection("bogus"), ExitCode.Usage);
    expectCliError(() => parseKind("bogus"), ExitCode.Usage);
  });

  test("未知类与非法 dump 报对应错误", () => {
    expectCliError(() => getClassView(index, "NoSuchClass"), ExitCode.ToolFailure);
    expectCliError(() => resolveMember(index, "NoSuchClass", "x"), ExitCode.ToolFailure);
    expectCliError(() => parseDump("{"), ExitCode.Transport);
    expectCliError(() => parseDump("{}"), ExitCode.Transport);
  });
});
