import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import {
  buildDigest,
  computeCoverage,
  countEvidenceRows,
  extractCausalTriples,
  extractIds,
  extractLevels,
  extractPlaytimeMinutes,
  parseRoundStartMs,
  parseTraceJsonl,
  readCliCoverageRecords,
  runHarnessCoverage,
  runHarnessDigest,
  runHarnessSetup,
  setupCommandSequence,
  type CaptureRunner,
} from "../../devtools/commands/harness";
import type { TraceRecord } from "../../cli/src/trace";
import { getCanonicalCatalog } from "../../devtools/commands/tool-audit";
import { getCanonicalCatalog as auditCatalog } from "../../devtools/commands/tool-audit";
import {
  main,
  parseHarnessCoverage,
  parseHarnessDigest,
  parseHarnessSetup,
} from "../../devtools/main";

let tmp: string;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-harness-"));
});

afterEach(() => {
  fs.rmSync(tmp, { recursive: true, force: true });
});

function captureStdout(): { read: () => string; restore: () => void } {
  const originalWrite = process.stdout.write;
  const originalLog = console.log;
  let output = "";
  const append = (chunk: unknown): void => {
    output += `${String(chunk)}\n`;
  };
  process.stdout.write = ((chunk: unknown) => {
    output += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  console.log = append;
  return {
    read: () => output,
    restore: () => {
      process.stdout.write = originalWrite;
      console.log = originalLog;
    },
  };
}

function captureStderr(): { read: () => string; restore: () => void } {
  const originalWrite = process.stderr.write;
  const originalError = console.error;
  let output = "";
  const append = (chunk: unknown): void => {
    output += `${String(chunk)}\n`;
  };
  process.stderr.write = ((chunk: unknown) => {
    output += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  console.error = append;
  return {
    read: () => output,
    restore: () => {
      process.stderr.write = originalWrite;
      console.error = originalError;
    },
  };
}

const FAKE_CATALOG = ["property_get", "property_set", "create_scene_node", "get_scene_tree", "ping_probe"];

function traceLine(
  tool: string,
  options: { ok?: boolean; wall_end_ms?: number; kind?: string } = {},
): string {
  return JSON.stringify({
    kind: options.kind ?? "tool_call",
    tool,
    ok: options.ok ?? true,
    wall_end_ms: options.wall_end_ms ?? 100,
  });
}

describe("IF-04 canonical catalog", () => {
  test("exports the 253 canonical names without aliases", () => {
    const catalog = getCanonicalCatalog();
    expect(auditCatalog()).toEqual(catalog);
    expect(catalog.length).toBe(253);
    expect(new Set(catalog).size).toBe(253);
    expect(catalog).toContain("property_get");
    expect(catalog).toContain("get_scene_tree");
    expect(catalog).not.toContain("ping");
    expect(catalog).not.toContain("set_theme_color");
  });
});

describe("IF-01 setup", () => {
  test("builds the seven-step command sequence in order", () => {
    const sequence = setupCommandSequence("/proj/demo", false);
    expect(sequence).toEqual([
      ["bun", "run", "gda-dev-tools", "build"],
      ["bun", "run", "gda-dev-tools", "deploy", "--project", "/proj/demo"],
      ["gda", "start", "--project", "/proj/demo"],
      ["gda", "list", "--project", "/proj/demo"],
      ["gda", "status", "--project", "/proj/demo", "--json"],
      ["gda", "ping", "--project", "/proj/demo"],
      ["gda", "tools", "list", "--project", "/proj/demo"],
    ]);
  });

  test("every gda step carries --project so no instance lookup falls back to the repo root", () => {
    const gdaSteps = setupCommandSequence("/proj/demo", false).filter(
      (command) => command[0] === "gda",
    );
    expect(gdaSteps.length).toBe(5);
    for (const command of gdaSteps) {
      expect(command).toContain("--project");
      expect(command[command.indexOf("--project") + 1]).toBe("/proj/demo");
    }
  });

  test("adds --headless only when explicitly requested", () => {
    expect(setupCommandSequence("/proj/demo", true)[2]).toEqual([
      "gda",
      "start",
      "--project",
      "/proj/demo",
      "--headless",
    ]);
  });

  test("runs every step and returns 0 on full success", async () => {
    const calls: string[][] = [];
    const capture = captureStdout();
    try {
      expect(
        await runHarnessSetup({ project: "/proj/demo", headless: false }, (command) => {
          calls.push(command);
          return 0;
        }),
      ).toBe(0);
    } finally {
      capture.restore();
    }
    expect(calls).toHaveLength(7);
    expect(capture.read()).toContain("$ bun run gda-dev-tools build");
    expect(capture.read()).toContain("$ gda tools list");
  });

  test("stops at the first failing step and returns its exit code", async () => {
    const calls: string[][] = [];
    const capture = captureStdout();
    const stderr = captureStderr();
    try {
      expect(
        await runHarnessSetup({ project: "/proj/demo", headless: false }, (command) => {
          calls.push(command);
          return calls.length === 3 ? 7 : 0;
        }),
      ).toBe(7);
    } finally {
      stderr.restore();
      capture.restore();
    }
    expect(calls).toHaveLength(3);
    expect(stderr.read()).toContain("harness setup stopped at");
  });
});

describe("coverage parsing and computation", () => {
  test("parses jsonl records and skips malformed lines", () => {
    const text = [
      traceLine("property_get", { wall_end_ms: 300 }),
      "not-json",
      JSON.stringify({ name: "get_scene_tree", kind: "tool_call", ok: false, wall_end_ms: 100 }),
      JSON.stringify({ kind: "event", tool: "", wall_end_ms: 50 }),
    ].join("\n");
    const records = parseTraceJsonl(text);
    expect(records).toHaveLength(3);
    expect(records[0].tool).toBe("property_get");
    expect(records[1].tool).toBe("get_scene_tree");
    expect(records[1].ok).toBe(false);
    expect(records[2].tool).toBe("");
  });

  test("counts canonical usage, excludes aliases and marks invalid touches", () => {
    const records = parseTraceJsonl(
      [
        traceLine("property_get", { wall_end_ms: 300 }),
        traceLine("property_get", { wall_end_ms: 310 }),
        traceLine("create_scene_node", { ok: false, wall_end_ms: 320 }),
        traceLine("set_theme_color", { wall_end_ms: 330 }),
      ].join("\n"),
    );
    const result = computeCoverage(records, FAKE_CATALOG, 1);
    expect(result.catalog_total).toBe(FAKE_CATALOG.length);
    expect(result.used).toEqual(["create_scene_node", "property_get"]);
    expect(result.unused).toEqual(["get_scene_tree", "ping_probe", "property_set"]);
    expect(result.alias_excluded).toEqual([{ tool: "set_theme_color", count: 1 }]);
    expect(result.coverage_used).toBe(2);
    expect(result.coverage_invalid_touches).toBe(2);
    expect(result.round_start_ms).toBe(300);
    expect(result.by_domain.property.total).toBe(2);
    expect(result.by_domain.property.used).toBe(1);
    expect(result.by_domain.property.unused).toBe(1);
    expect(result.suspected_gaming).toBe(false);
  });

  test("uses the real 253 catalog and flags coverage with no evidence", () => {
    const records = parseTraceJsonl(
      Array.from({ length: 12 }, (_, index) => traceLine("property_get", { wall_end_ms: index })).join(
        "\n",
      ),
    );
    const result = computeCoverage(records, getCanonicalCatalog(), 0);
    expect(result.catalog_total).toBe(253);
    expect(result.coverage_used).toBe(1);
    expect(result.suspected_gaming).toBe(true);
    const many = computeCoverage(
      parseTraceJsonl(
        getCanonicalCatalog()
          .slice(0, 12)
          .map((tool) => traceLine(tool, { wall_end_ms: 1 }))
          .join("\n"),
      ),
      getCanonicalCatalog(),
      0,
    );
    expect(many.coverage_used).toBe(12);
    expect(many.suspected_gaming).toBe(true);
  });

  test("CLI trace source drops plugin lifecycle events and pre-round calls", () => {
    const started = new Date(2026, 10, 23, 5, 32, 2, 73).getTime();
    const records: TraceRecord[] = [
      { source: "cli", kind: "tool_call", tool: "property_get", ok: true, wall_start_ms: started + 100, wall_end_ms: started + 200 },
      { source: "cli", kind: "tool_call", tool: "property_set", ok: false, wall_start_ms: started + 300, wall_end_ms: started + 400 },
      { source: "plugin", kind: "event", tool: "initialize", ok: false, wall_start_ms: started + 50, wall_end_ms: started + 60 },
      { source: "cli", kind: "event", tool: "tools/call", ok: false, wall_start_ms: started + 70, wall_end_ms: started + 80 },
      { source: "cli", kind: "event", tool: "tasks/get", ok: false, wall_start_ms: started + 90, wall_end_ms: started + 95 },
      { source: "cli", kind: "tool_call", tool: "property_get", ok: false, wall_start_ms: started - 86_400_000, wall_end_ms: started - 86_399_900 },
    ];
    const scoped = readCliCoverageRecords(records, started);
    expect(scoped.map((record) => record.tool)).toEqual(["property_get", "property_set"]);
    expect(scoped.every((record) => record.kind === "tool_call")).toBe(true);
    const result = computeCoverage(scoped, FAKE_CATALOG, 1, started);
    expect(result.alias_excluded).toEqual([]);
    expect(result.coverage_invalid_touches).toBe(1);
    expect(result.used).toEqual(["property_get", "property_set"]);
    expect(result.suspected_gaming).toBe(false);
  });

  test("counts evidence rows from a table or markers", () => {
    expect(
      countEvidenceRows(["| 证据 | 路径 |", "| --- | --- |", "| 截图 | img/a.png |", "| trace | trace-x#seq=1 |"].join("\n")),
    ).toBe(2);
    expect(countEvidenceRows(["- 截图：`img/a.png`", "- trace：`trace-x#seq=1`"].join("\n"))).toBe(2);
    expect(countEvidenceRows("")).toBe(0);
  });

  test("scopes coverage to the round start parsed from the directory name", () => {
    const roundStart = new Date(2026, 10, 20, 3, 28, 0, 0).getTime();
    expect(parseRoundStartMs("2026-11-20_03-28")).toBe(roundStart);
    expect(parseRoundStartMs("round")).toBeUndefined();
    expect(parseRoundStartMs("2026-11-20_03-28-2")).toBeUndefined();

    const records = parseTraceJsonl(
      [
        traceLine("property_get", { wall_end_ms: roundStart - 86_400_000 }),
        traceLine("property_set", { ok: false, wall_end_ms: roundStart - 3_600_000 }),
        traceLine("property_get", { wall_end_ms: roundStart + 1_000 }),
        traceLine("create_scene_node", { ok: false, wall_end_ms: roundStart + 2_000 }),
      ].join("\n"),
    );
    const result = computeCoverage(records, FAKE_CATALOG, 1, roundStart);
    expect(result.round_start_ms).toBe(roundStart);
    expect(result.used).toEqual(["create_scene_node", "property_get"]);
    expect(result.coverage_used).toBe(2);
    expect(result.coverage_invalid_touches).toBe(1);
    expect(result.used).not.toContain("property_set");
  });
});

describe("IF-02 coverage command", () => {
  test("writes coverage.json and snapshot and prints catalog_total=253", () => {
    const round = path.join(tmp, "round");
    fs.mkdirSync(round, { recursive: true });
    fs.writeFileSync(
      path.join(round, "evidence.md"),
      "| 证据 | 路径 |\n| --- | --- |\n| 截图 | img/a.png |\n",
      "utf8",
    );
    const runner: CaptureRunner = () => ({
      exitCode: 0,
      stdout: `${traceLine("property_get", { wall_end_ms: 10 })}\n`,
    });
    const capture = captureStdout();
    try {
      expect(runHarnessCoverage({ round, trace: null, json: false }, { runner })).toBe(0);
    } finally {
      capture.restore();
    }
    expect(capture.read()).toContain("catalog_total=253");
    const coverage = JSON.parse(fs.readFileSync(path.join(round, "coverage.json"), "utf8")) as {
      catalog_total: number;
      used: string[];
      unused: string[];
      by_domain: Record<string, unknown>;
      alias_excluded: unknown[];
      round_start_ms: number;
      trace_snapshot: string;
      coverage_used: number;
      coverage_invalid_touches: number;
      evidence_rows: number;
      suspected_gaming: boolean;
    };
    expect(coverage.catalog_total).toBe(253);
    expect(coverage.used).toEqual(["property_get"]);
    expect(coverage.unused).toHaveLength(252);
    expect(coverage.by_domain).toBeTruthy();
    expect(coverage.alias_excluded).toEqual([]);
    expect(coverage.round_start_ms).toBe(10);
    expect(coverage.trace_snapshot).toBe("trace-snapshot.jsonl");
    expect(coverage.coverage_used).toBe(1);
    expect(coverage.coverage_invalid_touches).toBe(0);
    expect(coverage.evidence_rows).toBe(1);
    expect(coverage.suspected_gaming).toBe(false);
    expect(fs.existsSync(path.join(round, "trace-snapshot.jsonl"))).toBe(true);
  });

  test("uses the CLI trace source by default and snapshots only scoped tool calls", () => {
    const round = path.join(tmp, "2026-11-23_13-30");
    fs.mkdirSync(round, { recursive: true });
    fs.writeFileSync(
      path.join(round, "evidence.md"),
      "| 证据 | 路径 |\n| --- | --- |\n| 截图 | img/a.png |\n",
      "utf8",
    );
    const started = new Date(2026, 10, 23, 13, 30, 0, 0).getTime();
    const cliRecords: TraceRecord[] = [
      { source: "cli", kind: "tool_call", tool: "property_get", ok: true, wall_start_ms: started + 100, wall_end_ms: started + 200 },
      { source: "plugin", kind: "event", tool: "initialize", ok: false, wall_start_ms: started + 50, wall_end_ms: started + 60 },
      { source: "cli", kind: "tool_call", tool: "property_set", ok: true, wall_start_ms: started + 300, wall_end_ms: started + 400 },
    ];
    const capture = captureStdout();
    try {
      expect(runHarnessCoverage({ round, trace: null, json: false }, { cliRecords })).toBe(0);
    } finally {
      capture.restore();
    }
    const coverage = JSON.parse(fs.readFileSync(path.join(round, "coverage.json"), "utf8")) as {
      coverage_used: number;
      coverage_invalid_touches: number;
      alias_excluded: unknown[];
      evidence_rows: number;
      suspected_gaming: boolean;
      trace_snapshot: string;
    };
    expect(coverage.coverage_used).toBe(2);
    expect(coverage.coverage_invalid_touches).toBe(0);
    expect(coverage.alias_excluded).toEqual([]);
    expect(coverage.evidence_rows).toBe(1);
    expect(coverage.suspected_gaming).toBe(false);
    expect(coverage.trace_snapshot).toBe("trace-snapshot.jsonl");
    const snapshot = fs.readFileSync(path.join(round, "trace-snapshot.jsonl"), "utf8");
    expect(snapshot).not.toContain("initialize");
    expect(snapshot.split(/\r?\n/).filter((line) => line.trim() !== "")).toHaveLength(2);
  });

  test("reads an exported trace snapshot via --trace", () => {
    const round = path.join(tmp, "round-trace");
    const snapshot = path.join(tmp, "trace.jsonl");
    fs.writeFileSync(snapshot, `${traceLine("property_set", { wall_end_ms: 5 })}\n`, "utf8");
    const capture = captureStdout();
    try {
      expect(runHarnessCoverage({ round, trace: snapshot, json: true }, { runner: () => ({ exitCode: 9, stdout: "" }) })).toBe(0);
    } finally {
      capture.restore();
    }
    const coverage = JSON.parse(fs.readFileSync(path.join(round, "coverage.json"), "utf8")) as {
      used: string[];
    };
    expect(coverage.used).toEqual(["property_set"]);
    expect(capture.read()).toContain('"catalog_total": 253');
  });

  test("writes coverage scoped to the round start for a named round directory", () => {
    const round = path.join(tmp, "2026-11-20_03-28");
    const roundStart = new Date(2026, 10, 20, 3, 28, 0, 0).getTime();
    fs.mkdirSync(round, { recursive: true });
    fs.writeFileSync(
      path.join(round, "evidence.md"),
      "| 证据 | 路径 |\n| --- | --- |\n| 截图 | img/a.png |\n",
      "utf8",
    );
    const runner: CaptureRunner = () => ({
      exitCode: 0,
      stdout: [
        traceLine("property_get", { wall_end_ms: roundStart - 86_400_000 }),
        traceLine("property_get", { wall_end_ms: roundStart + 1_000 }),
      ].join("\n"),
    });
    const capture = captureStdout();
    try {
      expect(runHarnessCoverage({ round, trace: null, json: false }, { runner })).toBe(0);
    } finally {
      capture.restore();
    }
    const coverage = JSON.parse(fs.readFileSync(path.join(round, "coverage.json"), "utf8")) as {
      round_start_ms: number;
      coverage_used: number;
      coverage_invalid_touches: number;
    };
    expect(coverage.round_start_ms).toBe(roundStart);
    expect(coverage.coverage_used).toBe(1);
    expect(coverage.coverage_invalid_touches).toBe(0);
  });

  test("returns a non-zero code when trace export fails", () => {
    const capture = captureStderr();
    try {
      expect(
        runHarnessCoverage(
          { round: path.join(tmp, "no-op"), trace: null, json: false },
          { runner: () => ({ exitCode: 4, stdout: "" }) },
        ),
      ).toBe(4);
    } finally {
      capture.restore();
    }
    expect(fs.existsSync(path.join(tmp, "no-op"))).toBe(false);
  });
});

const REPORT = [
  "# 端到端测试报告",
  "- 游玩时长：7 分钟",
  "- 关卡数：3",
  "",
  "## 1. 结论摘要",
  "## 3. 玩法闭环验证表",
  "## 6. 问题记录",
  "### Q1 [失败] 无法保存",
  "### Q2 [通过] 输入闭环",
  "## 7. 优化建议",
  "### S1 提升读回稳定性",
  "### S2 缩短等待",
  "",
  "## 8. 递归闭环产出",
  "| 编号 | 适用条件 | 教训 |",
  "| --- | --- | --- |",
  "| E1 | 条件甲 | 触发动作→满足条件→产出后果 |",
].join("\n");

const EVIDENCE = [
  "# 证据",
  "| 证据 | 路径 |",
  "| --- | --- |",
  "| 截图 | img/a.png |",
  "| trace | trace-x#seq=1 |",
  "| 状态读回 | get_scene_tree |",
].join("\n");

const COVERAGE = JSON.stringify({
  catalog_total: 253,
  used: ["property_get", "get_scene_tree"],
  unused: ["set_theme_color"],
  coverage_used: 2,
  coverage_invalid_touches: 1,
});

describe("digest parsing and computation", () => {
  test("extracts Q/S ids, levels, playtime and causal triples", () => {
    expect(extractIds(REPORT, "Q")).toEqual(["Q1", "Q2"]);
    expect(extractIds(REPORT, "S")).toEqual(["S1", "S2"]);
    expect(extractLevels(REPORT)).toEqual(["关卡1", "关卡2", "关卡3"]);
    expect(extractLevels("## 关卡\n- 第一关\n- 第二关")).toEqual(["第一关", "第二关"]);
    expect(extractPlaytimeMinutes(REPORT)).toBe(7);
    expect(extractCausalTriples(REPORT)).toEqual([
      { action: "触发动作", condition: "满足条件", consequence: "产出后果" },
    ]);
  });

  test("builds a digest with all required fields", () => {
    const digest = buildDigest("round-1", REPORT, EVIDENCE, COVERAGE);
    expect(digest.round).toBe("round-1");
    expect(digest.q_items).toEqual(["Q1", "Q2"]);
    expect(digest.s_items).toEqual(["S1", "S2"]);
    expect(digest.levels).toEqual(["关卡1", "关卡2", "关卡3"]);
    expect(digest.playtime_minutes).toBe(7);
    expect(digest.causal_triples).toHaveLength(1);
    expect(digest.coverage_gaps).toEqual(["set_theme_color"]);
    expect(digest.coverage_used).toBe(2);
    expect(digest.coverage_invalid_touches).toBe(1);
    expect(digest.evidence_rows).toBe(3);
    expect(digest.suspected_gaming).toBe(false);
  });

  test("recognizes the functional-unit count and full-playthrough wording used in report section 3", () => {
    const section = [
      "# 端到端测试报告",
      "- 功能单元数：7；机制清单：配置持久化、场景骨架、资源导出导入引用",
      "## 3. 功能/玩法闭环验证表（硬验收）+ 单元机制与净可玩内容表",
      "| 单元 | 主题/目标 | 净可玩内容 |",
      "| 1 | 项目与输入 | 设置 6 动作与窗口，读回一致（约 1 分钟） |",
      "| 7 | RID 服务器 | AStar 16 点 + RID 创建/释放（约 2 分钟） |",
      "功能单元数=7；机制数=7；单元间有机制差异=是。",
      "净可玩内容检验：最低为单元 7；完整通关流程约 6 分钟。无纯等待计时器凑时长。",
    ].join("\n");
    expect(extractLevels(section)).toEqual([
      "关卡1", "关卡2", "关卡3", "关卡4", "关卡5", "关卡6", "关卡7",
    ]);
    expect(extractPlaytimeMinutes(section)).toBe(6);
    const digest = buildDigest("round-section-3", section, EVIDENCE, COVERAGE);
    expect(digest.levels).toHaveLength(7);
    expect(digest.playtime_minutes).toBe(6);
  });

  test("throws on empty or unparseable inputs", () => {
    expect(() => buildDigest("r", "", EVIDENCE, COVERAGE)).toThrow(/report.md/);
    expect(() => buildDigest("r", "gibberish", EVIDENCE, COVERAGE)).toThrow(/recognizable/);
    expect(() => buildDigest("r", REPORT, "", COVERAGE)).toThrow(/evidence.md/);
    expect(() => buildDigest("r", REPORT, EVIDENCE, "{")).toThrow(/valid JSON/);
    expect(() => buildDigest("r", REPORT, EVIDENCE, "{}")).toThrow(/catalog_total/);
  });
});

describe("IF-03 digest command", () => {
  test("writes digest.json into the round directory", () => {
    const round = path.join(tmp, "digest-round");
    fs.mkdirSync(round, { recursive: true });
    fs.writeFileSync(path.join(round, "report.md"), REPORT, "utf8");
    fs.writeFileSync(path.join(round, "evidence.md"), EVIDENCE, "utf8");
    fs.writeFileSync(path.join(round, "coverage.json"), COVERAGE, "utf8");
    const capture = captureStdout();
    try {
      expect(runHarnessDigest({ round, json: true })).toBe(0);
    } finally {
      capture.restore();
    }
    const digest = JSON.parse(fs.readFileSync(path.join(round, "digest.json"), "utf8")) as {
      round: string;
      q_items: string[];
      s_items: string[];
      levels: string[];
      causal_triples: unknown[];
      coverage_gaps: string[];
      coverage_used: number;
      evidence_rows: number;
    };
    expect(digest.round).toBe("digest-round");
    expect(digest.q_items).toEqual(["Q1", "Q2"]);
    expect(digest.s_items).toEqual(["S1", "S2"]);
    expect(digest.levels).toHaveLength(3);
    expect(digest.causal_triples).toHaveLength(1);
    expect(digest.coverage_gaps).toEqual(["set_theme_color"]);
    expect(digest.coverage_used).toBe(2);
    expect(digest.evidence_rows).toBe(3);
    expect(fs.readFileSync(path.join(round, "report.md"), "utf8")).toBe(REPORT);
    expect(capture.read()).toContain('"q_items"');
  });

  test("reconciles coverage evidence fields with the complete evidence used by digest", () => {
    const round = path.join(tmp, "digest-reconcile");
    fs.mkdirSync(round, { recursive: true });
    fs.writeFileSync(path.join(round, "report.md"), REPORT, "utf8");
    fs.writeFileSync(path.join(round, "evidence.md"), EVIDENCE, "utf8");
    fs.writeFileSync(
      path.join(round, "coverage.json"),
      JSON.stringify({ ...JSON.parse(COVERAGE), evidence_rows: 0, suspected_gaming: true }),
      "utf8",
    );
    const capture = captureStdout();
    try {
      expect(runHarnessDigest({ round, json: false })).toBe(0);
    } finally {
      capture.restore();
    }
    const digest = JSON.parse(fs.readFileSync(path.join(round, "digest.json"), "utf8")) as {
      evidence_rows: number;
      suspected_gaming: boolean;
    };
    const coverage = JSON.parse(fs.readFileSync(path.join(round, "coverage.json"), "utf8")) as {
      evidence_rows: number;
      suspected_gaming: boolean;
    };
    expect(digest.evidence_rows).toBe(3);
    expect(digest.suspected_gaming).toBe(false);
    expect(coverage.evidence_rows).toBe(digest.evidence_rows);
    expect(coverage.suspected_gaming).toBe(digest.suspected_gaming);
  });

  test("reports a missing input as error exit code 1", () => {
    const capture = captureStderr();
    try {
      expect(runHarnessDigest({ round: path.join(tmp, "missing"), json: false })).toBe(1);
    } finally {
      capture.restore();
    }
    expect(capture.read()).toContain("missing digest input");
  });
});

describe("main.ts harness registration", () => {
  test("parses harness arguments strictly", () => {
    expect(parseHarnessSetup(["--project", "/p", "--headless"])).toEqual({
      project: "/p",
      headless: true,
    });
    expect(parseHarnessSetup(["--project", "/p"])).toEqual({ project: "/p", headless: false });
    expect(() => parseHarnessSetup([])).toThrow(/--project/);
    expect(() => parseHarnessSetup(["--project"])).toThrow(/expected one argument/);
    expect(() => parseHarnessSetup(["--project", "/p", "--bogus"])).toThrow(/unrecognized/);
    expect(parseHarnessCoverage(["--round", "/r", "--trace", "/t", "--json"])).toEqual({
      round: "/r",
      trace: "/t",
      json: true,
    });
    expect(() => parseHarnessCoverage(["--round"])).toThrow(/expected one argument/);
    expect(parseHarnessDigest(["--round", "/r"])).toEqual({ round: "/r", json: false });
    expect(() => parseHarnessDigest(["--round", "/r", "--json", "--trace", "/t"])).toThrow(/unrecognized/);
  });

  test("returns usage exit code 2 for invalid harness invocations", async () => {
    const capture = captureStderr();
    try {
      expect(await main(["harness"])).toBe(2);
      expect(await main(["harness", "setup"])).toBe(2);
      expect(await main(["harness", "coverage", "--round"])).toBe(2);
      expect(await main(["harness", "digest", "--round", "/r", "--bogus"])).toBe(2);
      expect(await main(["harness", "unknown"])).toBe(2);
    } finally {
      capture.restore();
    }
  });
});
