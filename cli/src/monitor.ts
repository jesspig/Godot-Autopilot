import fs from "node:fs";

export interface MonitorDef {
  i: number;
  n: string;
  t: string;
  cat?: string;
}

export interface MonitorFrame {
  n: number;
  d: number;
  t: number;
  v: number[];
  c: number[];
}

export interface MonitorSession {
  sessionId: string;
  builtin: MonitorDef[];
  custom: MonitorDef[];
  frames: MonitorFrame[];
}

interface SeriesStat {
  name: string;
  type: string;
  samples: number;
  min: number;
  max: number;
  avg: number;
  p50: number;
  p95: number;
  p99: number;
  first: number;
  last: number;
  delta: number;
}

export interface MonitorReport {
  session_id: string;
  frames: number;
  duration_ms: number;
  frame: {
    avg_ms: number;
    p95_ms: number;
    p99_ms: number;
    max_ms: number;
    slow_frames: number;
  };
  series: SeriesStat[];
}

function asRecord(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value)
    ? (value as Record<string, unknown>)
    : {};
}

function num(value: unknown, fallback = 0): number {
  return typeof value === "number" && Number.isFinite(value) ? value : fallback;
}

function definitions(value: unknown): MonitorDef[] {
  if (!Array.isArray(value)) return [];
  return value.map((item) => {
    const record = asRecord(item);
    return {
      i: Math.trunc(num(record.i)),
      n: typeof record.n === "string" ? record.n : "",
      t: typeof record.t === "string" ? record.t : "",
      cat: typeof record.cat === "string" ? record.cat : undefined,
    };
  }).filter((item) => item.n !== "");
}

function numbers(value: unknown): number[] {
  if (!Array.isArray(value)) return [];
  return value.map((item) => (typeof item === "number" && Number.isFinite(item) ? item : Number.NaN));
}

function percentile(sorted: number[], fraction: number): number {
  if (sorted.length === 0) return 0;
  const index = Math.min(sorted.length - 1, Math.max(0, Math.ceil(fraction * sorted.length) - 1));
  return sorted[index];
}

function stat(name: string, type: string, source: number[]): SeriesStat {
  const values = source.filter((value) => Number.isFinite(value)).sort((a, b) => a - b);
  if (values.length === 0) {
    return { name, type, samples: 0, min: 0, max: 0, avg: 0, p50: 0, p95: 0, p99: 0, first: 0, last: 0, delta: 0 };
  }
  const sum = values.reduce((acc, value) => acc + value, 0);
  const first = source.find((value) => Number.isFinite(value)) ?? 0;
  const finiteReverse = [...source].reverse().find((value) => Number.isFinite(value)) ?? 0;
  return {
    name,
    type,
    samples: values.length,
    min: values[0],
    max: values[values.length - 1],
    avg: sum / values.length,
    p50: percentile(values, 0.5),
    p95: percentile(values, 0.95),
    p99: percentile(values, 0.99),
    first,
    last: finiteReverse,
    delta: finiteReverse - first,
  };
}

export function parseMonitorText(text: string): MonitorSession {
  const session: MonitorSession = { sessionId: "", builtin: [], custom: [], frames: [] };
  for (const raw of text.split(/\r?\n/)) {
    if (raw.trim() === "") continue;
    let record: Record<string, unknown>;
    try {
      record = asRecord(JSON.parse(raw));
    } catch {
      continue;
    }
    const type = record.type;
    if (type === "session") {
      session.sessionId = typeof record.id === "string" ? record.id : "";
      session.builtin = definitions(record.builtin_monitors);
      session.custom = definitions(record.custom_monitors);
    } else if (type === "custom_change") {
      session.custom = definitions(record.custom_monitors);
    } else if (type === "frame") {
      session.frames.push({
        n: Math.trunc(num(record.n)),
        d: num(record.d),
        t: num(record.t),
        v: numbers(record.v),
        c: numbers(record.c),
      });
    }
  }
  return session;
}

export function analyzeMonitorSession(session: MonitorSession, slowMs: number): MonitorReport {
  const deltas = session.frames.map((frame) => frame.d).filter((value) => Number.isFinite(value));
  const frame = stat("frame/delta_ms", "time", deltas);
  const series: SeriesStat[] = [];
  for (let i = 0; i < session.builtin.length; i++) {
    series.push(stat(session.builtin[i].n, session.builtin[i].t, session.frames.map((f) => f.v[i] ?? 0)));
  }
  for (let i = 0; i < session.custom.length; i++) {
    series.push(stat(session.custom[i].n, session.custom[i].t, session.frames.map((f) => f.c[i] ?? 0)));
  }
  const duration = session.frames.length > 0
    ? session.frames[session.frames.length - 1].t - session.frames[0].t
    : 0;
  return {
    session_id: session.sessionId,
    frames: session.frames.length,
    duration_ms: Math.max(0, duration),
    frame: {
      avg_ms: frame.avg,
      p95_ms: frame.p95,
      p99_ms: frame.p99,
      max_ms: frame.max,
      slow_frames: session.frames.filter((f) => f.d > slowMs).length,
    },
    series,
  };
}

export function analyzeMonitorFile(filePath: string, slowMs: number): MonitorReport {
  const text = fs.readFileSync(filePath, "utf8");
  return analyzeMonitorSession(parseMonitorText(text), slowMs);
}
